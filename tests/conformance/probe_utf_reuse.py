"""Generate eight reuse probes from unchanged Java TSV (run from repository root).

Sources stay in ignored .work/utf-reuse. Acquire releases as documented in UTFSTRING.md.
This is a rejection probe, not the production codec or a replacement oracle.
"""
import hashlib
from pathlib import Path

ROOT = Path('.work/utf-reuse')
ARCHIVES = {
    'utfcpp': '6920a6a5d6a04b9a89b2a89af7132f8acefd46e0c2a7b190350539e9213816c0',
    'simdutf': '2706f1bef85a6d8598f82defd3848f1c5100e2e065c5d416d993118b53ea8d77',
}
WRITERS = {'S-high', 'S-low', 'S-supplementary'}
READERS = {'F-overlong2', 'F-overlong3', 'F-overlong4', 'F-cesu-pair', 'F-surrogate4'}
for name, expected in ARCHIVES.items():
    if hashlib.sha256((ROOT / (name + '.tar.gz')).read_bytes()).hexdigest() != expected:
        raise RuntimeError('Archive integrity mismatch: ' + name)

rows = []
for family in ['strings', 'forms']:
    for line in Path('tests/fixtures/encoding/expected/' + family + '.tsv').read_text().splitlines():
        row = line.split('\t')
        if len(row) > 4 and ((row[0] == 'writer' and row[1] in WRITERS) or
                             (row[0] == 'reader' and row[1] in READERS)):
            rows.append(row)
if len(rows) != 8:
    raise RuntimeError('Missing probe observations')

source = '''#include <codecvt>
#include <locale>
#include <utf8.h>
#include <simdutf.h>
#include <iostream>
#include <string>
int main() { int failures = 0;
'''
for row in rows:
    writer = row[0] == 'writer'
    units_hex = row[3] if writer else row[5]
    units = ','.join('0x' + units_hex[i:i+4] for i in range(0, len(units_hex), 4))
    body_hex = (row[4] if writer else row[3])[2:]
    body = ''.join('\\x' + body_hex[i:i+2] for i in range(0, len(body_hex), 2))
    source += '{ std::u16string units={' + units + '}; std::string body("' + body + '",' + str(len(body_hex)//2) + ');\n'
    source += 'bool standard=false, checked=false, simd=false;\n'
    if writer:
        source += '''try { std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> cv;
            standard = cv.to_bytes(units) == body; } catch(const std::exception &) {}
        try { std::string out; utf8::utf16to8(units.begin(),units.end(),std::back_inserter(out));
            checked = out == body; } catch(const std::exception &) {}
        char buffer[32]; auto n = simdutf::convert_utf16_to_utf8(units.data(),units.size(),buffer);
        simd = n != 0 && std::string(buffer,n) == body;
'''
    else:
        source += '''try { std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> cv;
            standard = cv.from_bytes(body) == units; } catch(const std::exception &) {}
        try { std::u16string out; utf8::utf8to16(body.begin(),body.end(),std::back_inserter(out));
            checked = out == units; } catch(const std::exception &) {}
        char16_t buffer[32]; auto n = simdutf::convert_utf8_to_utf16(body.data(),body.size(),buffer);
        simd = n != 0 && std::u16string(buffer,n) == units;
'''
    expected_match = 'true' if row[1] == 'S-supplementary' else 'false'
    source += 'std::cout << "' + row[1] + ' Java-match: standard=" << standard << " utfcpp=" << checked << " simdutf=" << simd << "\\n";\n'
    source += 'failures += standard != ' + expected_match + ' || checked != ' + expected_match + ' || simd != ' + expected_match + '; }\n'
source += 'return failures; }\n'
(ROOT / 'probe.cpp').write_text(source)
(ROOT / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
project(reuse LANGUAGES CXX)
set(SIMDUTF_TESTS OFF CACHE BOOL "" FORCE)
set(SIMDUTF_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(simdutf-5.2.8)
add_executable(probe probe.cpp)
target_include_directories(probe PRIVATE utfcpp-4.0.6/source)
target_link_libraries(probe PRIVATE simdutf)
target_compile_features(probe PRIVATE cxx_std_23)
''')
