"""Read-only exact human fixtures, legacy TSV equivalence, safety, budget and mutation checks."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess

from check_offline_inspection import run, validate_diagnostic


def safe(data):
    assert not data.startswith(b'\xef\xbb\xbf')
    text = data.decode('utf-8', 'strict')
    assert not text or text.endswith('\n')
    for c in text:
        n = ord(c)
        assert (n >= 32 or c == '\n') and not 127 <= n <= 159
        assert n not in (0x61c, 0x200e, 0x200f) and not 0x2028 <= n <= 0x202e
        assert not 0x2066 <= n <= 0x206f


def exact(result, case):
    assert (result.returncode, result.stdout, result.stderr) == (
        case['exit'], case['stdout'].encode('utf-8'), case['stderr'].encode('ascii')), (
            case['name'], result.returncode, result.stdout, result.stderr)
    safe(result.stdout)
    validate_diagnostic(result.stderr, result.returncode)
    lines = result.stdout.splitlines()
    assert sum(line.startswith(b'Complete: ') for line in lines) == (result.returncode == 0)
    if result.returncode == 0:
        assert lines[-1].startswith(b'Complete: ')


def hashes(root):
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def check(exe, root):
    before = hashes(root)
    document = json.loads((root / 'designed.json').read_text(encoding='utf-8'))
    cases = document['cases']
    assert document['format'] == 'qdcxx-human-designed-v1' and cases
    assert len({c['name'] for c in cases}) == len(cases)
    calls = 0
    measured = {}
    for case in cases:
        path = (root / case['input']).resolve()
        base = ['--profile', 'oracle-sre-v1', '--input', str(path), '--output-format', 'human']
        for size in sorted({1, 2, 3, 7, 16, 4096, path.stat().st_size + 1}):
            result = run(exe, [*base, '--chunk-size', str(size), *case['args']])
            exact(result, case)
            calls += 1
        if case['exit'] == 0 and not case['args']:
            expected = case['stdout'].encode('utf-8')
            exact(run(exe, [*base, '--max-output', str(len(expected))]), case)
            # Every row boundary: exact prefix fits, one less cannot partially publish that row.
            prefix = 0
            for line in expected.splitlines(keepends=True):
                for limit in {prefix, prefix + len(line) - 1}:
                    p = run(exe, [*base, '--max-output', str(limit)])
                    assert p.returncode == 6 and p.stdout == expected[:prefix], (case['name'], limit, p)
                    validate_diagnostic(p.stderr, 6)
                    assert p.stderr.split(b'\t')[2] == b'write'
                prefix += len(line)
            measured[case['name']] = len(expected)
    # Every historical designed scenario: default and explicit tsv must equal original bytes/exits.
    old = root.parent / 'offline-inspection'
    legacy = json.loads((old / 'designed.json').read_text(encoding='utf-8'))['cases']
    for case in legacy:
        base = ['--profile', 'oracle-sre-v1', '--input', str(old / case['input']), *case['args']]
        for size in (1, 7, 4096):
            arguments = [*base, '--chunk-size', str(size)]
            a = run(exe, arguments)
            b = run(exe, [*arguments, '--output-format', 'tsv'])
            expected = (case['exit'], case['stdout'].encode('ascii'), case['stderr'].encode('ascii'))
            assert (a.returncode, a.stdout, a.stderr) == expected
            assert (b.returncode, b.stdout, b.stderr) == expected
            calls += 2
    base = ['--profile', 'oracle-sre-v1', '--input', str(old / 'expected/composed.qtp')]
    a = run(exe, base); b = run(exe, [*base, '--output-format', 'tsv'])
    assert a.returncode == b.returncode == 0 and a.stdout == b.stdout and a.stderr == b.stderr == b''
    for tail in [['--output-format'], ['--output-format', ''], ['--output-format', 'auto'],
                 ['--output-format', 'HUMAN'], ['--output-format', 'human', '--output-format', 'tsv'],
                 ['--output-format', 'tsv', '--output-format', 'tsv'], ['--output-format=human']]:
        p = run(exe, [*base, *tail])
        assert p.returncode == 2 and not p.stdout
        assert p.stderr == b'incomplete\t2\tusage\t-\t-\t-\t-\t-\t-\t-\t-\n'
    p = run(exe, ['--help'])
    assert p.returncode == 0 and b'--output-format tsv|human' in p.stdout and b'not QD tape' in p.stdout
    p = subprocess.Popen([str(exe), *base, '--output-format', 'human'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    p.stdout.close(); error = p.stderr.read(); code = p.wait(timeout=30)
    assert code == 3; validate_diagnostic(error, 3)
    # Mutate temporary expectations in memory; verifier must reject each and pass the unchanged baseline.
    case = next(c for c in cases if c['name'] == 'lone-high')
    actual = run(exe, ['--profile', 'oracle-sre-v1', '--input', str(root / case['input']), '--output-format', 'human'])
    mutations = {
        'label': case['stdout'].replace('S.v=', 'v='),
        'value': case['stdout'].replace('S.v=1', 'S.v=2'),
        'escape': case['stdout'].replace('\\uD800', '\\uD801'),
        'escape-lookalike': case['stdout'].replace('\\uD800', '\\\\uD800'),
        'completion-count': case['stdout'].replace('records=1', 'records=2'),
        'missing-completion': case['stdout'].rsplit('Complete:', 1)[0],
        'double-completion': case['stdout'] + case['stdout'].splitlines()[-1] + '\n',
        'trailing-output': case['stdout'] + 'Record 999: injected\n',
        'frame-offset': case['stdout'].replace('bytes=[0,9)', 'bytes=[1,9)'),
    }
    for name, changed in mutations.items():
        assert changed != case['stdout']
        altered = copy.deepcopy(case); altered['stdout'] = changed
        try:
            exact(actual, altered)
        except AssertionError:
            pass
        else:
            raise AssertionError(('mutation accepted', name))
    exact(actual, case)
    assert before == hashes(root), 'verification rewrote fixtures'
    print(json.dumps({'humanCases': len(cases), 'legacyCases': len(legacy), 'chunkInvocations': calls,
                      'outputBytes': measured, 'rejectedMutations': list(mutations),
                      'baseline': 'passed; fixtures unchanged'}, sort_keys=True))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('fixtures', type=Path)
    args = parser.parse_args()
    check(args.executable.resolve(), args.fixtures.resolve())
