"""Immutable negative checks for reviewed binding plans; no golden rewriting."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    baseline = Path(__file__).parent / 'protocol/binding/plans.tsv'
    original = baseline.read_bytes()
    text = original.decode()
    lines = text.splitlines(keepends=True)
    mutations = {
        'target': text.replace('7/1/1,0', '7/0/1,0'),
        'order': text.replace('7/1/1,0', '7/1/0,1'),
        'skip': text.replace('11/1/1,-,0', '11/1/1,1,0'),
        'duplicate-target': text.replace('16/1/1,1,0,1', '16/1/0,1,0,1'),
        'missing-case': ''.join(lines[:1] + lines[2:]),
        'empty-coverage': lines[0],
        'empty-file': '',
        'malformed-hex': text.replace('reorder\t', 'reorder\tzz', 1),
        'unknown-case': text.replace('reorder\t', 'other\t', 1),
        'trailing-row': text + 'unexpected\n',
    }
    results = {}
    with tempfile.TemporaryDirectory(prefix='qdcxx-binding-') as directory:
        for name, value in mutations.items():
            assert value != text
            candidate = Path(directory) / (name + '.tsv')
            candidate.write_text(value, encoding='utf-8', newline='\n')
            before = candidate.read_bytes()
            result = subprocess.run([str(args.executable.resolve()), str(candidate)], capture_output=True)
            assert result.returncode != 0, name
            assert candidate.read_bytes() == before, name
            results[name] = result.returncode
        missing = subprocess.run([str(args.executable.resolve()), str(Path(directory) / 'missing.tsv')],
                                 capture_output=True)
        assert missing.returncode != 0
        results['missing-file'] = missing.returncode
    result = subprocess.run([str(args.executable.resolve()), str(baseline.resolve())], capture_output=True)
    assert result.returncode == 0, result.stdout.decode(errors='replace')
    assert baseline.read_bytes() == original
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'negativeExitCodes': results, 'baselinePass': True,
                                      'baselineSha256': hashlib.sha256(original).hexdigest()}, indent=2) + '\n')
    print(f'{len(results)} negative cases detected; unchanged baseline passed')


if __name__ == '__main__':
    main()
