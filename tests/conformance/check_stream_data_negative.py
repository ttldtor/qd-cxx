"""Reject temporary STREAM_DATA oracle mutations without modifying real fixtures."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('executable', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    source = Path('tests/fixtures/protocol/stream-data/expected/stream-data.tsv').resolve()
    baseline_hash = digest(source)
    baseline = source.read_text(encoding='utf-8')
    executable = args.executable.resolve()
    mutations = {
        'symbol': baseline.replace('004100420043', '004100420044', 1),
        'value': baseline.replace('22,44,0', '22,45,0', 1),
        'id': baseline.replace('record\t1\t0\t', 'record\t1\t2\t', 1),
        'order': baseline.replace('22,44,0', '44,22,0', 1),
        'failure': baseline.replace('error\tmessage\t15', 'error\tmessage\t14', 1),
        'failure-outcome': baseline.replace('error\tmessage\t15\nend\t73\t0\t0\t0\t1', 'end\t73\t0\t0\t0\t0', 1),
        'unknown-row': baseline.replace('complete\tstream-data', 'unknown\t0\ncomplete\tstream-data'),
        'wrong-arity': baseline.replace('profile\tQD-3.355', 'profile\t\tQD-3.355', 1),
        'duplicate-complete': baseline + 'complete\tstream-data\n',
        'trailing-data': baseline + 'case\tempty\n',
        'missing-complete': baseline.replace('complete\tstream-data\n', ''),
        'required-coverage': baseline.replace('case\tmixed', 'case\tmixed-missing', 1),
        'bad-value': baseline.replace('22,44,0', '22,2147483648,0', 1),
        'wire': baseline.replace('wire\t804402', 'wire\t804403', 1),
    }
    results = []
    with tempfile.TemporaryDirectory(prefix='qdcxx-stream-negative-') as directory:
        for name, changed in mutations.items():
            assert changed != baseline, name
            path = Path(directory) / (name + '.tsv')
            path.write_text(changed, encoding='utf-8', newline='\n')
            before = digest(path)
            run = subprocess.run([str(executable), str(path)], capture_output=True, text=True)
            assert run.returncode != 0, name
            assert digest(path) == before, name
            results.append({'case': name, 'exit': run.returncode, 'temporaryUnchanged': True})
        run = subprocess.run([str(executable), str(Path(directory) / 'absent.tsv')], capture_output=True, text=True)
        assert run.returncode != 0
        results.append({'case': 'missing-file', 'exit': run.returncode})
    subprocess.run([str(executable), str(source)], check=True)
    assert digest(source) == baseline_hash
    args.report.write_text(json.dumps({'executedAtUtc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'baselineSha256': baseline_hash, 'baselineUnchanged': True, 'baselinePassed': True,
        'negatives': results}, indent=2) + '\n', encoding='utf-8')
    print('All 15 negatives rejected; immutable baseline passed.')


if __name__ == '__main__':
    main()
