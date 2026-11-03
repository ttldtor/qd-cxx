# IPF source transfer dependency and ownership

[Русская версия](TRANSFER.ru.md). English is authoritative.

## Decision and current boundary

The selected transfer layer uses **Boost 1.92.0 Beast HTTP/1 parser headers and compiled static
Boost.URL**. Boost is a private dependency of a separate optional transfer layer. It must not
become a dependency, discovery step or fetch of the independent IPF value/local reader/writer
target or `qdcxx_feed`. This decision adds no URL loader, production API, dependency target or SDK
installation. The current local APIs remain described by [READER](READER.md) and [WRITER](WRITER.md).

The selection covers HTTP/HTTPS/file acquisition requirements. Compression and the live collector
remain separate implementations; their absence is not an accepted complete-client exclusion.
No proxy implementation, ambient proxy discovery or implicit cookie jar/replay is added by this decision.

## Requirements from the reference

Source authority is pinned QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`:

| Source | Relevant observable requirement |
| --- | --- |
| [InstrumentProfileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileReader.java) | Snapshot GET sets `X-Live: no`; explicit token overrides URL userinfo; HTTP success check requires 200; exposes Last-Modified |
| [InstrumentProfileConnection](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java) | `X-Live: yes/provided`, conditional If-Modified-Since/304, successful processing before timestamp update, distinct live completion |
| [URLInputStream](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/URLInputStream.java) | Local paths/file URLs and HTTP connections; explicit token precedence; Java default connect/read timeouts 30000/60000 ms come from ambient properties |
| [AuthToken](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/auth/AuthToken.java) | Basic uses UTF-8 then Base64; Bearer/custom tokens are also representable |

The reference's host:port shortcut resolves to `http://host:port/ipf/all.ipf.gz`; resolution is
distinct from compression support. Its thread interruption in connection.close is not proof of
prompt cancellation of blocked I/O or callback/native quiescence. Redirect behavior delegated to
the JDK is not assumed equivalent to a new client without production comparisons.

Native transfer must use explicit finite options, detached status/metadata and owned profile
prefixes. HTTP message completion, native retirement, parser EOF and an IPF completion marker are
distinct. A 304 means unchanged data, not a newly read complete empty snapshot. Errors after an IPF
marker cannot turn an incomplete/failed transfer into success. Conditional metadata must not
silently commit failed partial content. No automatic compression success is implied by a .gz URL.

## Why this dependency

Beast provides low-level HTTP/1 parsing rather than a complete acquisition client. Its
[exact-release parser source](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/basic_parser.hpp)
includes Boost.Asio buffer types: it is not a drop-in standalone-Asio socket library. The selected use is only
the pure parser/buffer vocabulary. Existing standalone Asio 1.38.2 and qualified
c-ares 1.34.9+qdcxx2 retain native ownership. Boost socket/TLS streams, HTTP read algorithms,
coroutines and thread pools are not selected.

