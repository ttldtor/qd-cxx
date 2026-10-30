# Production container fixtures

[Русская версия](README.ru.md). English is authoritative.

`baseline/` is Java QD 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`;
`current/` is the separate 3.356 pin `f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`.
Each directory contains actual closed FileWriter BINARY/STREAM_DATA/time=NONE output using
NONE (`none.qtp`), GZIP (`tape.qtp.gz`) and ZIP (`tape.qtp.zip`). The ordinary synthetic schema is
one record named Container with a compact integer field Container.F0=913 and symbol FLAGS.
No private market data or timestamp sidecar bytes are included. Raw bytes are marked `-text`.

The expected inflated bytes equal `none.qtp` exactly. Production FileReader reports one STREAM
record and one descriptor for each valid file. `compressed_input_tests` also verifies this exact
byte equality using one-byte source and destination chunks. Archive metadata such as production
ZIP entry timestamps is preserved; regeneration need not reproduce those physical bytes exactly.
Payload and integrity outcomes remain the compatibility criteria, with captured SHA-256 retaining
the identity of each artifact.

[Container specification](../../../../spec/protocol/TAPE-CONTAINERS.md) defines the selected stricter
closed-container policy. Java's acceptance of first ZIP entry only, absent ZIP central directory
or trailing bytes does not widen that profile. Native tests generate focused malformed headers,
CRC/ISIZE, truncated descriptor/directory/EOCD, extra entries and budget boundary cases in memory.
Production observations and native generated negatives remain distinct evidence.

The native regression also wraps the existing baseline/current production standard-Quote
`real-tape/producer/*/expected/quote-none.qtp` bytes using its independent test compressor, then
checks exact byte preservation through both envelopes. These are standard-schema byte-preservation
checks; they are separate from newly captured production FileWriter compressed Container fixtures
and from final CLI/parser/companion integration acceptance.
