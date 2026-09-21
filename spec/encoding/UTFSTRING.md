# Bounded C++ UTFString contract

[Русская версия](UTFSTRING.ru.md). English is authoritative.

QDCXX-011 implements the approved bounded policy over the unchanged Java QD 3.355
[primitive evidence](PRIMITIVES.md). API: [utf_string.hpp](../../include/qd/encoding/utf_string.hpp).
This is UTFString only, not a general Unicode conversion API.

## Representation and lifetime

`readUtfString(span<const byte>, UtfStringLimits)` returns `UtfStringResult` with `status`,
`consumed`, and `optional<u16string> value`. Interpret value only on `OK`: disengaged means
Java null; engaged empty means empty. Every error has disengaged value. The result owns its
UTF-16 units. `writeUtfString(optional<u16string_view>, span<byte>, UtfStringLimits)` borrows
input/output only during the call; their storage must not overlap. No wchar_t reinterpretation,
normalization, replacement characters or surrogate repair occurs.

The canonical writer preserves isolated surrogates as three-byte units and combines valid pairs
into four-byte code points. The reader preserves exact units, accepting measured overlong/CESU
forms. Null is compact long -1; empty is zero. Other prefixes count body bytes, not UTF-16 units.
The existing compact codec handles all prefixes without changing its integer behavior.

## Errors, positions and resource policy

Callers must supply both finite `size_t` budgets: `bodyBytes` and `codeUnits`. There is no default
argument and no unlimited sentinel. These are operational limits, not Java wire limits or an
exact allocator-footprint guarantee. Zero budgets allow null and empty.

Reader precedence:

1. Incomplete compact prefix: `INSUFFICIENT_INPUT`, consuming the supplied prefix.
2. Length outside -1..8589934588: `INVALID_LENGTH`, after the prefix.
3. Null succeeds; otherwise a body-byte budget excess gives `RESOURCE_LIMIT` after the prefix,
   without examining the body or allocating proportional to the declared length.
4. Read a character's full tail before checking continuation bits, as Java does. An unavailable
   tail gives `INSUFFICIENT_INPUT` at span end; a bad lead/tail or point above 0x10FFFF gives
   `INVALID_ENCODING`. Overlong encodings and surrogate code points are accepted.
5. Before appending a unit/pair, check its complete code-unit budget. Excess gives `RESOURCE_LIMIT`
   after the examined character, with no partial value. A pair never partially consumes the budget.
6. A completed character extending beyond the declared body gives `INVALID_ENCODING` after the
   append-budget check. This deliberate budget precedence is C++ policy, not a Java measurement.

The primitive does not clip input to the declared body: `01c280` fails at 3, `01c2` is incomplete
at 2, and `014142` succeeds at 2 with one trailing byte. It never reads outside the supplied span.
There is no rollback/resumable-state promise. A future frame parser must supply a frame-bounded span.
The owning reader grows only from examined, budgeted units; `std::bad_alloc` or `std::length_error`
may propagate and no partial value escapes. It is intentionally not noexcept.

Writer preflight checks a lower bound on wire length from input units, then the input-unit budget,
then counts canonical bytes with subtraction-based overflow checks against 8589934588, then checks
the body-byte budget and total span capacity. A too-large length gives `INVALID_LENGTH`; a policy
excess gives `RESOURCE_LIMIT`; insufficient representable total/capacity gives `INSUFFICIENT_OUTPUT`.
Every failure writes zero and leaves the entire output unchanged. Success leaves bytes after
`written` untouched. The writer allocates nothing and is noexcept. No fabricated huge string_view
is used to test arithmetic: it would violate the valid-storage precondition.

The shared `Status` adds UTF outcomes; existing compact status values, noexcept guarantees,
allocation-free behavior and results remain unchanged.

## Reuse decision, made before implementation

Storage and conversion are separate. C++23 `u16string`, `u16string_view`, `optional`, `span` and
fixed-width arithmetic are reused, as is the existing compact codec. Containers do not transcode.
C++23's deprecated `codecvt_utf8_utf16`/`wstring_convert` are conversion facilities, but supply
neither this envelope nor the resource/atomic-output/Java-position contract. They were assessed
against the [C++23 draft N4950](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf)
and executed with GCC 13.3/libstdc++. No platform-dependent wchar_t conversion is selected.

