# Option-chain production observations

[Русская версия](README.ru.md). English is authoritative.

`input.tsv` contains case names and exact standard field names in its header. Every value
is lowercase four-hex-digit UTF-16 units; `-` denotes empty text. Rows are profiles in
admission order, grouped into cases. The oracle also runs an empty-input case.

`oracle.tsv` is the exact pinned production Java result: CASE/CHAIN counts, ordered SERIES
metadata, CALL/PUT strike bits and profile SYMBOL, merged STRIKES, AROUND selections and
NEGATIVE exception class. Binary64 is canonical Java `doubleToLongBits` hex; string columns
use the same UTF-16 encoding. Empty strike lists use `-`. Only outer HashMap order is
normalized. No expected result is hand-derived from the C++ implementation.

Coverage: dual/equal/product-only/missing destinations; calls/puts; last replacement by
series/side/strike; missing strike and expiration; non-option/lowercase type; missing,
short, lowercase, invalid and nonvalidated extended CFI; nonfinite strikes; negative
strikes/day IDs; signed-zero strikes; every series metadata field, nonfinite/signed-zero
multiplier/SPC, unpaired UTF-16; insertion/exact/tail/nonfinite-center strike selection;
negative count. Native tests additionally check ownership, independent copies/moves,
all budget classes and allocation failure at every admission allocation.

[Provenance](provenance.json) records the pinned revision, actual JDK8 version, source/tool/
production-class hashes and exact fixture hash. The [maintained launcher](../../../tools/upstream/option-chains-oracle/README.md)
captures/verifies real production classes without network work or rebuilding the SDK.
