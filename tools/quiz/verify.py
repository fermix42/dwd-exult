"""Differential checks of the actual original/replacement quiz bytecode.

This bounded test interpreter supports only instructions used by these helpers.
It is not a game engine and does not certify native UI/encounter integration.
"""
import argparse
import copy
import difflib
import hashlib
import itertools
import json
from pathlib import Path
import random
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ANSWERS = {0x886: [120, 60, 120, 0, 30, 60, 60, 60],
           0x852: [40, 31, 2, 6, 6, 1],
           0x98c: [8, 4, 52, 11, 6, 40, 6, 1, 36, 4, 3, 6, 3, 4, 27]}
QUESTION_TEXTS = {}


def functions(data):
    pos = 0

    def read(n):
        nonlocal pos
        v = int.from_bytes(data[pos:pos+n], 'little')
        pos += n
        return v

    def scope():
        nonlocal pos
        count = read(4)
        read(4)
        for _ in range(count):
            pos = data.index(0, pos) + 1
            kind = read(2)
            read(4)
            if kind == 4:
                scope()
                methods = read(2)
                pos += 2 * methods
                read(2)
            elif kind == 6:
                read(4)

    if data[:8] == bytes.fromhex('ffffffff59534355'):
        pos = 8
        scope()
    result = {}
    while pos < len(data):
        number = read(2)
        extended = number in (65534, 65535)
        if number == 65534:
            number, size = read(4), read(4)
        elif number == 65535:
            number, size = read(2), read(4)
        else:
            size = read(2)
        body = data[pos:pos+size]
        assert len(body) == size and number not in result
        result[number] = (body, extended)
        pos += size
    assert pos == len(data)
    return result


def embedded(header, game):
    text = Path(header).read_text().split('quiz_' + game + '[] = {')[1].split('}')[0]
    return functions(bytes(map(int, re.findall(r'\d+', text))))


def instructions(body):
    """Decode UCC helper instructions, retaining branch destinations separately."""
    start = 2 + int.from_bytes(body[:2], 'little')
    assert int.from_bytes(body[start+4:start+6], 'little') == 0
    code = body[start+6:]
    decoded, targets, offsets = [], {}, {}
    position = 0
    two = {0x12, 0x1c, 0x1d, 0x1e, 0x1f, 0x21, 0x26, 0x2f, 0x42, 0x43, 0x46}
    zero = {0x09, 0x0e, 0x0f, 0x10, 0x13, 0x14, 0x16, 0x17, 0x18, 0x19,
            0x1a, 0x22, 0x25, 0x2c, 0x2d, 0x2e, 0x30, 0x32, 0x33, 0x3e}
    while position < len(code):
        offsets[position] = len(decoded)
        op = code[position]
        position += 1
        token = (op,)
        if op in two:
            token += (int.from_bytes(code[position:position+2], 'little'),)
            position += 2
        elif op in (0x05, 0x06):
            delta = int.from_bytes(code[position:position+2], 'little', signed=True)
            position += 2
            targets[len(decoded)] = position + delta
        elif op == 0x02:
            token += tuple(int.from_bytes(code[p:p+2], 'little') for p in range(position, position+8, 2))
            delta = int.from_bytes(code[position+8:position+10], 'little', signed=True)
            position += 10
            targets[len(decoded)] = position + delta
        elif op in (0x38, 0x39):
            token += (int.from_bytes(code[position:position+2], 'little'), code[position+2])
            position += 3
        elif op == 0x44:
            token += (code[position],)
            position += 1
        else:
            assert op in zero, ('unexpected UCC opcode', hex(op))
        decoded.append(token)
    offsets[position] = len(decoded)
    return body[:start+6], decoded, {i: offsets[target] for i, target in targets.items()}


