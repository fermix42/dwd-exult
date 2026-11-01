"""Real-renderer chart captures and native hit-region checks; grants chart only.

Uses isolated 320x200 logical game/UI settings at 1x or 2x. No Orb or plot fixture.
"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'test_cli'))
from driver import Driver, BASE

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--run', required=True)
p.add_argument('--mod', default='')
p.add_argument('--scale', type=int, choices=(1,2), default=1)
a = p.parse_args()
d = Driver()
records = []

def do(cmd, **kw):
    command = dict(cmd=cmd, **kw)
    response = d.execute(command)
    records.append(dict(command=command, response=response))
    return response

def check(label, value):
    d.assertions += 1
    d.failures += not value
    records.append(dict(check=label, passed=bool(value)))
    print(label, bool(value), flush=True)
    if not value: raise AssertionError(label)

def settle(owner):
    for _ in range(200):
        s = do('state')['state']
        if s['input_owner'] == owner: return s
        time.sleep(.01)
    raise TimeoutError(owner)

# Read font widths from the authoritative BG font, without transcribing glyphs.
b = (Path(os.environ['EXULT_TEST_BG_PATH'])/'static/fonts.vga').read_bytes()
o, n = struct.unpack_from('<II', b, 128+8*2)
font = b[o:o+n]
def width(text):
    total = 0
    for c in text:
        offset = struct.unpack_from('<I',font,4+4*ord(c))[0]
        right,left = struct.unpack_from('<hh',font,offset)
        total += right+left+1
    return total

original_popen = subprocess.Popen
def launch(args, **kw):
    cfg = Path(args[args.index('-c')+1])
    assert BASE/'runs' in cfg.resolve().parents
    tree = ET.parse(cfg)
    video = tree.getroot().find('video')
    values = {'display/width':320*a.scale,'display/height':200*a.scale,
              'game/width':320,'game/height':200,'ui/width':320,'ui/height':200,
              'ui/scale_method':'Point','ui/fill_mode':'Fit','ui/fill_scaler':'Point',
              'fill_mode':'Fit','fill_scaler':'Point','scale':a.scale,'scale_method':'Point'}
    for path,value in values.items():
        e = video
        for part in path.split('/'):
            child = e.find(part)
            if child is None: child = ET.SubElement(e,part)
            e = child
        e.text = str(value)
    tree.write(cfg,encoding='unicode')
    return original_popen(args, **kw)

dx = [0,1,1,1,0,-1,-1,-1]
dy = [-1,-1,0,1,1,1,0,-1]
directions = ['north','northeast','east','southeast','south','southwest','west','northwest']
retail = ['Britain','Jhelom','Yew','Minoc','Trinsic','New Magincia','Skara Brae','Moonglow']
towns = ['Britain','Jhelom','Yew','Minoc','Trinsic','Skara Brae','New Magincia','Moonglow']
shrines = ['Compassion','Valor','Justice','Sacrifice','Honor','Spirituality','Humility','Honesty']
stamps = [(160,94),(233,94),(207,111),(233,128),(160,130),(87,128),(133,111),(87,94)]
crops = [(6,8),(9,8),(5,9),(6,9),(6,8),(7,6),(7,8),(8,9)]
runes = [5,4,1,0,6,3,2,7] if a.mod else [5,4,1,0,6,2,3,7]
marks = []
for i,direction in enumerate(directions):
    if a.mod:
        x,y = 160+73*dx[i],111+28*dy[i]
        sx,sy = stamps[i]
        w,h = crops[runes[i]]
        marks.append(('near-'+direction, (x-width(towns[i])//2,y-4,width(towns[i]),8), (sx-w//2,sy-h//2,w,h)))
        x = 30 if i==6 else 290 if i==2 else 160+116*dx[i]
        y = 54 if dy[i]<0 else 155 if dy[i]>0 else 111
        caption = 42 if i==6 else 278 if i==2 else x
        marks.append(('far-'+direction,(caption-width(shrines[i])//2,y+12,width(shrines[i]),8),(x-w,y-h,w*2,h*2)))
    else:
        x,y = 160+105*dx[i],102+45*dy[i]
        w,h = crops[runes[i]]
        marks.append(('near-'+direction,(x-width(retail[i])//2,y+18,width(retail[i]),8),(x-w*3//2,y-h*3//2,w*3,h*3)))

try:
    BASE.mkdir(parents=True,exist_ok=True)
    keys = BASE/(a.run+'.keys')
    keys.write_text('w ORB_CHART\n')
    subprocess.Popen = launch
    do('init',game='bg',mod=a.mod,run=a.run,keys_path=str(keys))
    subprocess.Popen = original_popen
    do('test_orb_chart_item')
    origin = do('state')['state']
    do('key',key='w')
    settle('modal')
    check('all native destination controls',len([c for c in do('controls')['controls'] if c['action'].startswith(('near-','far-'))])==len(marks))
    do('screenshot',full=True)
    check('native/scaled renderer dimensions',struct.unpack_from('>II',(d.run/'screenshot.png').read_bytes(),16)==(320*a.scale,200*a.scale))
    # Every label/rune rectangle, including transparent spaces, must belong to
    # its intended native button. A two-pixel halo tests small-mark usability.
    for action,label,rune in marks:
        for kind,rect in [('name',label),('rune',rune)]:
            x,y,w,h = rect
            points = [(x+w//2,y+h//2),(x,y),(x+w-1,y),(x,y+h-1),(x+w-1,y+h-1)]
            if a.mod and action.startswith('near-'):
                points += [(x-2,y+h//2),(x+w+1,y+h//2),(x+w//2,y-2),(x+w//2,y+h+1)]
            observed = [(px,py,do('control_at',x=px,y=py)['action']) for px,py in points]
            check(action+' '+kind+' native region',all(v==action for _,_,v in observed))
    cx,cy = (160,111) if a.mod else (160,102)
    check('entire compass is cancellation',all(do('control_at',x=cx+xx,y=cy+yy)['action']=='cancel' for xx in range(-12,13) for yy in range(-12,13)))
    do('key',key='escape')
    settle('gameplay')
    # Use actual SDL/native input at the rune and caption, rather than calling
    # chart selection directly. With no Orb, the chart must only close.
    for action,label,rune in marks:
        for kind,(x,y,w,h) in [('name',label),('rune',rune)]:
            do('key',key='w')
            settle('modal')
            do('click',x=x+w//2,y=y+h//2)
            s = settle('gameplay')
            check(action+' '+kind+' click',s['queue_pause_depth']==0 and not s['target_intercept_pending'] and (s['x'],s['y'],s['map'])==(origin['x'],origin['y'],origin['map']))
    do('exit')
except Exception as error:
    if not d.failures: d.failures += 1
    records.append(dict(error=str(error)))
    raise
finally:
    subprocess.Popen = original_popen
    if d.p and d.p.poll() is None:
        try: do('key',key='escape');time.sleep(.05)
        except Exception: pass
    result = d.close()
    if d.run: (d.run/'presentation-checks.json').write_text(json.dumps(dict(records=records,result=result),indent=2))
    print(json.dumps(result),flush=True)
if result['result'] != 'PASS': raise SystemExit(1)
