# IPF URL/source reader

[Русская версия](URL-READER.ru.md). English is authoritative.

## Optional target and simple use

The optional qdcxx_ipf_transfer target provides InstrumentProfileReader in
qd/ipf/instrument_profile_url_reader.hpp. It combines the existing
[IPF parser](READER.md), bounded private HTTP framing and
[byte owners](../transport/BYTE-OWNER.md). The local qdcxx_ipf functions and qdcxx_feed remain independent
of Boost. BUILD_IPF_TRANSFER requires explicit IPF, CLIENT, RUNTIME and TLS enablement and an
absolute ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK naming the qualified sealed SDK.
There is no download, package-manager or ambient SDK fallback.

Create the standalone owner on main before application threads:

    auto created = ipf::InstrumentProfileReader::create(options);
    if (!created)
        return report(created.error());
    auto reader = std::move(*created);
    std::stop_source cancellation;
    auto result = reader->loadURL(source, request, cancellation.get_token());

loadURL drives finite native/HTTP/IPF turns and native retirement internally. The application has
no work/poll loop. Another application thread may request_stop on the stop source; it never calls
the facade or an SDK. All facade operations and destruction remain on the creator thread.
No worker, callback from a native handler, proxy discovery or cookie jar is created.

The create(RuntimeBootstrap&, options) overload borrows an explicit same-linked-instance host
bootstrap on its creator thread. Create that host on main before application threads; retain it
through facade destruction and any active operation drain. A caller combining the feed runtime
and source reader uses this explicit host profile at startup. The owned bootstrap hidden inside
an existing ApiRuntime is not exported or reverse-shared.

HTTPS requires explicit ordinary trustPem, qualified TLS limits/host maxima, SDK/resident allowances
and positive reader-lifetime tlsWork. DNS names require the explicit numeric DNS servers/port.
There is no system trust/default resolver or plaintext fallback. See
[TLS resources](../transport/TLS-RESOURCES.md) for the same-image embedding obligations.

## Reading compressed snapshots

The same loadURL call reads plain, gzip and ZIP sources; the filename is not a selector.
ZIP skips directories and returns the first file's IPF data. HTTP Content-Encoding:gzip selects
one gzip layer; absent/identity coding selects by content magic. Nested envelopes and other HTTP
codings are rejected rather than decoded recursively.

Inspect result.envelope for the selected kind, admitted encoded/decoded byte counts, finite work
and selected container completion. A gzip/ZIP trailer failure returns CONTAINER_FAILURE while
retaining whole profiles already accepted by the IPF parser. envelope.finished, httpFinished,
nativeRetired and ipf.finished/completionSeen report different boundaries. A COMPLETE marker
alone does not make a corrupt compressed response successful.

limits.envelope sets finite inputBytes, outputBytes, calls, workBytes, inflaterBytes, metadataBytes
and entries. Output includes skipped directory payload. The compression-capable reader reserves
its fixed decoder plus the entire inflater allowance before loading, including for plain input.
See [container contract](COMPRESSION.md) and [ZIP interoperability decision](../../docs/decisions/ADR-0007-ipf-container-interoperability.md)
for supported forms, Java runtime differences and resource units.

## Sources and credentials

Sources are local paths, qualified file URIs, HTTP or HTTPS. The host:port shorthand with a valid
port in1..65535 expands to http://host:port/ipf/all.ipf.gz and preserves the query. The compression envelope is selected from content, independently of that suffix. File URIs admit empty/localhost authority, no userinfo/port/query/fragment,
and bounded percent-decoded UTF-8 path bytes without NUL. Windows drive and drive-relative forms
follow the reference's Windows-only drive adjustment after shorthand classification. Unknown URI
schemes fail before acquisition/I/O without local fallback.

HTTP address syntax uses Boost.URL validation and fixed8192-byte backing. Initial userinfo is
separate owned Basic data, split at the first literal colon. Percent sequences remain literal
credential bytes; they are not decoded. Literal UTF-8 userinfo is separated by bounded borrow
before strict validation of the sanitized address; this does not add IRI host/path conversion.
An explicit owned Authorization value or provider overrides initial userinfo. A selected provider
failure never falls back to it. Basic joins exact UTF-8 user/password bytes before Base64;
Bearer/custom values have explicit finite syntax/size checks.

