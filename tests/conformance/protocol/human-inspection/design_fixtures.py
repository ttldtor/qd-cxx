"""One-time designed fixture construction; never run by verification; no C++ output consulted."""
from pathlib import Path
import argparse
import json
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True,help='New directory; existing destinations are rejected')
ROOT=parser.parse_args().output
ROOT.mkdir(parents=True,exist_ok=False)
OLD=Path(__file__).resolve().parents[3]/'fixtures/protocol/offline-inspection'
def quote(text):
    out='"';i=0
    while i<len(text):
        c=text[i];v=ord(c);i+=1
        if c in {'"','\\','\t','\r','\n'}: out+={'"':'\\"','\\':'\\\\','\t':'\\t','\r':'\\r','\n':'\\n'}[c]
        elif 0xd800<=v<=0xdbff and i<len(text) and 0xdc00<=ord(text[i])<=0xdfff:
            out+=chr(0x10000+((v-0xd800)<<10)+(ord(text[i])-0xdc00));i+=1
        elif v<32 or 127<=v<=159 or v in [0x61c,0x200e,0x200f] or 0x2028<=v<=0x202e or 0x2066<=v<=0x206f or 0xd800<=v<=0xdfff: out+=f'\\u{v:04X}'
        else:out+=c
    return out+'"'
def fromhex(s):return '' if s=='-' else bytes.fromhex(s).decode('utf-16-be','surrogatepass')
HEADER='qd-qtp-inspect human 1 profile=oracle-sre-v1 identities=local\n'
def human(tsv):
    out=''
    for line in tsv.splitlines():
        r=line.split('\t')
        if r[0]=='format':out+=HEADER
        elif r[0]=='frame':
            _,i,a,b,t,p,n=r;name={'-':'EMPTY_HEARTBEAT','2':'DESCRIBE_RECORDS','15':'STREAM_DATA'}.get(t,'UNSUPPORTED')
            out+=f'Frame {i}: {name} ({t}) bytes=[{a},{b}) payload={p} size={n}\n'
        elif r[0]=='record':
            _,fr,i,local,name,symbol,vs=r
            labels={'0':['S.v'],'1':['R.y','R.x','R.z'],'2':[]}[local]
            fields='' if vs=='-' else ', '.join(f'{l}={v}' for l,v in zip(labels,vs.split(','),strict=True))
            out+=f'Record {i}: frame={fr} local={local} name={quote(fromhex(name))} symbol={quote(fromhex(symbol))} fields={{{fields}}}\n'
        else:
            _,b,f,r,c,w=r;out+=f'Complete: bytes={b} frames={f} records={r} charged-records={c} wire-fields={w}\n'
    return out
cases=[]
for c in json.loads((OLD/'designed.json').read_text())['cases']:
    if '--max-output' in c['args']:continue
    cases.append(dict(name='legacy-'+c['name'],input='../offline-inspection/'+c['input'],args=c['args'],exit=c['exit'],stderr=c['stderr'],stdout=human(c['stdout'])))
# Exact UTF-16 stimuli are explicitly designed; modified UTF-8 retains isolated units.
def compact(n):
    assert 0<=n<8192
    return bytes([n]) if n<64 else bytes([0x80|(n>>8),n&255])
def frame(t,p):
    b=compact(t)+p;return compact(len(b))+b
D=bytes.fromhex('080209015301017608')
subjects={
'ascii':'ABC', 'empty':'', 'cyrillic':'Привет', 'supplementary':'\ud83d\ude00',
'lone-high':'\ud800', 'lone-low':'\udfff', 'surrogate-order':'\ud800\ud800\udc00\udfff',
'lookalikes':r'\uD800\u0000\n\t', 'quotes':'"\\', 'injection':'\0\nComplete: bytes=0\r\tincomplete\x1b[31m',
'c0':''.join(map(chr,range(32))), 'del-c1':''.join(map(chr,range(127,160))),
'line-bidi':''.join(map(chr,[0x61c,0x200e,0x200f,*range(0x2028,0x202f),*range(0x2066,0x2070)])),
'utf8-boundaries':''.join(map(chr,[0x20,0x7e,0xa0,0x7ff,0x800,0xd7ff,0xe000,0xffff,0x10000,0x10ffff])),
'no-normalization':'é e\u0301',
}
for name,s in subjects.items():
    b=s.encode('utf-8','surrogatepass');p=b'\xfc'+compact(len(b))+b+b'\x09\x01';f=frame(15,p);wire=D+f
    path=name+'.qtp';(ROOT/path).write_bytes(wire)
    out=HEADER+'Frame 0: DESCRIBE_RECORDS (2) bytes=[0,9) payload=2 size=7\n'
    out+=f'Frame 1: STREAM_DATA (15) bytes=[9,{len(wire)}) payload={len(wire)-len(p)} size={len(p)}\n'
    out+=f'Record 0: frame=1 local=0 name="S" symbol={quote(s)} fields={{S.v=1}}\n'
    out+=f'Complete: bytes={len(wire)} frames=2 records=1 charged-records=1 wire-fields=1\n'
    cases.append(dict(name=name,input=path,args=[],exit=0,stderr='',stdout=out))
(ROOT/'designed.json').write_text(json.dumps({'format':'qdcxx-human-designed-v1','provenance':'Designed presentation policy; historical TSV projections and explicit Unicode stimuli, not Java human output.','cases':cases},ensure_ascii=True,indent=2)+'\n',encoding='utf-8',newline='\n')
print(len(cases),'designed human cases')