The alternative was libcurl's multi interface. It supplies substantial acquisition behavior but
adds a compiled C SDK and a network/DNS/cache/global/callback ownership graph to qualify.
Its [global initialization](https://curl.se/libcurl/c/curl_global_init.html) and
[SSL context callback](https://curl.se/libcurl/c/CURLOPT_SSL_CTX_FUNCTION.html) do not establish
reuse of this project's qualified TLS engine or isolation of the same OpenSSL image.
The [curl license](https://curl.se/docs/copyright.html) is permissive and MIT-inspired; licensing
alone did not decide against it. The chosen boundary reduces new network-owner machinery while
retaining explicit adapter/provisioning work; this is an engineering judgment, not a benchmark.

## Exact source, license and provisioning

| Selected input | Identity |
| --- | --- |
| [Stable release](https://www.boost.org/releases/1.92.0/) | Boost 1.92.0 |
| [Official archive metadata](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz.json) | `boost_1_92_0.tar.gz`; SHA-256 `c4a3b310ddd2472416e091067166b0713be97c63f38c212c484ada022fd296ce` |
| Boost release commit | `afdfa32505af73e3d208144b3f623f0096cb62b6` |
| [Beast release source](https://github.com/boostorg/beast/tree/7c1e061f91e2ef542217b76286c314d006c0c8fc) | `7c1e061f91e2ef542217b76286c314d006c0c8fc` |
| [URL release source](https://github.com/boostorg/url/tree/a34f3a4a1a0e695f679453af71dfc050a59170e6) | `a34f3a4a1a0e695f679453af71dfc050a59170e6` |
| [License from exact release](https://raw.githubusercontent.com/boostorg/boost/boost-1.92.0/LICENSE_1_0.txt) | Boost Software License 1.0; retain required notices/license with distributed inputs/artifacts |

Boost.URL 1.92.0 requires a compiled library: the release
[`src.hpp`](https://raw.githubusercontent.com/boostorg/url/boost-1.92.0/include/boost/url/src.hpp)
explicitly discontinues the former one-TU integration. Do not provision it using older header-only
instructions. Its static archive, complete transitive source/header closure, license, compiler,
CRT flags and build options require explicit source/artifact hashes and qualification before use.
No archive acquisition, SDK build or native qualification was performed for this decision.

Provisioning must accept an explicit verified source/artifact location, reject wrong/missing
identity actionably, and avoid a mandatory globally installed Boost, package manager or hidden
download. The source archive digest identifies an input, not a qualified binary or proof of
bounded allocation. Selected ON profiles and installed/relocated consumers must bind the actual
MSVC 19.42 x64 Release/MD and GCC 13.3 x86-64 Debug/sanitizer artifacts. OFF must not discover/link/fetch
Boost or network SDKs through local IPF/feed targets. No global configuration is changed.

## HTTPS integration

HTTPS retains the existing [controlled TLS profile](../transport/TLS-RUNTIME.md) and
[resource/host requirements](../transport/TLS-RESOURCES.md): exact OpenSSL 3.5.9 image/configuration,
explicit trust and verified reference identity. No system-root/Schannel substitution, extra TLS
profile, verification bypass or plaintext fallback is selected.

Public TlsRuntime currently owns a QTP session, not a generic HTTP stream. Its private byte-engine
seam is a reuse opportunity, not a ready public loader. Implementation must qualify a bounded
protocol-neutral native owner/seam while preserving existing QTP owner behavior and the exact TLS
policy/WANT/closure path. A new transfer participates in the same controlled image's shared,
thread, history and failure reservations. Private contexts do not provide free isolation, and
shared host preflight/storage must not be silently duplicated or refunded.

## Source and credential ownership

Initial sources may use only the specified local path, file URI, HTTP or HTTPS forms. Unknown URI
schemes are rejected before acquisition/I/O, without fallback to a local path. Exact spelling,
local path/file URI mapping, Windows drive forms, source-shortcut behavior and canonicalization
must be qualified by the implementation against pinned comparisons; this decision defines no new
URL API or Windows path guessing rules. The local reader keeps ownership of its binary file.
After relative resolution, redirect targets may use only HTTP(S); file and all other schemes are
rejected before acquisition/I/O. A network redirect never permits opening a local file.

The selected HTTP credential boundary is a separate bounded owned Authorization provider for
Basic/Bearer/custom HTTP values. It does not alter the existing QTP Basic provider or promise
universal equivalence to every Java AuthToken value. Validate size, scheme/header syntax and
CR/LF before publication. Invoke application credential code only after verification of the
intended TLS origin. Anonymous HTTP is permitted; secret-bearing plaintext HTTP is rejected.

Initial HTTP(S) URL userinfo remains supported as an owned Basic credential source. Parse the bounded
caller input by borrow; extract only userinfo into separate owned secret storage, then construct
a transport/address representation without userinfo. Query remains opaque transport data; removing
userinfo does not make the URI safe to log. Do not retain an owning copy
of the full credential URL or a view into the caller URL after the call. Percent/colon/UTF-8 details
must follow pinned production observations before implementation claims; do not silently decode.

An explicit Authorization provider/token overrides initial userinfo. Provider failure does not
trigger fallback to userinfo. Redirect Location userinfo is rejected. A cross-origin redirect
does not inherit the initial secret or provider automatically; HTTPS-to-HTTP downgrade is rejected.
Origin is the tuple of scheme, canonical host and effective port, independently of the DNS address
or resolved route. Exact spelling and canonicalization require implementation qualification.
A credential may authorize only its intended still-live attempt. No full URI/query, userinfo, token or provider exception text
enters diagnostics/logs. Caller memory and secure erasure of ordinary C++ storage are not promised.

The operation retains its own stable state and provider lease during user code, invokes no user
code under owner locks, then rechecks public lifetime, attempt, cutoff and deadline before using
a result. A provider may request close or destroy/replace the public owner without authorizing a
stale attempt; no callback or work-buffer borrow may escape its documented lifetime.

## Bounds, retry work and cancellation

Separate finite limits must cover URI/Location/secret bytes, status/header/trailer bytes and counts,
wire/framing/chunk-extension bytes, payload bytes, parser/profile output, redirect/informational
response attempts, storage overlap and cumulative/turn work, plus existing DNS/native/TLS scopes.
Check caps before the corresponding allocation/copy/growth; report overflow/refusal without
treating allocator overhead as zero or promising a hard RSS/latency bound.

The selected [basic_parser source](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/basic_parser.hpp)
permits single-contiguous-buffer input and exposes header/body limits. Its
[buffer-sequence path](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/impl/basic_parser.hpp)
can allocate flattening storage. Use owned bounded contiguous input and bounded derived-field
storage; no claim of zero heap for the whole owner follows. Retain unconsumed structured prefixes
only within capacity. Header/body limits alone do not bound repeated retries, chunk metadata,
informational messages or all source bytes.

Charge each parser invocation and presented/examined slice conservatively, even if it accepts
zero bytes or fails. A need-more result without new input/EOF must not busy-loop over the same
prefix or refund work. EOF is delivered once at the valid boundary. Fixed URL storage and relative
resolution must be bounded before growth; borrowed parsed components/redirect references must not
outlive their stable input or survive mutation of that storage.

Request-close is an allocation-free, idempotent admission cutoff. Cancellation/terminal publication
and native/callback retirement are distinct; retain descriptor/handler/TLS/input/output/credential
owners until matching completion/drain. Cooperative progress pays explicit cleanup work; request-close
performs no inline join and promises no immediate quiescence or hard I/O deadline. Final owner
destruction may run the selected fallback drain and wait for host progress; observable quiescence
requires explicit progress/drain before release. Late completions cannot resume parsing,
invoke a stale provider or publish a successful complete snapshot.

## Implementation qualification required before availability

The next implementation must preserve the accepted local APIs and prove:

- Exact dependency/source/license/CRT artifacts, selected ON and dependency-free OFF graphs;
  separate self-contained installed/physically relocated consumers on both required platforms.
- Bounded protocol-neutral native/TLS seam and all affected QTP regressions; existing controlled
  host/shared reservations remain truthful through final retirement.
- Real controlled HTTP 200/HTTPS 200/file success, relative/bounded redirects, conditional 304,
  Last-Modified and distinct framing/IPF completion; compression remains a separate pipeline stage.
- Explicit token versus userinfo, percent/colon/UTF-8 production facts, Basic/Bearer/custom syntax,
  redaction, redirect userinfo/cross-origin/downgrade and failed-provider no-fallback cases.
- Partial headers/body/EOF, conflicting framing, header/trailer/chunk/work/output limits,
  failed allocation, cancellation in each phase and actual callback/native lifetime boundaries.
- MSVC 19.42 x64 Release/MD and GCC 13.3 Debug ASan/UBSan checks on final source, independent review
  and actual generated documentation/link/source-hash binding.

This is an accepted dependency/ownership decision with source/document verification. It is not an
executed HTTP/TLS runtime, SDK qualification, new API, proxy/compression/collector implementation,
release or complete-client acceptance. Future unsupported cases must be explicit; they are not
silently approved exclusions.
