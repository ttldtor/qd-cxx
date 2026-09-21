"""Capture current build/source metadata and reproduce designed artifacts into a fresh temporary directory."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[4]
REPORT = Path(__file__).resolve().parent
FIXTURE = ROOT / 'tests/fixtures/protocol/human-inspection'


def dump(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


profiles = {}
for name in ['msvc', 'clang', 'linux-debug', 'linux-release', 'utf-asan', 'human-production-ON', 'human-production-OFF']:
    path = ROOT / '.work' / name
    profiles[name] = {'cacheEntries': [s for s in (path / 'CMakeCache.txt').read_text().splitlines()
        if re.match(r'(CMAKE_(CXX_(COMPILER|FLAGS)|BUILD_TYPE|GENERATOR)|BUILD_TESTING|FETCHCONTENT_FULLY_DISCONNECTED|QD_BUILD_QTP_INSPECT)[^=]*=', s)], 'compiler': []}
    for compiler in (path / 'CMakeFiles').glob('*/CMakeCXXCompiler.cmake'):
        profiles[name]['compiler'] += [s for s in compiler.read_text().splitlines() if any(x in s for x in
            ['CMAKE_CXX_COMPILER_ID ', 'CMAKE_CXX_COMPILER_VERSION ', 'CMAKE_CXX_COMPILER_ARCHITECTURE_ID '])]
profiles['productionHasDeps'] = {v: (ROOT / ('.work/human-production-' + v) / '_deps').exists() for v in ['ON', 'OFF']}
assert not any(profiles['productionHasDeps'].values())
dump(REPORT / 'build-profile.json', profiles)
with tempfile.TemporaryDirectory(prefix='human-designed-', dir=ROOT / '.work') as temporary:
    destination = Path(temporary) / 'fresh'
    subprocess.run([sys.executable, str(REPORT / 'design_fixtures.py'), '--output', str(destination)], check=True)
    artifacts = [p for p in FIXTURE.iterdir() if p.suffix in ['.json', '.qtp']]
    for path in artifacts:
        assert path.read_bytes() == (destination / path.name).read_bytes(), path
paths = [ROOT / 'CMakeLists.txt', ROOT / 'tools/format/Format-Cpp.ps1', ROOT / 'tools/run_utc.py']
for directory, pattern in [('include', '*.hpp'), ('src', '*.cpp'), ('tools/qtp-inspect', '*.hpp'), ('tools/qtp-inspect', '*.cpp')]:
    paths += list((ROOT / directory).rglob(pattern))
paths += list((ROOT / 'tests/conformance').glob('*.cpp'))
paths += [ROOT / 'tests/conformance' / name for name in ['check_human_inspection.py', 'check_offline_inspection.py', 'check_offline_negative.py']]
paths += list(REPORT.glob('*.py'))
dump(REPORT / 'source-sha256.json', {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)})
dump(REPORT / 'fixture-sha256.json', {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(artifacts)})
print(json.dumps({'reproducedDesignedArtifacts': len(artifacts), 'sourceFiles': len(paths), 'productionHasDeps': profiles['productionHasDeps']}))
