"""Audit retained Java evidence; not a primitive codec or C++ conformance test."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIX = ROOT / 'tests/fixtures/encoding'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    expected = FIX / 'expected'
    assert sorted(p.name for p in expected.iterdir()) == ['forms.tsv', 'integers.tsv', 'strings.tsv']
    counts = {}
    for path in sorted(expected.iterdir()):
        rows = [line.split('\t') for line in path.read_text(encoding='utf-8').splitlines()]
        assert rows[-1] == ['complete', path.stem]
        writers = [r for r in rows if r[0] == 'writer']
        readers = [r for r in rows if r[0] == 'reader']
        lookup = {(r[1], r[2]): r for r in readers}
        assert len(lookup) == len(readers)
        for r in readers:
            size = len(bytes.fromhex(r[3]))
            assert 0 <= int(r[6]) <= size and int(r[7]) == size - int(r[6])
        for w in writers:
            method = w[2].replace('write', 'read', 1)
            r = lookup[w[1], method]
            assert r[3:6] == [w[4], 'ok', w[3]]
            assert int(r[6]) == len(bytes.fromhex(w[4])) and r[7] == '0'
            if w[1].startswith(('I-', 'L-', 'S-')):
                for n in range(len(bytes.fromhex(w[4]))):
                    prefix = lookup[f'{w[1]}/prefix-{n}', method]
                    assert prefix[3] == w[4][:n * 2]
                    assert prefix[4:7] == ['error', 'java.io.EOFException', str(n)]
        counts[path.name] = {'writers': len(writers), 'readers': len(readers),
                             'prefixes': sum('/prefix-' in r[1] for r in readers),
                             'errors': sum(r[4] == 'error' for r in readers)}
    integers = (expected / 'integers.tsv').read_text().splitlines()
    writer_ids = {r.split('\t')[1] for r in integers if r.startswith('writer\t')}
    for kind, bits in [('I', [6, 13, 20, 27]), ('L', [6, 13, 20, 27, 34, 41, 48, 55])]:
        for bit in bits:
            for sign in ['neg', 'pos']:
                for delta in [-1, 0, 1]:
                    assert f'{kind}-b{bit}-{sign}-d{delta}' in writer_ids
        for name in ['zero', 'minus1', 'plus1', 'minus2', 'min', 'max']:
            assert f'{kind}-{name}' in writer_ids
    proof = json.loads((FIX / 'captures/verify-proof.json').read_text())
    for key in ['mismatchDetected', 'temporaryExpectationsUnchanged', 'baselinePassed', 'baselineUnchanged']:
        assert proof[key] is True
    assert proof['observedFailure'] == 'VERIFY FAILED: integers.tsv'
    validation = json.loads((FIX / 'captures/validation.json').read_text())
    for run in ['capture-a', 'capture-b', 'mismatch', 'baseline']:
        meta = json.loads((FIX / 'captures' / run / 'metadata.json').read_text())
        assert meta['upstream']['commit'] == '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
        assert meta['upstreamClean'] is True
        for tool in meta['toolHashes']:
            assert sha(ROOT / 'tools/upstream/encoding-oracle' / tool['path']) == tool['sha256']
        assert sha(FIX / 'cases.json') == meta['casesSha256']
        assert sha(ROOT / 'tools/upstream/schema-oracle/SchemaOracle.java') == meta['environmentToolSha256']
        for name, hashes in validation['runs'][run].items():
            assert sha(expected / name) == hashes['normalized']
            if run.startswith('capture-'):
                raw = FIX / 'captures' / run / 'raw' / name
                assert sha(raw) == hashes['raw']
                assert raw.read_bytes().replace(b'\r\n', b'\n') == (expected / name).read_bytes()
                assert raw.with_suffix('.stderr').stat().st_size == 0
    assert validation['rawCapturesEqual'] and validation['oldGoldensUnchanged']
    for rel, digest in validation['oldGoldenHashes'].items():
        assert sha(ROOT / rel) == digest
    print(json.dumps({'status': 'PASS', 'counts': counts}, indent=2))


if __name__ == '__main__':
    main()
