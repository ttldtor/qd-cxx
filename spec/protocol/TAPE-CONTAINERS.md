# Closed binary tape input containers

[Русская версия](TAPE-CONTAINERS.ru.md). English is authoritative.

This specification defines the optional, tool-owned `CompressedInput` boundary. The logical output
feeds the existing chunked tape reader; it does not change [tape wire semantics](TAPE-READER.md),
whole-owned admission or replay. The adapter introduces no transport, callbacks, threads, archive
extraction or recursive decompression. Selection and physical file ownership belong to the tool.
The borrowed uncompressed inspector API retains its existing contract.

The pass-through NONE backend and API are available whenever the inspector or its tests are built,
including with `ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION=OFF`. `containerAvailable` reports compiled
support without allocation or I/O: NONE is always available, GZIP/ZIP require that option, and invalid
enum values are unavailable. Known unavailable kinds fail with `UNSUPPORTED` on first read before
allocation or source access; construction remains lazy. Owners can query availability before copying
paths or opening files. With no tool/test consumer, no backend target is created. zlib acquisition
and the C language are enabled only for an actual compression consumer; core stays independent.

## Ownership and completion

`CompressedInput` borrows a binary `std::istream` until destruction and owns only finite decoder,
input scratch and ZIP metadata state. Its constructor performs no input I/O. `read` borrows a
nonempty writable span only during the call and returns its produced prefix, a physical completion
flag and a typed error. No caller memory or callback is retained. `finish` performs no I/O and
returns `NOT_FINISHED` until physical completion. Destruction releases allocations without reading,
draining or closing the borrowed source. The file owner destroys the decoder before closing files.

A late CRC/trailer/directory/source failure may accompany a nonempty produced prefix. The caller
submits that prefix through its existing feed/resubmit path before processing the terminal source
failure. The prefix is not a promise of successful completion. Errors are sticky; further calls
perform no reads or inflater work. An empty read span gives `INVALID_ARGUMENT` before work, including
after successful completion, and preserves an already recorded error. Repeated nonempty reads after
completion return zero bytes and completion without further I/O.

Container physical completion requires validated integrity and actual physical EOF. Final inspection
COMPLETE additionally requires parser finish, companion and split-source closure where selected,
successful owner close and successful output flush. Cancellation, refusal, resource limits or any
layer failure cannot authenticate COMPLETE. Cancellation must not drain the source to discover CRC.

## GZIP profile

Read all concatenated members as a single logical byte stream, including empty members. Require at
least one member and validate every header, reserved flag, optional header checksum, deflate stream,
CRC32 and ISIZE. Physical EOF is valid only between complete members. Arbitrary trailing bytes,
partial next headers and truncated trailers fail. The result must not depend on physical short-read
partitioning while operational work/call limits remain sufficient.

The implementation uses zlib's gzip wrapper and bounded header feeding. `Z_BLOCK` separates deflate
block completion from wrapper trailer processing so expansion accounting excludes headers/trailers.
Header names, comments and extra data are not retained. They still consume metadata and work limits.

## ZIP profile

Accept single-disk ZIP32 with exactly one nondirectory file and bounded empty directories before or
after it. Methods are stored (0) and deflate (8). The ordinary signed ZIP32 data descriptor emitted
by production writers is supported for deflate. Stored entries require known local sizes; unknown
stored sizes/data descriptors are unsupported. A directory's decoded payload must be empty.

Validate local headers, deflate termination, CRC, actual sizes, signed descriptors where present,
central entries, corresponding names/methods/flags/local offsets, EOCD counts/size/offset/comment,
and physical EOF. Central entries must have the same order as local entries. Metadata names are
opaque bytes; no encoding conversion or filesystem access uses them. Extra fields have validated
length framing; ZIP64 extra fields are unsupported. No payload scan guesses a descriptor boundary.

Reject zero files, multiple files, encryption, unsupported flags/methods, ZIP64, multidisk/split
archives, self-extracting prefixes, reordered central entries, unsigned descriptors and trailing
bytes. A second file cannot be silently ignored after reporting the first file as successful.
Unsupported choices and malformed structure remain distinct from resource refusals.

## Finite bounds and coordinates

All configuration is explicit per instance; zero means literal zero, never unlimited/default.
Mandatory configuration/representability are checked before state allocation or input work. Each
dynamic allocation checks its remaining owned capacity first; variable metadata lengths are parsed
from a bounded header before their allocation is considered. Input
scratch must be positive and representable by both `uInt` and `streamsize`. Physical and inflated
allowances must leave room for one representable diagnostic excess byte. Counter additions and
allocation products are checked. Work is never refunded after failure, skip or replacement.

