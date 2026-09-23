# Configured event Java coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The oracle uses locked QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` unchanged. These are Java
observations, not C++ pass claims. [Contracts](../../../../spec/schema/CONFIGURED-EVENTS.md) and
[Profile semantics](../../../../spec/events/PROFILE.md) describe the accepted bounded projections.

| Cases | Actual surface and finding |
| --- | --- |
| default | Unchanged complete default Quote/Profile; actual delegate putEvent and createEvent, FileWriter, FileReader in STREAM/NONE and RAW/MESSAGE; three distinctive events per event/mode |
| sequence, nano, both, explicit | Four independent Quote optional combinations; explicit field selection wins over nano shortcut; subsecond packed Sequence differs from independent TimeNanoPart |
| endpoint-file, endpoint-api | FEED endpoint scheme resolution; conflicting file/system/API values; file wins over system, explicit builder wins over file |
| set-before, mutation | setProperty before initialization affects singleton; after does not; independently configured endpoint differs without mutating original scheme |
| xml, xml-system, xml-properties | Actual resource:dxlink.xml with imports; default options ignore JVM properties; enabling options allows explicit loader properties to win |
| xml-extension | Separate compatible XML overlay enables both Quote optionals and adds full8/full10 generic fields; original schemas retained |
| reorder | Separate ordinary programmatic records reverse physical logical field order, add extra full8 fields and put Quote at local ID1 |
| minimal | Only required Quote price/size and Profile Description retained; all optional absence defaults delivered through production delegates/files |
| numeric-int, integer-representation | Separate programmatic all-numeric8 and property-selected numeric8 schemas; signed int projection measured independently of wire conversion |
| rejected | Missing required local fields produce no event delegate; positive ChosenPrice alias and conflicting BidPrice alias use actual DefaultRecord alias operations |
| xml-unsupported, tiny-unsupported | Original full137 XML string and full24 tiny decimal variants retained; unsupported by bounded C++ full-ID profile |
| invalid-boolean | Java invalid nanoTime reports diagnostic error and falls back to false |

All 19 file-producing scenarios additionally contain separate empty wire descriptions through
production FileReader: local required fields remain present, raw defaults are delivered. This differs
from minimal local absence. Null, empty, embedded NUL, paired and isolated surrogate strings are
retained exactly; decimal inputs include NaN, infinities and negative zero. Dates and seconds include
negative and boundary values. Flags include zero, reserved bits and -1; raw flags remain independent
of zero QTP event flags. Original input and delegate-put values are both retained: input rounding,
second truncation and unsupported-property omission are not hidden by comparing only ideal inputs.

Every put/read comparison checks all mapped properties against actual production delegate output,
with eventTime=0 for STREAM and 1600000000123 for RAW. The callback count records RAW fanout;
only STREAM callbacks furnish one-record event expectations. Direct Sequence boundary probes
separately write INT_MIN/-1/0/1/4194303/4194304/INT_MAX to the cursor and call the production delegate.
They establish getter projection, not setter acceptance or a production-file test of those raw patterns.

The source-resolution slice covers the ledger in CONFIGURED-EVENTS, not arbitrary providers,
all configuration aliases, environment sources, live updates or full dxlink event coverage.
C++ accepts explicit resolved inputs; no general schema loader, new types, indexed records,
wire coercion, compression, broader transport/session semantics or private-tape support follows.