def audit_correction(ucc, game, replacement, directory):
    """Recompile with original defaults; prove the correction's precise delta."""
    source = (ROOT / 'data' / ('quiz_' + game + '.uc')).read_text()
    if game == 'bg':
        source, changes = re.subn(r'UI_input_numeric_value\((0, (?:210, 5|60, 1)), v\d+\[v0\[v5\]\]\)',
                                  r'UI_input_numeric_value(\1, 0)', source)
        assert changes == 2
    else:
        source, changes = re.subn(r'UI_input_numeric_value\(1, 70, 1, \d+\)',
                                  'UI_input_numeric_value(1, 70, 1, 35)', source)
        assert changes == 15
    path = Path(directory) / (game + '.uc')
    path.write_text(source)
    output = path.with_suffix('.ucb')
    subprocess.run([ucc, '-b', '-o', str(output), str(path)], check=True, capture_output=True)
    baseline = functions(output.read_bytes())
    result = {}
    for number, (body, _) in replacement.items():
        oldhead, old, oldtargets = instructions(baseline[number][0])
        newhead, new, newtargets = instructions(body)
        assert oldhead == newhead, ('non-code layout changed', hex(number))
        mapping, deltas = {len(old): len(new)}, []
        for kind, a, b, c, d in difflib.SequenceMatcher(a=old, b=new, autojunk=False).get_opcodes():
            if kind == 'equal':
                mapping.update(zip(range(a, b), range(c, d)))
            else:
                assert kind == 'replace'
                assert old[a:b] == [(0x1f, 0 if game == 'bg' else 35)]
                if game == 'bg':
                    assert new[c:d] == [(0x21, 3), (0x26, 0), (0x26, 6)]
                else:
                    assert d-c == 1 and new[c][0] == 0x1f
                mapping[a] = c
                deltas.append({'old': old[a:b], 'new': new[c:d]})
        assert len(deltas) == (1 if game == 'bg' else 15)
        for instruction, target in oldtargets.items():
            assert newtargets[mapping[instruction]] == mapping[target], ('branch changed', hex(number))
        result[hex(number)] = {'ucc_original_defaults_body_bytes': len(baseline[number][0]),
                               'only_default_instructions_changed': True,
                               'branch_destinations_preserved': True, 'deltas': deltas}
    return result


