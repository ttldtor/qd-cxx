"""Strict process-level CLI contract checks; designed policy and Java observations stay separate."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def unsigned(cell):
    assert re.fullmatch(r'0|[1-9][0-9]*', cell), ('unsigned', cell)
    value = int(cell)
    assert value <= 2**64-1
    return value


def units(cell):
    assert cell == '-' or re.fullmatch(r'(?:[0-9a-f]{4})+', cell), ('units', cell)


def values(cell):
    if cell == '-':
        return
    for v in cell.split(','):
        assert re.fullmatch(r'0|-?[1-9][0-9]*', v), ('value', v)
        assert -(2**31) <= int(v) < 2**31


def validate(data, success):
    """Reject unknown rows/arity/syntax/order; return complete semantic record cells."""
    assert b'\r' not in data and not data.startswith(b'\xef\xbb\xbf')
    text = data.decode('ascii')
    assert not text or text.endswith('\n')
    rows = [r.split('\t') for r in text.splitlines()]
    if not rows:
        assert not success
        return []
    assert rows[0] == ['format', 'qd-qtp-inspect', '1']
    frame_count = 0
    records = []
    end = 0
    done = False
    current_type = None
    for row in rows[1:]:
        assert not done and all(row), ('after completion/empty cell', row)
        kind = row[0]
        if kind == 'frame':
            assert len(row) == 7
            assert unsigned(row[1]) == frame_count
            assert unsigned(row[2]) == end
            end = unsigned(row[3])
            assert end > int(row[2])
            if row[4] == '-':
                assert row[5:] == ['-', '0']
                current_type = None
            else:
                current_type = unsigned(row[4])
                assert current_type <= 2**31-1
                base = unsigned(row[5]); size = unsigned(row[6])
                assert int(row[2]) < base <= end and base+size == end
            frame_count += 1
        elif kind == 'record':
            assert len(row) == 7 and current_type == 15
            assert unsigned(row[1]) == frame_count-1
            assert unsigned(row[2]) == len(records)
            assert unsigned(row[3]) <= 2**31-1
            units(row[4]); units(row[5]); values(row[6])
            assert row[4] != '-'
            records.append(row[3:])
        elif kind == 'complete':
            assert len(row) == 6 and success
            assert unsigned(row[1]) == end
            assert unsigned(row[2]) == frame_count
            assert unsigned(row[3]) == len(records)
            assert unsigned(row[4]) >= len(records)
            unsigned(row[5]); done = True
        else:
            raise AssertionError(('unknown row', row))
    assert done == success
    return records


def validate_diagnostic(data, code):
    if code == 0:
        assert data == b''
        return
    assert data.endswith(b'\n') and b'\r' not in data and data.count(b'\n') == 1
    row = data.decode('ascii').rstrip('\n').split('\t')
    assert len(row) == 11 and row[:2] == ['incomplete', str(code)]
    assert re.fullmatch(r'(?:usage|open|read|write|flush|input-limit|frame-limit|sequence|type|duplicate-id|allocation|(?:binding|data|description|framing)\.[0-9]+)', row[2])
    for v in row[3:]:
        if v != '-':
            unsigned(v)
    for rel, absolute in [(7,9),(8,10)]:
        if row[6] != '-' and row[rel] != '-':
            assert unsigned(row[6])+unsigned(row[rel]) == unsigned(row[absolute])


def java_records(path):
    """Read the entire independently captured observation, with no ignored/unknown rows."""
    rows = path.read_text(encoding='utf-8').splitlines()
    assert rows[0] == 'profile\tQD-3.355\toracle-sre-v1\tEMPTY_SET\traw-QTP'
    wire = rows[1].split('\t')
    assert len(wire) == 2 and wire[0] == 'wire' and re.fullmatch(r'(?:[0-9a-f]{2})+',wire[1])
    records=[]
    for line in rows[2:-2]:
        row=line.split('\t')
        assert len(row)==7 and row[0]=='record' and unsigned(row[1])==len(records)
        unsigned(row[2]);units(row[3]);units(row[4]);values(row[5]);assert row[6]=='0'
        records.append(row[2:6])
    end=rows[-2].split('\t')
    assert len(end)==4 and end[0]=='end' and unsigned(end[1])==len(bytes.fromhex(wire[1]))
    assert unsigned(end[2])==len(records)>0 and unsigned(end[3])>0
    assert rows[-1]=='complete\toffline-inspection'
    return bytes.fromhex(wire[1]),records


def run(exe, arguments):
    return subprocess.run([str(exe),*arguments],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)


def check(exe, root, manifest, quick=False):
    document=json.loads(manifest.read_text(encoding='utf-8'))
    assert document['format']=='qdcxx-offline-designed-v1' and document['cases']
    assert len({c['name'] for c in document['cases']})==len(document['cases'])
    invocations=0
    for case in document['cases']:
        path=root/case['input']
        assert path.is_file()
        expected=case['stdout'].encode('ascii');error=case['stderr'].encode('ascii')
        validate(expected,case['exit']==0);validate_diagnostic(error,case['exit'])
        # Every boundary as a fixed read size plus bytewise, whole, and larger-than-file reads.
        sizes=[4096] if quick else sorted({1,2,3,7,16,4096,len(path.read_bytes())+1})
        for size in sizes:
            p=run(exe,['--profile','oracle-sre-v1','--input',str(path), '--chunk-size',str(size),*case['args']])
            assert (p.returncode,p.stdout,p.stderr)==(case['exit'],expected,error), (case['name'],size,p.returncode,p.stdout,p.stderr,expected,error)
            validate(p.stdout,p.returncode==0);validate_diagnostic(p.stderr,p.returncode)
            invocations+=1
    wire,records=java_records(root/'expected/offline-inspection.tsv')
    assert (root/'expected/composed.qtp').read_bytes()==wire
    baseline=None
    for size in ([4096] if quick else list(range(1,len(wire)+2))+[4096]):
        p=run(exe,['--profile','oracle-sre-v1','--input',str(root/'expected/composed.qtp'),'--chunk-size',str(size)])
        assert p.returncode==0 and p.stderr==b'', (p.returncode,p.stderr)
        assert validate(p.stdout,True)==records
        if baseline is None: baseline=p.stdout
        assert p.stdout==baseline
        invocations+=1
    if not quick:
        good=['--profile','oracle-sre-v1','--input',str(root/'expected/composed.qtp')]
        bad=[['--help','x'],good+['--bogus','1'],good+['--profile','oracle-sre-v1'],good+['--max-input'],
             good+['--max-input','-1'],good+['--max-input','+1'],good+['--max-input','1x'],good+['--max-input',''],
             good+['--max-input','18446744073709551616'],good+['--chunk-size','0'],good+['--max-frame','18446744073709551615'],
             good+['--chunk-size','18446744073709551615'],['--profile','wrong','--input','x'],['--input','x'],
             ['--profile','oracle-sre-v1'],['--profile','oracle-sre-v1','--input','-'],good+['--max-input','1','--max-input','2'],
             good+['--max-input=1'],good+['extra']]
        for arguments in bad:
            p=run(exe,arguments);assert p.returncode==2 and p.stdout==b'',(arguments,p)
            validate_diagnostic(p.stderr,2);invocations+=1
        p=run(exe,['--help']);assert p.returncode==0 and b'--max-output' in p.stdout and not p.stderr
        no_arguments=run(exe,[])
        assert (no_arguments.returncode,no_arguments.stdout,no_arguments.stderr)==(0,p.stdout,b'')
        assert p.stdout.startswith(b'qdcxx-qtp-inspect --profile ') and b'complete\t' not in p.stdout
        invocations+=1
        # Close the reader before launch so help-output failure does not depend on process scheduling.
        for arguments in ([],['--help']):
            read_fd,write_fd=os.pipe()
            os.close(read_fd)
            try:
                failed=subprocess.run([str(exe),*arguments],stdout=write_fd,stderr=subprocess.PIPE,timeout=30)
            finally:
                os.close(write_fd)
            assert failed.returncode==3,(arguments,failed.returncode,failed.stderr)
            validate_diagnostic(failed.stderr,3)

        with tempfile.TemporaryDirectory() as tmp:
            p=run(exe,['--profile','oracle-sre-v1','--input',str(Path(tmp)/'absent.qtp')])
            assert p.returncode==3 and not p.stdout;validate_diagnostic(p.stderr,3)
            # A directory is not a readable binary file: OSes may fail at open or first read.
            p=run(exe,['--profile','oracle-sre-v1','--input',tmp])
            assert p.returncode==3 and b'complete\t' not in p.stdout;validate_diagnostic(p.stderr,3)
        # Real process pipe failure complements portable streambuf injection.
        p=subprocess.Popen([str(exe),*good],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        p.stdout.close()
        err=p.stderr.read();code=p.wait(timeout=30)
        assert code==3,(code,err)
        validate_diagnostic(err,3)
    print(json.dumps({'designedCases':len(document['cases']),'javaRecords':len(records),'processInvocations':invocations}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path);parser.add_argument('fixtures',type=Path)
    parser.add_argument('--manifest',type=Path);parser.add_argument('--quick',action='store_true')
    args=parser.parse_args()
    check(args.executable.resolve(),args.fixtures.resolve(),args.manifest or args.fixtures/'designed.json',args.quick)
