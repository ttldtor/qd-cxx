# Native subscription writer captures

[Русская версия](README.ru.md). English is authoritative.

The indexed expected tree contains a 317-byte native stream, actual production Java observations,
actual time-reader calls, source provenance and a SHA-256 manifest. It was produced by
[SubscriptionWriter](../../../../spec/subscriptions/WRITER.md) using the deterministic native test
fixture, then parsed by the locked [production verifier](../../../../tools/upstream/subscription-writer-oracle/README.md).
The pin is QD source 3.355 at 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f.
No protocol-version handshake is emitted; source pin does not fabricate runtime version metadata.

| Record | Native local ID | Outgoing sender ID | Java receiver ID | Fixture fields |
| --- | --- | --- | --- | --- |
| Alpha | 0 | 41 | 2 | x compact type 8 |
| Beta | 1 | 3 | 1 | x/y compact type 8, outgoing order explicit |
| Indexed | 2 | 7 | 0 | explicit timed hi/lo/x compact type 8 |

There are 42 observed tuples: 7 each for 11/12/16/17/21/22. Symbols include empty, embedded NUL,
unpaired UTF-16, AB* and A?, with message-local repeats. HISTORY_ADD contributes 7 actual
readSubscriptionTime calls (MIN/MAX/-1/0/repeated bounds). HISTORY_REMOVE contributes none;
its callback raw time is 0. These are raw signed record-time values, not business milliseconds.

The native fixture writes input-derived expected.tsv and expected-time-reads.tsv into its build
directory; the Java callback files must match exactly. Expectations are not recovered by decoding
the native implementation's bytes. Native golden byte tests separately cover framing/description/
symbol/bound syntax. This is not qualification of every data-field conversion by a subscription callback.

Windows MSVC and Linux GCC generated the same native bytes; both streams parsed into identical
five-file artifact trees using the same Windows JDK 8 provider profile. Linux Java execution is
not inferred. Source/tool digests are part of this exact reproducibility profile, separate from
private compiled-class/JVM receipts. Regeneration never rewrites this committed tree automatically.
[Validation](../../../conformance/subscriptions/WRITER-VALIDATION.md) states the executed limits.
