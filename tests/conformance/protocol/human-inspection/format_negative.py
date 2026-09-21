"""Check pinned formatting rejects a temporary mismatch without rewriting it, then accepts baseline."""
from pathlib import Path
import hashlib
import json
import subprocess

path = Path('.work/human-format-negative.cpp')
path.write_text('int main(){return 0;}\n', encoding='utf-8', newline='\n')
before = hashlib.sha256(path.read_bytes()).hexdigest()
base = ['pwsh', '-NoProfile', '-File', 'tools/format/Format-Cpp.ps1']
result = subprocess.run([*base, '-Mode', 'Check', '-Files', str(path)], capture_output=True, text=True)
assert result.returncode != 0
assert hashlib.sha256(path.read_bytes()).hexdigest() == before
subprocess.run([*base, '-Mode', 'Format', '-Files', str(path)], check=True)
formatted = path.read_bytes()
subprocess.run([*base, '-Mode', 'Check', '-Files', str(path)], check=True)
assert path.read_bytes() == formatted
subprocess.run([*base, '-Mode', 'Format', '-Files', str(path)], check=True)
assert path.read_bytes() == formatted
print(json.dumps({'formatMismatch': 'rejected without rewriting', 'baseline': 'passed', 'pipeline': 'idempotent'}))
