"""Immutable negative checks for Java symbol projections; no golden rewriting."""
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
    baseline = Path(__file__).parent / '../fixtures/encoding/symbols/expected/symbols.tsv'
    original = baseline.read_bytes()
    text = original.decode()
    lines = text.splitlines(keepends=True)
    rows = [line.rstrip('\n').split('\t') for line in lines]
    def changed(phase, column, value):
        copy = [r.copy() for r in rows]
        for r in copy:
            if r[0] == 'read' and r[1] == 'abc' and r[2] == phase:
                r[column] = value
                return '\n'.join('\t'.join(r) for r in copy) + '\n'
        raise AssertionError('Missing mutation target')
    mutations = {
        'decoded-units': changed('main', 9, '004100420044'),
        'consumed': changed('main', 6, '1'),
        'repeat-state': changed('after-repeat', 9, '0058'),
        'mru-state': changed('after-mru', 10, '7'),
        'cipher': changed('main', 7, '0'),
        'missing-case': ''.join(line for line in lines if not line.startswith('read\tabc\t')),
        'empty-file': '',
        'missing-completion': ''.join(lines[:-1]),
        'malformed-units': changed('main', 9, 'xyz'),
        'unknown-row': ''.join(lines[:-1]) + 'unknown-observation\t0\n' + lines[-1],
        # A trailing empty cell must count as an extra column, even on diagnostic-only rows.
        'wrong-column-count': lines[0].rstrip('\n') + '\t\n' + ''.join(lines[1:]),
        'duplicate-complete': text + lines[-1],
        'row-after-complete': text + 'parse.begin\t28\n',
    }
    expected_diagnostics = {
        'unknown-row': 'Unknown Java TSV row type: unknown-observation',
        'wrong-column-count': 'Wrong TSV column count',
        'duplicate-complete': 'No observations, including another complete, may follow complete',
        'row-after-complete': 'No observations, including another complete, may follow complete',
    }
    results = {}
    confirmed_diagnostics = {}
    with tempfile.TemporaryDirectory(prefix='qdcxx-symbol-') as directory:
        for name, value in mutations.items():
            assert value != text
            candidate = Path(directory) / (name + '.tsv')
            candidate.write_text(value, encoding='utf-8', newline='\n')
            before = candidate.read_bytes()
            result = subprocess.run([str(args.executable.resolve()), str(candidate)], capture_output=True)
            assert result.returncode != 0, name
            if name in expected_diagnostics:
                diagnostic = expected_diagnostics[name]
                assert diagnostic in result.stdout.decode(errors='replace'), name
                confirmed_diagnostics[name] = diagnostic
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
                                      'newCaseDiagnostics': confirmed_diagnostics,
                                      'baselineSha256': hashlib.sha256(original).hexdigest()}, indent=2) + '\n')
    print(f'{len(results)} negative cases detected; unchanged baseline passed')


if __name__ == '__main__':
    main()
