"""Negative checks use temporary copies only; no expected compact bytes are synthesized."""
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
    parser.add_argument('--skip-format', action='store_true', help='Run codec negatives only (Linux without Windows formatters)')
    args = parser.parse_args()
    executable = args.executable.resolve()
    original = {p.name: digest(p) for p in FIXTURES.glob('*.tsv')}
    results = {}
    mutations = {
        'writer-byte': ('writer\tI-zero\twriteCompactInt\t0\t00', 'writer\tI-zero\twriteCompactInt\t0\t01', 'bytes expected 01 actual 00'),
        'reader-value': ('reader\tI-zero\treadCompactInt\t00\tok\t0\t1\t0', 'reader\tI-zero\treadCompactInt\t00\tok\t1\t1\t0', 'value expected 1 actual 0'),
        'bad-number': ('writer\tI-zero\twriteCompactInt\t0\t00', 'writer\tI-zero\twriteCompactInt\t0x\t00', 'invalid decimal'),
        'bad-hex': ('writer\tI-zero\twriteCompactInt\t0\t00', 'writer\tI-zero\twriteCompactInt\t0\tgg', 'invalid hex'),
        'bad-status': ('\t00\tok\t0\t1\t0', '\t00\tunknown\t0\t1\t0', 'unexpected status'),
        'bad-category': ('java.io.EOFException', 'java.io.IOException', 'unexpected exception category'),
    }
    (ROOT / '.work').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=ROOT / '.work', prefix='negative-') as temporary:
        work = Path(temporary)
        for name, (old, new, diagnostic) in mutations.items():
            target = work / name
            shutil.copytree(FIXTURES, target)
            integers = target / 'integers.tsv'
            text = integers.read_text()
            if old not in text:
                raise RuntimeError(f'Mutation target absent: {name}')
            integers.write_text(text.replace(old, new, 1))
            before = digest(integers)
            code, output = run([executable, target])
            if code == 0 or diagnostic not in output or 'I-zero' not in output or digest(integers) != before:
                raise RuntimeError(f'Negative check failed: {name}: {output}')
            results[name] = {'exit': code, 'diagnostic': next(line for line in output.splitlines() if diagnostic in line),
                             'case': 'I-zero', 'temporaryUnchanged': True}
        target = work / 'empty'
        shutil.copytree(FIXTURES, target)
        (target / 'integers.tsv').write_text('profile\tQD-3.355\tByteArrayInput\tx\tx\ncomplete\tintegers\n')
        code, output = run([executable, target])
        if code == 0 or 'empty selected coverage' not in output:
            raise RuntimeError('Empty coverage was not rejected')
        results['empty'] = {'exit': code, 'diagnostic': next(line for line in output.splitlines() if 'empty selected coverage' in line)}
        # Exercise actual CTest failure propagation with a temporary test registration.
        (work / 'CTestTestfile.cmake').write_text(
            f'add_test(mutated_java_vector "{executable.as_posix()}" "{(work / "writer-byte").as_posix()}")\n')
        code, output = run(['ctest', '--test-dir', work, '--output-on-failure'])
        if code == 0 or 'bytes expected 01 actual 00' not in output or '0% tests passed' not in output:
            raise RuntimeError(f'CTest did not propagate mismatch: {output}')
        results['ctest-mutation'] = {'exit': code, 'output': output}
        if not args.skip_format:
            format_command = ['pwsh', '-NoProfile', '-File', ROOT / 'tools/format/Format-Cpp.ps1']
            sample = work / 'sample.cpp'
            sample.write_text('int main(){return 0;}\n')
            before = digest(sample)
            code, output = run([*format_command, '-Mode', 'Check', '-Files', sample])
            if code == 0 or 'Formatting mismatch' not in output or digest(sample) != before:
                raise RuntimeError(f'Formatting mismatch/read-only check failed: {output}')
            results['format-mismatch'] = {'exit': code, 'originalUnchanged': True}
            code, output = run([*format_command, '-Mode', 'Format', '-Files', sample])
            if code != 0:
                raise RuntimeError(output)
            before = digest(sample)
            for mode in ['Check', 'Format', 'Check']:
                code, output = run([*format_command, '-Mode', mode, '-Files', sample])
                if code != 0 or digest(sample) != before:
                    raise RuntimeError(f'Pipeline idempotence failed: {output}')
            results['format-idempotence'] = True
            code, output = run([*format_command, '-Uncrustify', work / 'missing.exe'])
            if code == 0 or 'Missing formatter' not in output:
                raise RuntimeError('Missing tool not rejected')
            results['missing-tool-rejected'] = True
        else:
            results['format'] = 'not run: --skip-format'
    code, output = run([executable, FIXTURES])
    if code != 0:
        raise RuntimeError(output)
    if original != {p.name: digest(p) for p in FIXTURES.glob('*.tsv')}:
        raise RuntimeError('Real fixture changed')
    args.report.write_text(json.dumps({'executedAtUtc': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
                                      'baselinePassed': True, 'fixtureHashes': original,
                                      'fixturesUnchanged': True, 'checks': results}, indent=2) + '\n')
    print('PASS: fixture/parser/CTest negatives; formatting ' + ('skipped' if args.skip_format else 'passed') +
          '; real and temporary expectations unchanged')


if __name__ == '__main__':
    main()
