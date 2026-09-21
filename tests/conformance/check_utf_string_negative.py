"""Negative checks use temporary copies only; no expected UTFString bytes are synthesized."""
import argparse
from datetime import datetime, timezone
import os
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'tests/fixtures/encoding/expected'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command):
    result = subprocess.run([str(x) for x in command], cwd=ROOT, capture_output=True, text=True, env={**os.environ, 'TZ': 'UTC'})
    return result.returncode, result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve()
    original = {p.name: digest(p) for p in FIXTURES.glob('*.tsv')}
    results = {}
    mutations = {
        'writer-byte': ('writer\tS-ascii\twriteUTFString\t004100420043\t03414243', 'writer\tS-ascii\twriteUTFString\t004100420043\t03414244', 'bytes expected 03414244 actual 03414243'),
        'reader-value': ('03414243\tok\t004100420043', '03414243\tok\t004100420044', 'value expected 004100420044 actual 004100420043'),
        'bad-units': ('writer\tS-ascii\twriteUTFString\t004100420043', 'writer\tS-ascii\twriteUTFString\t00410042004', 'invalid UTF-16 hex length'),
        'bad-hex': ('writer\tS-ascii\twriteUTFString\t004100420043', 'writer\tS-ascii\twriteUTFString\t0041004200gg', 'invalid hex'),
        'bad-status': ('03414243\tok\t004100420043', '03414243\tunknown\t004100420043', 'unexpected status'),
        'bad-category': ('reader\tS-ascii/prefix-0\treadUTFString\t\terror\tjava.io.EOFException', 'reader\tS-ascii/prefix-0\treadUTFString\t\terror\tunknown', 'unexpected exception category'),
    }
    (ROOT / '.work').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.work', prefix='negative-') as temporary:
        work = Path(temporary)
        for name, (old, new, diagnostic) in mutations.items():
            target = work / name
            shutil.copytree(FIXTURES, target)
            strings = target / 'strings.tsv'
            text = strings.read_text()
            if old not in text:
                raise RuntimeError(f'Mutation target absent: {name}')
            strings.write_text(text.replace(old, new, 1))
            before = digest(strings)
            code, output = run([executable, target])
            if code == 0 or diagnostic not in output or 'S-ascii' not in output or digest(strings) != before:
                raise RuntimeError(f'Negative check failed: {name}: {output}')
            results[name] = {'exit': code, 'diagnostic': next(line for line in output.splitlines() if diagnostic in line),
                             'case': 'S-ascii', 'temporaryUnchanged': True}
        target = work / 'empty'
        shutil.copytree(FIXTURES, target)
        (target / 'strings.tsv').write_text('profile\tQD-3.355\tByteArrayInput\tx\tx\ncomplete\tstrings\n')
        code, output = run([executable, target])
        if code == 0 or 'empty selected coverage' not in output:
            raise RuntimeError('Empty coverage was not rejected')
        results['empty'] = {'exit': code, 'diagnostic': next(line for line in output.splitlines() if 'empty selected coverage' in line)}
        # Exercise actual CTest failure propagation with a temporary test registration.
        (work / 'CTestTestfile.cmake').write_text(
            f'add_test(mutated_java_vector "{executable.as_posix()}" "{(work / "writer-byte").as_posix()}")\n')
        code, output = run(['ctest', '--test-dir', work, '--output-on-failure'])
        if code == 0 or 'bytes expected 03414244 actual 03414243' not in output or '0% tests passed' not in output:
            raise RuntimeError(f'CTest did not propagate mismatch: {output}')
        results['ctest-mutation'] = {'exit': code, 'output': output}
    code, output = run([executable, FIXTURES])
    if code != 0:
        raise RuntimeError(output)
    if original != {p.name: digest(p) for p in FIXTURES.glob('*.tsv')}:
        raise RuntimeError('Real fixture changed')
    args.report.write_text(json.dumps({'executedAtUtc': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
                                      'baselinePassed': True, 'fixtureHashes': original,
                                      'fixturesUnchanged': True, 'checks': results}, indent=2) + '\n')
    print('PASS: UTF fixture/parser/CTest negatives; real and temporary expectations unchanged')


if __name__ == '__main__':
    main()