Secret-bearing plaintext HTTP is rejected. An HTTPS provider is called only after verification of
the intended origin, on the creator, outside native-owner locks. Its destination and sanitized
origin views live only during that callback. Returned bytes are validated before publication.
Provider captures, arbitrary callback work and last-release destructor work are application
preconditions, outside library-owned memory/time bounds.

The provider may cancel, destroy or replace its facade. The active call has a stable operation/host
lease and does not access the facade afterward. On return it rechecks public lifetime, attempt,
cutoff, stop token and deadline. A stale result cannot authorize the old or replacement operation.
Credentials, provider lease, buffers, parser, TLS/native owners and bootstrap survive until matching
native retirement.

Redirects resolve encoded RFC3986 components with bounded merge/dot removal and repeat URL validation.
Encoded dots/slashes are not decoded. Only HTTP(S) redirect targets are allowed; userinfo, file/other
schemes and HTTPS downgrade are rejected before I/O. Origin is canonical scheme/host/effective port,
not a DNS address. Crossing origin permanently drops the initial token/provider and conditional
metadata for that load. No secret is automatically replayed across origin.

No full URI/query, userinfo, token or provider exception text enters a diagnostic. Sanitizing
userinfo does not make a query log-safe. Ordinary C++ storage and caller memory have no secure-erasure
guarantee.

## Separate outcome and completion evidence

| Evidence | Meaning |
| --- | --- |
| SUCCESS | Source/HTTP framing and parser EOF succeeded under the selected IPF policy, with native retirement observed |
| NOT_MODIFIED | An explicit conditional request received304; no new parser snapshot, complete flag or metadata commit |
| CANCELLED | Stop token, creator cancel or facade retirement cut off admission before success |
| FAILED | First typed source/native/TLS/HTTP/IPF/work/resource cause, retaining whole earlier profiles |
| httpFinished | One valid final HTTP framing boundary; an IPF marker alone never sets it |
| ipf.finished / completionSeen / complete | Existing independent parser EOF/marker/policy evidence |
| nativeRetired | Owned native handlers/borrows have drained before return |
| lastModified / metadataCommitted | Parsed candidate HTTP date versus successful200 content metadata commit |

Snapshot GET sends X-Live:no and Accept-Encoding:identity. X-Live:provided is observed as metadata;
it does not implement live collection.304 without an explicit conditional request is a status
failure. Last-Modified accepts the three HTTP-date forms; conditional serialization emits IMF-fixdate.
The obsolete two-digit year form uses the HTTP fifty-year interpretation rule.
Malformed dates do not become a committed timestamp.

Completion markers do not stop the IPF parser. Missing COMPLETE follows the existing ALLOW/REQUIRE
policy. A truncated HTTP/TLS close after a marker remains transfer failure with the owned profile
prefix. Candidate metadata never commits failed partial content. Native EOF, HTTP framing completion,
IPF EOF, marker admission and TLS close_notify are distinct.

## Finite accounting and retirement

All limits are per instance/load and checked before their corresponding growth/copy/publication.
The defaults are configurable; independent bounds can refuse a source before another bound is reached:

| Scope | Default |
| --- | --- |
| Source / Location / Authorization | 8192 bytes each |
| Header / fields; trailer / fields | 65536 /128;16384 /64 |
| Payload / plaintext HTTP wire / framing overhead | 67108864 /83886080 /8388608 bytes |
| Chunk extension / cumulative extension | 4096 /1048576 bytes |
| Redirects / informationals / HTTP attempts | 5 /8 /6; no connection retry |
| HTTP calls / presented-copy work / internal turns | 1000000 /268435456 bytes /1000000 |
| Contiguous input / whole request | 131072 /32768 bytes |
| Transfer-owned reservation / cleanup turns | 16777216 bytes /256 reserved turns |
| Overall / connect / idle-read / write | 300 /30 /60 /60 seconds |
| Handshake / TLS close / cadence | 30 /1 seconds /1 millisecond |

The transfer reservation includes fixed host/operation/parser-address structures, bounded windows,
selected secret overlap, settings and a capped shared host/control allocation. IPF logical
parser/profile output, native/TLS/process reservations and allocator bookkeeping remain separate.
There is no hard RSS or latency claim.

