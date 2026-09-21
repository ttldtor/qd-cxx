# Java symbol coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

[156 inputs](inputs.tsv) produce [3465 reader observations](expected/symbols.tsv): 693 main reads
(original vectors and every strict prefix), each followed by repeat, MRU-repeat, reset/repeat and
an ABC sentinel on separate buffers. This observes primitive state; it is not parser recovery.
19 canonical writer inputs also produce 114 consecutive reader observations with flags 0/1/5/5/0/-1.
Null writer input is rejected before writing; null cipher decode is not an empty symbol.

| Area | Cases/evidence |
| --- | --- |
| Writer forms | empty, a, abc, abcd, abcde, abcdef, slash7, dollar7: zero/15/20/30/35-bit widths; seven/eight unencodable strings; ascii, quote, bmp, pair, high, low, nul, len63/len64 |
| Additional acceptance | zero15/20/30/35, penta-nul, dangling, noncipher35, UTF/CESU empty/overlong/pair and four-byte BMP forms, overlong-length |
| Errors/positions | all c0..df and fa/fb reserved leads; negative/above-INT_MAX lengths; undefined repeat; malformed UTF tails/leads/range; FD supplementary rejection; FC body overrun; every strict prefix |
| State | fresh and seeded ABC/MRU=5; repeated/new/zero/negative/cross-width flags; duplicate prefixes; post-failure repeat/MRU; explicit reset; canonical repeated symbols |
| Production | real composer bytes and parser callbacks: ABC,ABC,XYZ,XYZ / XYZ,XYZ,ABC; second message starts with 633a rather than ff |

Column order for read: id, phase, seed bytes, input bytes, Java outcome, consumed position, cipher,
raw string UTF-16, decoded UTF-16, flags, Java flag-byte count. '-' means empty, 'null' means null.
Writer rows retain original units/cipher/bytes. Only line endings are normalized; errors retain
Java classes/messages. Raw captures and complete provenance are in [validation](VALIDATION.md).

Source mapping and C++ projection are in [SYMBOLS](../../../../spec/encoding/SYMBOLS.md).
Reader options are ignored by pinned Java; production parser uses SUPPORTED_SET internally, while
writer/primitive probes use EMPTY_SET. Nonzero C++ masks are explicitly unsupported. No negotiation,
resolver, huge allocation, other input subclasses, full Unicode enumeration, event/time semantics,
PART, transport or upstream test-suite execution is covered. Small boundary representatives do not
claim exhaustive penta values or flag combinations. Historical fixtures and lock are preserved.