| Limit | Default | Charge |
| --- | --- | --- |
| Physical bytes | 64 MiB | Every observed source byte, including bounded read-ahead |
| Inflated bytes | 64 MiB | Every logical byte before publication to QTP |
| Expansion ratio/slack | 1000 / 64 KiB | Per-member deflate input and produced payload |
| GZIP members / ZIP entries | 1024 / 64 | Empty members/directories included |
| One metadata record / total scanned metadata | 256 KiB / 4 MiB | Header/name/extra/comment/descriptor/directory bytes |
| Work bytes / inflater calls | 256 MiB / 262144 | Physical + inflated + metadata scan/move charges; every actual inflate call |
| Retained metadata/scratch | 4 MiB | Input scratch, ZIP entry slots and retained names |
| zlib allocation | 1 MiB | Outstanding allocations including allocator headers |
| Input chunk | 4096 bytes | Finite read-ahead; short reads aggregate into this bounded chunk |

Metadata scans reserve their required byte charge before scanning and do not refund it after a
truncated read. Entry storage grows only for observed entries; a large entry-count allowance does
not preallocate its maximum. Both old and replacement slot storage must fit the retained allowance
before growth; moving existing slots adds irreversible work charge. Names own exact byte arrays,
with no unaccounted string capacity or terminator. These are logical owned-capacity bounds, separate
from fixed decoder state, allocator bookkeeping and process RSS.

At an exact physical allowance a single one-byte EOF/excess probe is permitted. EOF charges no byte;
an additional observed byte is reported diagnostically and returns `COMPRESSED_LIMIT` without
forwarding it. At an exact inflated allowance, trailer processing may produce zero further bytes;
at most one reserved probe output byte may detect excess, which is discarded with `INFLATED_LIMIT`.
Similarly, a nonzero exhausted byte-work allowance may probe for zero-byte EOF/trailer progress;
any additional byte work fails before publication. A configured zero work allowance permits no
read/inflater work. Every actual inflater invocation still requires a remaining call allowance.

Byte/member/integrity outcomes are invariant under physical short reads while operational work/call
limits remain sufficient. Operational cap exhaustion returns `WORK_LIMIT`, never a false format
error or COMPLETE. These limits bound admitted work and memory; they do not promise a CPU deadline.
Physical I/O failure discovered after buffered input is deferred until that bounded prefix is
processed. No further physical read is attempted after discovering the failure.

Keep `compressedPhysicalRead` (including read-ahead), `compressedConsumed`, logical produced/QTP
accepted offsets and member/entry/source ordinal distinct. No exact inverse compressed coordinate
exists for arbitrary logical frames. Companion timestamp positions refer to inflated QTP bytes,
not physical compressed offsets. Each data/companion envelope has its own closure and finite bounds.

## Dependency boundary

The optional backend is [zlib 1.3.2](https://zlib.net/), under its
[zlib license](https://zlib.net/zlib_license.html). Source archive:
`https://zlib.net/zlib-1.3.2.tar.gz`, SHA-256
`bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`.
Retain its source-distribution copyright/license notice. Only the in-memory inflate/checksum API
is used; ZIP envelope validation is tool-owned code, not a zlib ZIP validation claim.

`ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION` defaults OFF. Acquisition and enabling the C compiler
are permitted only with ON and a requested compression consumer/test target. OFF performs no
dependency fetch, source lookup or system zlib discovery. Core, encoding, protocol and event targets
receive no compression include, link dependency or macro. Ambient DLL discovery is not supported.

Format references: [RFC 1952](https://www.rfc-editor.org/info/rfc1952/),
[PKWARE ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).

## Production compatibility limits

The compatibility references remain Java QD 3.355 at
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and separate 3.356 at
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Actual FileWriter/FileReader observations show valid
NONE/GZIP/ZIP payload parity. Java's ZIP wrapper selects only the first file after leading
directories; its reader can accept missing central directories and trailing bytes. Its GZIP
continuation can depend on JDK 8 read-ahead. FileReader logs and catches late container failures,
so returned readInto or zero parser errors is insufficient closure evidence.

The selected C++ closed-container profile deliberately validates the full finite envelope and has
the stricter restrictions above. It does not claim universal Java archive acceptance or live-source
support. [Container fixtures](../../tests/fixtures/protocol/tape-containers/README.md) identify
production bytes and executed checks separately from future integration checks.