class Run:
    def __init__(self, function, game, rolls, mode, flags=None):
        body, extended = function
        width = 4 if extended else 2
        size = int.from_bytes(body[:width], 'little')
        self.strings = body[width:width+size]
        offset = width + size
        self.argc = int.from_bytes(body[offset:offset+2], 'little')
        locals_count = int.from_bytes(body[offset+2:offset+4], 'little')
        externs = int.from_bytes(body[offset+4:offset+6], 'little')
        assert self.argc == 0 and externs == 0
        self.code = body[offset+6:]
        self.locals = [0] * locals_count
        self.stack, self.trace, self.questions = [], [], []
        self.flags = dict(flags or {})
        self.rolls = iter(rolls)
        self.mode, self.game, self.ip = mode, game, 0
        self.init_loop = False
        self.last_roll = 0
        self.number = 0
        self.pending_text = ''
        self.last_question = 0
        self.intrinsics = re.findall(r'USECODE_INTRINSIC_PTR\((\w+)\)',
                                    (ROOT / 'usecode' / (game + 'intrinsics.h')).read_text())

    def read(self, size=2, signed=False):
        value = int.from_bytes(self.code[self.ip:self.ip+size], 'little', signed=signed)
        self.ip += size
        return value

    def intrinsic(self, name, args):
        if name == 'die_roll':
            self.last_roll = next(self.rolls)
            assert args[0] <= self.last_roll <= args[1]
            self.trace.append((name, args, self.last_roll))
            return self.last_roll
        if name == 'input_numeric_value':
            low, high, step, initial = args
            question = self.last_question
            answer = ANSWERS[self.number][question-1]
            assert low <= answer <= high and (answer-low) % step == 0
            self.questions.append((question, low, high, step, initial, answer))
            index = len(self.questions)-1
            wrong = self.mode == 'wrong' or (self.mode == 'retry' and index == 0)
            chosen = (answer + step if answer + step <= high else answer - step) if wrong else answer
            self.trace.append((name, args[:3], chosen))
            return chosen
        if name in ('set_item_flag', 'clear_item_flag'):
            self.trace.append((name, args))
            return 0
        raise AssertionError(('unexpected intrinsic', name, args))

    def execute(self, number):
        self.number = number
        pop = self.stack.pop
        push = self.stack.append
        for _ in range(100000):
            at = self.ip
            op = self.read(1)
            if op in (0x05, 0x06):
                delta = self.read(signed=True)
                if op == 0x06 or not pop():
                    self.ip += delta
            elif op in (0x09, 0x0e, 0x0f, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x22):
                b, a = pop(), pop()
                push({0x09: lambda: a+b, 0x0e: lambda: int(bool(a) and bool(b)),
                      0x0f: lambda: int(bool(a) or bool(b)), 0x16: lambda: int(a>b),
                      0x17: lambda: int(a<b), 0x18: lambda: int(a>=b),
                      0x19: lambda: int(a<=b), 0x1a: lambda: int(a!=b),
                      0x22: lambda: int(a==b)}[op]())
            elif op == 0x10:
                push(int(not pop()))
            elif op == 0x12:
                self.locals[self.read()] = copy.deepcopy(pop())
            elif op in (0x13, 0x14):
                push(int(op == 0x13))
            elif op in (0x1c, 0x1d):
                offset = self.read()
                text = self.strings[offset:self.strings.index(0, offset)].decode('latin1')
                if op == 0x1c:
                    self.pending_text += text
                else:
                    push(text)
            elif op == 0x1e:
                push([pop() for _ in range(self.read())])
            elif op == 0x1f:
                push(self.read(signed=True))
            elif op == 0x21:
                push(copy.deepcopy(self.locals[self.read()]))
            elif op == 0x26:
                index, array = pop()-1, self.locals[self.read()]
                push(copy.deepcopy(array[index]) if isinstance(array, list) and 0 <= index < len(array) else 0)
            elif op in (0x25, 0x2c, 0x2d, 0x32):
                return (pop() if op == 0x2d else 0), self.flags, self.trace
            elif op == 0x2e:
                self.init_loop = True
            elif op == 0x02:
                counter, total, variable, array = [self.read() for _ in range(4)]
                delta = self.read(signed=True)
                if self.init_loop:
                    self.locals[counter] = 0
                    self.locals[total] = len(self.locals[array])
                    self.init_loop = False
                index = self.locals[counter]
                if index >= self.locals[total]:
                    self.ip += delta
                else:
                    self.locals[variable] = self.locals[array][index]
                    self.locals[counter] = index + 1
            elif op == 0x2f:
                self.pending_text += str(self.locals[self.read()])
            elif op == 0x30:
                array, value = pop(), pop()
                push(int(value in array))
            elif op == 0x33:
                self.trace.append(('say', self.pending_text))
                question = self.pending_text.strip('"')
                if question in QUESTION_TEXTS[number]:
                    self.last_question = QUESTION_TEXTS[number].index(question) + 1
                self.pending_text = ''
            elif op in (0x38, 0x39):
                name = self.intrinsics[self.read()]
                args = [pop() for _ in range(self.read(1))]
                result = self.intrinsic(name, args)
                if op == 0x38:
                    push(result)
            elif op == 0x3e:
                push('caller')
            elif op == 0x42:
                push(int(self.flags.get(self.read(), 0)))
            elif op == 0x43:
                flag, value = self.read(), pop()
                self.flags[flag] = value
            elif op == 0x44:
                push(self.read(1))
            elif op == 0x46:
                array, index, value = self.read(), pop()-1, pop()
                self.locals[array][index] = value
            else:
                raise AssertionError(('unsupported instruction', hex(op), at))
        raise AssertionError('instruction limit exceeded')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bg', required=True)
    parser.add_argument('--si', required=True)
    parser.add_argument('--header', required=True)
    parser.add_argument('--mods', required=True)
    parser.add_argument('--report', required=True)
    parser.add_argument('--ucc', required=True)
    args = parser.parse_args()
    report = {'resources': [], 'questions': {}, 'functions': {}, 'cases': 0}
    for game in ('bg', 'si'):
        path = Path(getattr(args, game))
        original = functions(path.read_bytes())
        replacement = embedded(args.header, game)
        assert set(replacement) == ({0x886, 0x852} if game == 'bg' else {0x98c})
        with tempfile.TemporaryDirectory(prefix='quiz-audit-', dir=Path(args.report).resolve().parent) as temporary:
            report.setdefault('structural_audit', {}).update(audit_correction(args.ucc, game, replacement, temporary))
        modpath = Path(args.mods) / ('Keyring' if game == 'bg' else 'sifixes') / 'data/usecode'
        mod = functions(modpath.read_bytes())
        assert not set(mod).intersection(replacement)
        for p in (path, modpath):
            report['resources'].append({'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})
        for number, function in replacement.items():
            body, _ = original[number]
            strings = body[2:2+int.from_bytes(body[:2], 'little')].decode('latin1').split('\0')
            newbody, extended = function
            assert not extended
            newsize = int.from_bytes(newbody[:2], 'little')
            newstrings = newbody[2:2+newsize].decode('latin1').split('\0')
            assert set(strings) == set(newstrings), ('dialogue pool changed', hex(number))
            def metadata(blob):
                offset = 2 + int.from_bytes(blob[:2], 'little')
                return {'body_bytes': len(blob), 'text_bytes': offset-2,
                        'arguments': int.from_bytes(blob[offset:offset+2], 'little'),
                        'locals': int.from_bytes(blob[offset+2:offset+4], 'little'),
                        'externs': int.from_bytes(blob[offset+4:offset+6], 'little')}
            report['functions'][hex(number)] = {'original': metadata(body), 'replacement': metadata(newbody),
                                               'dialogue_string_set_identical': True}
            questions = [s.strip('"') for s in strings if s.strip('"').endswith('?')]
            QUESTION_TEXTS[number] = list(reversed(questions)) if game == 'bg' else questions
            assert len(QUESTION_TEXTS[number]) == len(ANSWERS[number])
            seen = {}
            count = len(ANSWERS[number])
            size = 3 if number != 0x852 else 2
            sequences = list(itertools.permutations(range(1, count+1), size))
            rng = random.Random(number)
            duplicate_draws = [tuple(rng.sample(range(1, count+1), size)) for _ in range(20)]
            # SI: a failing first round consumes three more fresh questions.
            for case, sequence in enumerate(sequences + duplicate_draws):
                extra = [i for i in range(1, count+1) if i not in sequence][:3]
                for mode in ('correct', 'wrong', 'retry'):
                    rolls = list(sequence) + extra
                    if case >= len(sequences):
                        rolls.insert(1, sequence[0])
                    new = Run(function, game, rolls, mode)
                    old = Run(original[number], game, rolls, mode)
                    result = new.execute(number)
                    reference = old.execute(number)
                    assert result == reference, (hex(number), sequence, mode, result, reference)
                    success = mode != 'wrong' if game == 'si' else mode == 'correct'
                    if number == 0x852:
                        assert result[1][56] == int(not success)
                        expected_flag = 'clear_item_flag' if success else 'set_item_flag'
                        assert result[2][-1] == (expected_flag, ['caller', 25])
                    else:
                        assert result[0] == int(success)
                        if game == 'si' and not success:
                            assert result[2][-1] == ('set_item_flag', [-356, 25])
                    for q, low, high, step, initial, answer in new.questions:
                        assert initial == answer, (hex(number), q, initial, answer)
                        seen[q] = {'answer': answer, 'minimum': low, 'maximum': high, 'step': step}
                    # BG retries recall the flags set by the preceding invocation.
                    if game == 'bg':
                        new = Run(function, game, [], mode, result[1])
                        old = Run(original[number], game, [], mode, reference[1])
                        assert new.execute(number) == old.execute(number)
                        for record in new.questions:
                            assert record[4] == record[5]
                    report['cases'] += 1
            assert len(seen) == count
            report['questions'][hex(number)] = seen
    Path(args.report).write_text(json.dumps(report, indent=2) + '\n')
    print('PASS:', report['cases'], 'differential cases; all 29 defaults; supported mod helper overrides absent')


if __name__ == '__main__':
    main()