| Candidate and official versioned source | Reader | Writer | Costs and decision |
| --- | --- | --- | --- |
| [UTF-CPP 4.0.6](https://github.com/nemtrif/utfcpp/tree/v4.0.6), `source/utf8/checked.h`, `core.h`, `unchecked.h` | Checked path rejects overlong and surrogate values; `validate_next` restores its iterator on error, unlike Java consumption. Unchecked `next` lacks an end parameter and cannot handle untrusted truncated input safely without our own validation. | Checked conversion rejects isolated surrogates. Unchecked conversion drops a trailing high surrogate and assumes a following unit completes a pair. | Header-only, BSL-1.0 notices required for copied headers; no runtime/transitive production library. Iterator API can avoid allocations, string wrappers allocate. Portable C++98+ source and documented MSVC support; actual probe only GCC. A bounds/error/envelope adapter plus writer segmentation would retain the custom work. Rejected. |
| [simdutf 5.2.8](https://github.com/simdutf/simdutf/tree/v5.2.8), `include/simdutf/implementation.h`, README | Validating conversion rejects the measured nonstandard forms. Valid-input APIs cannot be used on those inputs or malformed prefixes. Error position is not Java consumed position. | Validating UTF-16 conversion rejects isolated surrogates. | MIT or Apache-2.0; MIT choice would require copyright/license notice. Compiled library, CPU dispatch and added binary/build cost; caller-owned output, no required production dependencies. Officially supports GCC, LLVM Clang and Visual Studio; ClangCL candidate integration was not tested. Tests/tools disabled for the actual GCC probe. Rejected. |

Both libraries can convert a valid scalar subset, in either direction. That does not justify a
second path: the required envelope, byte/count preflight, atomic writes, incremental budgets and
nonstandard/malformed parsing remain custom. Splitting/repairing input just to reuse conversion
would add maintenance and extra passes without removing the critical logic. Unchecked UTF-CPP
append primitives are only a few shifts and do not offset a production dependency. We therefore
implement a small QD-specific codec; no third-party implementation code/types enter the API or
production build. No performance claim or speculative fast path is made.

Executed decision probes use unchanged `S-supplementary`, `S-high`, `S-low`, `F-overlong2/3/4`,
`F-cesu-pair`, `F-surrogate4` observations. [Initial library probe](../../tests/conformance/encoding/utf-reuse-initial.log)
preceded implementation. [Final exact comparison](../../tests/conformance/encoding/utf-reuse.log)
compares bytes or UTF-16 units for all three candidates: only S-supplementary matches. In particular,
libstdc++ not throwing for a trailing high surrogate is not success: it does not produce Java bytes.
This bounded probe does not claim a complete candidate-platform matrix.

## Reproduce candidate probes

[Generator](../../tests/conformance/probe_utf_reuse.py) reads the original TSV and checks archive
SHA-256 before generating a separate ignored CMake project. No candidates are fetched by the main
build. Release archives and extracted notices stay under `.work/utf-reuse`; offline reruns require
that cache. Exact archive integrity:

- UTF-CPP v4.0.6: `6920a6a5d6a04b9a89b2a89af7132f8acefd46e0c2a7b190350539e9213816c0`.
- simdutf v5.2.8: `2706f1bef85a6d8598f82defd3848f1c5100e2e065c5d416d993118b53ea8d77`.

From WSL at the repository root (acquisition requires network):

```bash
mkdir -p .work/utf-reuse
curl -L https://codeload.github.com/nemtrif/utfcpp/tar.gz/refs/tags/v4.0.6 -o .work/utf-reuse/utfcpp.tar.gz
curl -L https://codeload.github.com/simdutf/simdutf/tar.gz/refs/tags/v5.2.8 -o .work/utf-reuse/simdutf.tar.gz
python3 tests/conformance/probe_utf_reuse.py
tar -xf .work/utf-reuse/utfcpp.tar.gz -C .work/utf-reuse
tar -xf .work/utf-reuse/simdutf.tar.gz -C .work/utf-reuse
python3 tools/run_utc.py --log .work/utf-reuse/configure.log -- cmake -S .work/utf-reuse -B .work/utf-reuse/build -G Ninja -DCMAKE_BUILD_TYPE=Release
python3 tools/run_utc.py --log .work/utf-reuse/build.log -- cmake --build .work/utf-reuse/build -j 4
python3 tools/run_utc.py --log .work/utf-reuse/probe.log -- .work/utf-reuse/build/probe
```

## Verification limits

[Coverage](../../tests/conformance/encoding/COVERAGE.md) and
[validation](../../tests/conformance/encoding/VALIDATION.md) report executed configurations,
negative checks and unchanged evidence. Huge accepted allocations, allocator-failure injection,
32-bit execution, other BufferedInput implementations, other Unicode APIs, complete Unicode/QTP,
PentaCodec, importer and transport are not covered. The API remains provisional.
