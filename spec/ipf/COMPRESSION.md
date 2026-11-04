# Compressed IPF sources

[Русская версия](COMPRESSION.ru.md). English is authoritative.

The URL reader applies exactly one incremental source envelope before the existing UTF-8/IPF parser.
It detects NONE, GZIP (1f8b) and ZIP (504b0304) from content, independent of filename.
A response Content-Encoding:gzip selects that same single GZIP envelope; its decoded bytes are IPF,
not input to another magic detector. Absent/identity coding uses magic detection. Duplicate, stacked
and other HTTP codings are rejected. Requests continue to send Accept-Encoding:identity.

GZIP validates the selected member header, deflate payload, CRC32 and size trailer, and supports
optional headers and concatenated members. After a valid member, trailing non-member bytes and an
incomplete/invalid next member header follow the JDK8 stream behavior and are ignored. A subsequent
complete member header commits that member: payload/trailer truncation or corruption then fails.

ZIP skips directory entries, including their payload, and reads the first file entry.
STORED entries require known local/ZIP64 sizes; data descriptors are supported for DEFLATE only. Stored and
deflated entries, local ZIP64 size extras and data descriptors are supported. CRC and actual sizes
of every skipped directory and the selected file are checked. Names are bounded UTF-8 metadata;
they are never paths and no archive file is extracted. Encryption and unsupported methods fail.
No-file input fails. Later entries, central-directory data and trailing bytes are not interpreted
after the selected file; success does not claim that an entire archive is valid.

Descriptors may include or omit their signature. Local ZIP64 extended information selects eight-byte
size fields even for small payload; otherwise ZIP32 fields apply. Unsigned descriptors do not require
lookahead beyond their fields. Valid forms rejected by JDK8 defects are supported intentionally;
[ADR-0007](../../docs/decisions/ADR-0007-ipf-container-interoperability.md) records exact divergence rows.
ZIP64 metadata paired with a32-bit descriptor is inconsistent and fails explicitly.

IpfTransferLimits::envelope has independent finite encoded input, produced output, decoder calls,
byte-visit work, inflater backing, ZIP metadata and member/entry caps. Produced output includes
discarded ZIP directory data. Byte work charges offered input/output windows and zeroed allocations
before work, including failures; actual counters never refund on a new member or redirect. Fixed
decoder backing is included in transfer resident preflight, and the whole inflater allowance is
reserved there before allocation. zlib allocations use a capped aligned allocator in the same
operation. One decoder turn presents/produces at most 4096 bytes. There is no full response or
expanded-payload buffer. Cancellation is checked between the enclosing finite turns on the SDK
creator thread; no worker or callback invokes decompression.

Envelope integrity, HTTP framing, native/TLS retirement, IPF EOF and IPF COMPLETE are separate
evidence. Overall success requires the selected source layers. A later bad trailer keeps the
already accepted whole-profile prefix and COMPLETE observation, but cannot publish success.
Failure draining consumes only the already owned decoded cache; it performs no source, TLS,
native write or provider work. The first cause remains unchanged by later errors.

The optional IPF transfer target privately uses the project's exact zlib1.3.2 archive SHA256
bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16 and its checked license.
Its installed static closure includes the archive and license. Pure IPF/feed with transfer OFF
does not acquire or discover zlib. Tape inspection has its own container policy and is not an
IPF dependency.

Primary references: [GZIP format](https://www.rfc-editor.org/rfc/rfc1952),
[ZIP format](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT),
[zlib streaming API](https://zlib.net/manual.html), and pinned QD
[StreamCompression](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/StreamCompression.java).
The checked fixture oracle uses actual pinned QD production classes under JDK8; format documents
alone do not define Java stream edge behavior. Raw InstrumentProfileParser input remains UTF-8 IPF.
