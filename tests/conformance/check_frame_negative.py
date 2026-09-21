"""Read-only negative checks for the strict frame fixture selection and derived expectations."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(directory):
    return {p.name: digest(p) for p in directory.iterdir() if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixtures = root / 'tests/fixtures/protocol/expected'
    projections = root / 'tests/conformance/protocol/frames.tsv'
    before = fingerprint(fixtures)
    projection_before = digest(projections)
    outcomes = []
    with tempfile.TemporaryDirectory(prefix='framing-negative-', dir=root / '.work') as temporary:
        temporary = Path(temporary)
        for case in ['baseline', 'expected-byte', 'invalid-hex', 'missing-input', 'empty-selection', 'empty-projections']:
            selected = temporary / case
            selected.mkdir()
            for name in ['P01.tsv', 'P02.tsv']:
                shutil.copyfile(fixtures / name, selected / name)
            expected = selected / 'frames.tsv'
            shutil.copyfile(projections, expected)
            source = selected / 'P02.tsv'
            if case == 'expected-byte':
                original = expected.read_text()
                modified = original.replace('M:0:00:0:3', 'M:0:01:0:3', 1)
                assert modified != original
                expected.write_text(modified)
            elif case == 'invalid-hex':
                source.write_text(source.read_text().replace('p02.input\tempty\t00', 'p02.input\tempty\tgg', 1))
            elif case == 'missing-input':
                source.write_text(source.read_text().replace('p02.input\tempty\t00\n', '', 1))
            elif case == 'empty-selection':
                source.write_text('')
            elif case == 'empty-projections':
                expected.write_text('')
            temporary_before = fingerprint(selected)
            run = subprocess.run([str(args.executable.resolve()), str(selected), str(expected)],
                                 capture_output=True, text=True)
            assert run.returncode == (0 if case == 'baseline' else 1), (case, run.returncode, run.stdout, run.stderr)
            assert '[doctest]' in run.stdout and ('Status: SUCCESS' if case == 'baseline' else 'Status: FAILURE') in run.stdout
            assert fingerprint(selected) == temporary_before
            outcomes.append({'case': case, 'exitCode': run.returncode, 'temporaryFilesUnchanged': True,
                             'stdout': run.stdout, 'stderr': run.stderr})
    assert fingerprint(fixtures) == before
    assert digest(projections) == projection_before
    report = {'executedAtUtc': datetime.now(timezone.utc).isoformat().replace('+00:00', 'Z'),
              'executable': str(args.executable), 'originalFilesUnchanged': True,
              'fixtureHashes': before, 'projectionSha256': projection_before, 'checks': outcomes}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS: baseline, expected-byte mutation, malformed/missing/empty selected inputs and empty projections; all files immutable')


if __name__ == '__main__':
    main()