The C native resident allowance is96MiB, including the existing finite DNS profile envelope.
The byte-work default is64GiB of conservative logical preparation/copy units, not allocated storage.
DNS units default to finite UINT64_MAX credits: each admitted existing DNS entry debits its measured
worst-entry units. This is a finite counter, not an unlimited sentinel. Independent DNS calls,
packets/bytes, HTTP attempts, internal turns and deadlines still bound progress. These whole-load
grants survive redirects without refund or renewal. HTTPS resident funding must include the declared
native allowance together with its own TLS grants/history; the host does not silently fund that gap.

Profile-vector requested growth is clamped to the unchanged IPF profile bound; array/text relocation
work is charged before growth. Platforms with throwing move preparation copy the old vector prefix.
Old/candidate profile text stays within twice the logical output-unit envelope; failed allocation
retains the earlier owned prefix. This does not increase IpfReadLimits.

Cumulative wire/framing/parser/copy/attempt counters survive redirects and informational messages.
Zero-progress parser attempts are charged; need-more without new bytes/EOF cannot busy-loop.
Cleanup steps/handlers are partitioned from the original nativeWork, never copied into a new grant.
Success waits for observed native/TLS retirement. Exhausted cleanup grants produce WORK_LIMIT and
retain the existing safe fallback drain; request-only cancel does not promise immediate quiescence.

Each HTTPS child reuses existing F+R admission, H history, E/close grants and same-image
thread/failure maxima. Reader-lifetime TLS credits are debited across loads/redirects. No redirect,
cancel, parser reset or child retirement refunds/reinitializes shared SDK history or host storage.
Keep host maxima truthful across cooperating and foreign SDK users through final retirement.

Live collection remains a separate pipeline implementation. Supported source envelopes are defined in [COMPRESSION](COMPRESSION.md).

## References

Behavior comparisons use the pinned Java production sources in [TRANSFER](TRANSFER.md), including
its snapshot200 versus connection304 distinction and the explicit C credential/redirect boundary.
The wire rules are [RFC9110](https://www.rfc-editor.org/rfc/rfc9110.html),
[RFC9112](https://www.rfc-editor.org/rfc/rfc9112.html) and
[RFC3986 resolution vectors](https://www.rfc-editor.org/rfc/rfc3986.html#section-5.4).

Borrowed creation retains stable bootstrap identity rather than the wrapper address. The application
may move that bootstrap facade, including during an authorization callback, and destroy its moved-from
wrapper. Retain the moved-to owner through reader/operation retirement. Every later load/redirect
acquires a child from the same stable lease and original bounded borrow domain.
Local acquisition accepts regular files only. POSIX uses nonblocking open followed by fstat on
that opened descriptor; Windows verifies the opened disk-file handle and rejects directories.
A symlink to a regular file is supported. FIFO, pipe and device sources are rejected before read;
a pathname-only precheck is insufficient. Native filesystem acquisition and regular-file reads
still depend on filesystem/OS progress and have no hard scheduling or latency bound.

Literal source/userinfo and decoded file-URI paths require complete UTF-8 scalars: malformed,
overlong, surrogate and out-of-range sequences fail before I/O. ASCII percent triplets in userinfo
remain literal credential bytes. Authorization factories check whole finite size before scanning.

The transfer resident allowance includes the simultaneous fixed source-return/redirect builders,
borrowed parsing views, retained Location copy and native open-path workspace. Return/move elision
is not assumed. This is conservative requested backing, separately from allocator bookkeeping/RSS.

After a native first cause, ordinary TLS polling stops: it could accept or submit pending request
output. Only plaintext already verified and cached is copied and consumed without an SDK call;
unverified ciphertext fails closed. Earlier detached profiles remain available. Plain completed input uses the dedicated cache-only raw drain; no ordinary owner poll follows a cause.
Native-error plain prefixes use a dedicated cache-only RawByteOwner drain, never ordinary poll.
Completed Windows bytes/core input enter the existing raw input window under original native-step
and copy-operation/byte credits. Zero credits yield before mutation; no context poll, new socket
operation, codec step or output acceptance/submission occurs. Pending handlers/output remain owned
through later retirement, and the original first cause is unchanged.
HTTPS construction funds two transfer-owned trust copies simultaneously: host settings and the
temporary caller EndpointTls snapshot. Its complete inline object and bounded hostname/string
backing are included before either copy allocates. The byte owner's third internal settings
snapshot remains independently funded by the existing TLS allowance; neither domain borrows
unclaimed room from the other.