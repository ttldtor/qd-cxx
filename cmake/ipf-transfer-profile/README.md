# IPF transfer dependency source profile

[Русская версия](README.ru.md). English is authoritative.

This standalone profile builds **Boost 1.92.0 URL as a static library** and qualifies the selected
Beast HTTP/1 `basic_parser` with one contiguous caller-owned buffer. It does not enable a production
transfer target, URL loader, socket, TLS stream, coroutine or thread pool. The root build and the
independent IPF/feed dependency graphs remain unchanged. See the approved [transfer boundary](../../spec/ipf/TRANSFER.md).

[Provision/build/seal instructions](../../tools/deps/IPF-TRANSFER.md) are explicit and project-local.
Ordinary configure never acquires this dependency; the standalone profile defaults to OFF and does
not even enable a compiler in that mode.

## Pinned input and selected closure

[boost-1.92.0.sources.json](boost-1.92.0.sources.json) records the official archive URL/SHA256,
exact release/component commits, BSL-1.0 license and all **417** selected source/license hashes.
The archive SHA256 is `c4a3b310ddd2472416e091067166b0713be97c63f38c212c484ada022fd296ce`.
The selection is the union of actual compiler dependency lists from MSVC 19.42 x64 Release/MD and
GCC 13.3.0 x86-64 Debug ASan/UBSan, plus all URL `src` files, the license and version header.
All source files are compiled separately; discontinued `boost/url/src.hpp` is not used.
This is a finite closure for the recorded macros and probes, without a general minimality claim.
A later adapter needing another header/macro must extend and requalify the profile explicitly.
The provisioner extracts only exact listed archive paths, without Boost docs, examples or build scripts.

The static target exports `BOOST_ALL_NO_LIB=1`, `BOOST_URL_NO_LIB=1`, `BOOST_URL_STATIC_LINK=1` and
`BOOST_BEAST_HEADER_ONLY=1`; `BOOST_URL_SOURCE=1` is private to URL compilation. Windows additionally
sets `_WIN32_WINNT=0x0A00`, `WINVER=0x0A00`, `NOMINMAX` and `WIN32_LEAN_AND_MEAN`.
The supported profiles are single-configuration Windows MSVC 19.42 x64 Release/MD and
Linux GCC 13.3.0 x86-64 Debug with explicit `-fsanitize=address,undefined -fno-omit-frame-pointer`.
No arbitrary compiler/CRT/architecture compatibility is inferred.

## Ownership and qualification limits

The [probe](../../tests/conformance/ipf-transfer-dependency/parser_probe.cpp) checks content-length,
retained suffixes, no-body status, chunk/trailer completion, exact/exceeded body limits, header limit,
malformed status, conflicting content lengths, truncated EOF and caller replay of an unconsumed
partial header. URI probes check a borrowed input pointer, preserved encoded userinfo/query, port,
relative reference, malformed host/percent, missing scheme and embedded NUL rejection. A generic URI
parser accepts other schemes; transfer policy still must reject them before I/O.

A URL view borrows the caller's live input. These probes retain no view past that input's lifetime;
they establish no owning-input facade. Parser field/body borrows are consumed during callbacks.
The selected contiguous-buffer overload avoids the multi-buffer flattening convenience path.
Dynamic/multi-buffer wrappers may allocate. No universal zero-heap, bounded-RSS, parser-work,
HTTP acquisition, cancellation, credential, redirect or TLS-envelope qualification is claimed here.
Those production contracts remain separate work.

The installed private SDK contains selected sources/license, a static archive, relative CMake package
files and build metadata. A separate seal binds the actual archive and metadata hashes to the pinned
source manifest. This is per-build artifact identity: different build paths/toolchain patch levels
can change archive bytes. Reproducible preparation is not a promise of a universal portable binary hash.
A seal verifies local consistency in trusted directories, not authenticity of an untrusted delivery,
independent review, or proof that tests ran. Run the documented tests before sealing.

The [standalone consumer](../../tests/conformance/ipf-transfer-dependency/consumer/CMakeLists.txt)
uses only the installed package and exported private target. Relocation requires no original source
or build directory. Public library interfaces must not expose Boost types; this standalone SDK is an
internal dependency package, not a new public IPF API. Static linking alone does not prove ODR or
symbol isolation from another Boost image in an application.
