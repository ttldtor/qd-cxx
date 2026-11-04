# Standard serialized attachment witnesses

[Русская версия](README.ru.md). English is authoritative.

`capture-v1` contains original pinned QD3.355 production observations.
[Provenance](provenance.json) binds upstream sources, harness, launcher, Java runtime,
production class/resource inventory and all 29 artifacts. The
[oracle](../../../tools/upstream/attachment-oracle/README.md) reproduces them without rebuilding QD.

Each of 14 `event` TSV rows records case, actual wire record ID, field name, full type 249,
nullable body hex, explicit Java getter result, Configuration version and exact composer wire hex.
`null` and `empty` differ. `.qtp` is actual standard-scheme composer output followed through parser
and event delegate; `.tape` is separate production FileWriter output from the same event/delegate
cursor. Two `text-field` rows record only the independent standard Text identity 10.

Both events cover null, String `hello`, empty/content Java byte[], a benign custom Serializable,
engaged-empty raw and malformed raw `010203`. The raw forms survive production wire parsing but
explicit Java Object access throws MarshallingException. Empty Java byte[] occupies 27 serialized
bytes starting `aced0005`, different from engaged-empty. Configuration version `-7` passes through
the actual delegate; filtering is not exercised. The oracle's custom class is a representation
witness, not a production native codec or arbitrary Java Object compatibility promise.

Persistent CTest `attachment_serialized_contract` reads FileWriter tapes with the public generic
TapeReader. It covers exact owned values after reader/schema lifetime, chunks 1/7/65536, independent
copy mutation, unknown records, exact body bounds and truncated data-frame failure. The composer
artifacts provide precise truncation independent of FileWriter trailing markers.
Typed projections, registry and Java decoding remain outside this test's claim.
See [contract](../../../spec/events/ATTACHMENTS.md).

[Recorded validation](../../conformance/ATTACHMENT-VALIDATION.md) separates executed generic checks
from future typed API requirements.
