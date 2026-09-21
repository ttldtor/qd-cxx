"""Reject mutated description expectations and malformed/empty evidence without changing inputs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    original = root / 'tests/fixtures/protocol/p08/expected/descriptions.tsv'
    before = digest(original)
    text = original.read_text()
    rows = text.splitlines(keepends=True)
    mutations = {
        'expected-type': text.replace('0/0052/0078:8', '0/0052/0078:9'),
        'invalid-hex': text.replace('\t00015200\t', '\tgg015200\t', 1),
        'unknown-row': text.replace('case\t', 'unknown\t', 1),
        'invalid-status': text.replace('\tOK\t', '\tUNKNOWN\t', 1),
        'missing-case': ''.join(rows[1:]),
        'empty-coverage': 'complete\tdescriptions\n',
        'empty-file': '',
        'trailing-row': text + 'complete\tdescriptions\n',
    }
    results = []
    with tempfile.TemporaryDirectory(prefix='description-negative-', dir=root / '.work') as directory:
        for name, changed in mutations.items():
            path = Path(directory) / (name + '.tsv')
            path.write_text(changed, newline='\n')
            fingerprint = digest(path)
            run = subprocess.run([str(args.executable.resolve()), str(path)], capture_output=True, text=True)
            assert run.returncode != 0, name
            assert digest(path) == fingerprint, name
            results.append({'name': name, 'exitCode': run.returncode, 'temporaryUnchanged': True,
                            'stdout': run.stdout, 'stderr': run.stderr})
        run = subprocess.run([str(args.executable.resolve()), str(original)], capture_output=True, text=True)
        assert run.returncode == 0, run.stdout
        assert digest(original) == before
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'executedAtUtc': datetime.now(timezone.utc).isoformat(),
        'baselineSha256': before, 'baselineUnchanged': True, 'baselinePassed': True,
        'negativeChecks': results, 'baselineOutput': run.stdout}, indent=2) + '\n')
    print(f'PASS: {len(results)} negative checks; baseline passed; all fixtures unchanged')


if __name__ == '__main__':
    main()
