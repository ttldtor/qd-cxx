"""Mutate temporary expectations only; require rejection and then an unchanged full baseline."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable',type=Path);parser.add_argument('fixtures',type=Path)
parser.add_argument('--proof',required=True,type=Path)
args=parser.parse_args()
root=args.fixtures.resolve();exe=args.executable.resolve()
def hashes(path):
    return {str(p.relative_to(path)):hashlib.sha256(p.read_bytes()).hexdigest() for p in path.rglob('*') if p.is_file()}
before=hashes(root)
original=json.loads((root/'designed.json').read_text(encoding='utf-8'))
target=next(c for c in original['cases'] if c['name']=='two-data')
def changed(old,new):
    assert old in target['stdout']
    return lambda case:case.update(stdout=case['stdout'].replace(old,new,1))
mutations={
    'value':changed('004100420043\t1\n','004100420043\t9\n'),
    'symbol':changed('004100420043','004100420044'),
    'local-id':changed('record\t1\t0\t0','record\t1\t0\t1'),
    'name':changed('0053','0052'),
    'offset':changed('frame\t0\t0\t9','frame\t0\t1\t9'),
    'row-type':changed('record\t','event\t'),
    'row-arity':changed('record\t1\t0\t0','record\t1\t0'),
    'number-syntax':changed('record\t1\t0\t0','record\t01\t0\t0'),
    'hex-syntax':changed('004100420043','00410042zzzz'),
    'complete-count':lambda c:c.update(stdout=c['stdout'].rsplit('\t',1)[0]+'\t999\n'),
    'missing-complete':lambda c:c.update(stdout='\n'.join(c['stdout'].splitlines()[:-1])+'\n'),
    'double-complete':lambda c:c.update(stdout=c['stdout']+c['stdout'].splitlines()[-1]+'\n'),
    'trailing-row':lambda c:c.update(stdout=c['stdout']+'unknown\tx\n'),
    'empty-selection':lambda c:None,
}
proof={}
with tempfile.TemporaryDirectory(prefix='qd-offline-negative-') as tmp:
    tmp=Path(tmp)
    for name,mutation in mutations.items():
        document=copy.deepcopy(original);case=copy.deepcopy(target);mutation(case)
        document['cases']=[] if name=='empty-selection' else [case]
        path=tmp/(name+'.json');path.write_text(json.dumps(document),encoding='utf-8')
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        p=subprocess.run([sys.executable,str(Path(__file__).with_name('check_offline_inspection.py')),str(exe),str(root),'--manifest',str(path),'--quick'],capture_output=True,text=True)
        assert p.returncode!=0,(name,p.stdout,p.stderr)
        assert hashlib.sha256(path.read_bytes()).hexdigest()==digest
        proof[name]='rejected; temporary expectation unchanged'
    # Independent Java observations also reject unknown/missing rows and semantic mutations.
    clone=tmp/'fixtures';shutil.copytree(root,clone)
    java=clone/'expected/offline-inspection.tsv';base=java.read_bytes()
    for name,mutated in [('java-value',base.replace(b'22,44,0',b'22,45,0')),
                         ('java-unknown',base.replace(b'end\t',b'unknown\t')),
                         ('java-completion',base.replace(b'complete\toffline-inspection\n',b''))]:
        java.write_bytes(mutated)
        p=subprocess.run([sys.executable,str(Path(__file__).with_name('check_offline_inspection.py')),str(exe),str(clone),'--quick'],capture_output=True,text=True)
        assert p.returncode!=0,name
        assert java.read_bytes()==mutated
        proof[name]='rejected; temporary observation unchanged'
    assert hashes(root)==before
    subprocess.run([sys.executable,str(Path(__file__).with_name('check_offline_inspection.py')),str(exe),str(root)],check=True)
assert hashes(root)==before
proof['baseline']='passed; original fixtures unchanged'
args.proof.parent.mkdir(parents=True,exist_ok=True)
args.proof.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
print(json.dumps(proof,indent=2))
