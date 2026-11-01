"""Unit checks of pure destination helpers fetched from the running engine.

A bounded bytecode evaluator supplies local object/flag inputs. It never writes
engine flags or creates/travels through gates. This is not live quest evidence.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/test_cli'))
from driver import Driver

class TargetReached(Exception):
    pass

class Helper:
    def __init__(self, functions, repaired):
        self.functions = functions
        self.repaired = repaired
        self.intrinsics = re.findall(r'USECODE_INTRINSIC_PTR\((\w+)\)',(ROOT/'usecode/bgintrinsics.h').read_text())

    def run(self, number, args, context=None):
        assert number in (0x823,0x826) or (number==0x311 and context is not None), 'only pure helpers or pre-target guards are allowed'
        body = self.functions[number]
        offset = 2+int.from_bytes(body[:2],'little')
        argc, count, external_count = [int.from_bytes(body[i:i+2],'little') for i in range(offset,offset+6,2)]
        assert argc == len(args)
        externs = [int.from_bytes(body[i:i+2],'little') for i in range(offset+6,offset+6+external_count*2,2)]
        code = body[offset+6+external_count*2:]
        local = copy.deepcopy(args)+[0]*count
        stack = []
        ip = 0
        def read(n=2,signed=False):
            nonlocal ip
            v = int.from_bytes(code[ip:ip+n],'little',signed=signed)
            ip += n
            return v
        pop,push = stack.pop,stack.append
        for _ in range(2000):
            at = ip
            op = read(1)
            if op in (0x05,0x06):
                delta = read(signed=True)
                if op==0x06 or not pop(): ip += delta
            elif op in (0x09,0x0e,0x0f,0x16,0x17,0x18,0x19,0x1a,0x22):
                b,a = pop(),pop()
                push({0x09:lambda:a+b,0x0e:lambda:int(bool(a) and bool(b)),0x0f:lambda:int(bool(a) or bool(b)),
                      0x16:lambda:int(a>b),0x17:lambda:int(a<b),0x18:lambda:int(a>=b),0x19:lambda:int(a<=b),
                      0x1a:lambda:int(a!=b),0x22:lambda:int(a==b)}[op]())
            elif op==0x10: push(int(not pop()))
            elif op==0x12: local[read()] = copy.deepcopy(pop())
            elif op in (0x13,0x14): push(int(op==0x13))
            elif op==0x1d:
                start=read()
                strings=body[2:offset]
                push(strings[start:strings.index(0,start)].decode('latin1'))
            elif op==0x1e: push([pop() for _ in range(read())])
            elif op==0x1f: push(read(signed=True))
            elif op==0x21: push(copy.deepcopy(local[read()]))
            elif op==0x24:
                number = externs[read()]
                if number==0x826:push(self.run(number,[pop()]))
                elif context is not None and number==0x93e:push(int(context['inside_trinsic']))
                elif context is not None and number==0x8fe:pop() # Bark helper: no world side effect in this unit check.
                else:raise AssertionError(('unexpected pre-target external',hex(number)))
            elif op==0x26:
                index = pop()-1
                array = local[read()]
                push(copy.deepcopy(array[index]) if isinstance(array,list) and 0<=index<len(array) else array if index==0 and not isinstance(array,list) else 0)
            elif op in (0x25,0x2c,0x32): return 0
            elif op==0x2d: return pop()
            elif op==0x30:
                array,value = pop(),pop()
                push(int(value in (array if isinstance(array,list) else [array])))
            elif op in (0x38,0x39):
                name = self.intrinsics[read()]
                arguments = [pop() for _ in range(read(1))]
                if context is not None and name=='click_on_item':raise TargetReached()
                if context is not None and name=='close_gumps':continue
                assert name in ('get_item_quality','get_item_shape'), (name,arguments)
                value = arguments[0]['quality' if name=='get_item_quality' else 'shape']
                if op==0x38: push(value)
            elif op==0x42:
                flag = read()
                assert flag==308 or (context is not None and flag in (4,87))
                push(int(self.repaired) if flag==308 else int(context['broken'] if flag==4 else context['left_trinsic']))
            elif op==0x44: push(read(1))
            elif op==0x48:push(1) # Ordinary double-click event; stop before native targeting.
            elif op==0x46:
                index = read()
                element,value = pop()-1,pop()
                if not isinstance(local[index],list): local[index] = [local[index]]
                local[index] += [0]*max(0,element+1-len(local[index]))
                local[index][element] = copy.deepcopy(value)
            else: raise AssertionError(('unsupported pure-helper opcode',hex(op),at))
        raise AssertionError('helper exceeded instruction bound')

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run',required=True)
p.add_argument('--mod',default='')
a = p.parse_args()
d = Driver()
records = []
def check(label,value):
    d.assertions += 1
    d.failures += not value
    records.append(dict(check=label,passed=bool(value)))
    print(label,bool(value),flush=True)
    if not value: raise AssertionError(label)
try:
    init = d.execute(dict(cmd='init',game='bg',mod=a.mod,run=a.run))
    records.append(dict(init=init))
    before = d.execute(dict(cmd='state'))['state']
    check('effective supported profile',before['orb_chart_layout']==(2 if a.mod else 1))
    bodies = {n:bytes.fromhex(d.execute(dict(cmd='inspect_usecode',function=n))['bytes']) for n in (0x311,0x823,0x826,0x824,0x825,0x93e)}
    records.append(dict(loaded_functions={hex(n):dict(size=len(b),sha256=hashlib.sha256(b).hexdigest(),bytes=b.hex()) for n,b in bodies.items()}))
    retail = [[1231,1314,0],[261,2826,0],[519,491,0],[1855,402,0],[1023,2435,1],[2175,2338,0],[567,1609,0],[2879,1810,0]]
    towns = [retail[i]+[0] for i in (0,1,2,3,4,6,5,7)]
    towns[7][2] = 2
    shrines = [[1464,1356,0,0],[507,2672,0,0],[750,140,0,0],[2289,966,0,0],[867,2526,0,0],[2919,2403,1,1],[2850,2502,0,0],[2713,1164,0,0]]
    for repaired in (False,True):
        vm = Helper(bodies,repaired)
        expected = copy.deepcopy(towns+shrines if a.mod else retail)
        if a.mod and not repaired: expected[13] = towns[5]
        if not a.mod and repaired: expected[7] = [2919,2402,2]
        for quality,position in enumerate(expected):
            actual = vm.run(0x823,[dict(shape=779,quality=quality)])
            records.append(dict(repaired=repaired,quality=quality,actual=actual,expected=position))
            check(('repaired ' if repaired else 'unrepaired ')+'destination '+str(quality),actual==position)
        for shape in (779,157,1113,1114,777):
            check('gate classification '+str(shape),bool(vm.run(0x826,[dict(shape=shape,quality=0)]))==(shape in ((779,157,1113,1114) if a.mod else (779,157))))
    # Evaluate only the actual caller's pre-target guard instructions. Region
    # membership is a local boundary input; no gate or travel is emulated.
    for broken,left,inside,allowed in [(True,True,False,False),(True,False,True,False),(False,False,True,False),(False,True,True,True),(False,False,False,True)]:
        reached=False
        try:Helper(bodies,False).run(0x311,[0],dict(broken=broken,left_trinsic=left,inside_trinsic=inside))
        except TargetReached:reached=True
        check('pre-target plot guard '+str((broken,left,inside)),reached==allowed)
    after = d.execute(dict(cmd='state'))['state']
    check('unit evaluation leaves real story and inventory unchanged',all(before[k]==after[k] for k in ('orb_given','orb_left_trinsic','orb_sphere_broken','orb_timelord_repaired','inventory')))
    d.execute(dict(cmd='exit'))
except Exception as error:
    if not d.failures: d.failures += 1
    records.append(dict(error=str(error)))
    raise
finally:
    result = d.close()
    if d.run: (d.run/'usecode-unit-checks.json').write_text(json.dumps(dict(records=records,result=result),indent=2))
    print(json.dumps(result),flush=True)
if result['result']!='PASS': raise SystemExit(1)
