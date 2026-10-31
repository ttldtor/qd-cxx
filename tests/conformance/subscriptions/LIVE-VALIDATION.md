# Pure live-session validation

[Русская версия](LIVE-VALIDATION.ru.md). English is authoritative.

The [session contract](../../../spec/subscriptions/LIVE-SESSION.md) covers a pure
in-memory QTP component. Native tests and the optional
[Java control observer](../../../tools/upstream/live-session-oracle/README.md)
have separate roles: native tests check ownership/admission and the observer
compares actual locked production parser callbacks with native expectations.
A resource preflight is not a substitute for that comparison.

## Native scenarios

- Fresh opaque epochs; successful ownership transfer and unchanged caller inputs
  after factory failure; matching schema/composer provenance.
- Source-backed full/delta protocol state, exact nullable text, inherited message
  properties, `hs` token handling and independent readiness/authentication gates.
- All heartbeat masks, signed millisecond/mark extremes, explicit empty payload
  versus mask zero, and empty envelopes without a message type.
- Every fragmentation boundary, exact accepted suffix accounting, malformed
  current-frame rejection without exposing a record prefix, and explicit EOF.
- Immutable replacement descriptions, unsupported full types and actual wrappers,
  unknown described records, old generations and views retained after session destruction.
- Exact TICKER/STREAM/HISTORY message identity, generic field payload ownership,
  raw removal/virtual field metadata, requested output layouts and raw HISTORY bounds.
- Whole retained output backing across partial consumption, owner/capacity peaks,
  allocation failures and work exhaustion before publication.
- Configuration accounting with zero visit credits; copied/moved immutable storage
  metrics reflect the resulting owner's capacities without a new factory traversal.
- Repeated frames beyond a per-frame byte quota without treating a live connection
  as a finite tape file.
- The complete default Quote schema and original Java description/data fixtures
  through the generic session into the existing prepared Quote mapping.

The internal `qdcxx_live_session_tests` executable receives the repository source
root and its private capture directory through CMake compile definitions. Run
`ctest --test-dir <build> -R "^protocol_live_session$" --output-on-failure`.
It runs finite tests and emits
only synthetic controls and their expected callback ledger. Test failure produces
a nonzero result; it neither opens a network connection nor starts persistent processes.

## Evidence boundary

The final source was checked with MSVC 19.42 x64 Release and GCC 13.3 Debug
with AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer was explicitly
disabled. The 40-test native profile passed on Linux. On Windows, 39 tests passed
inside the sandbox; `tooling_format_contract` failed to create a source alias
with `Access denied`, then passed alone with the required filesystem access.
Thus every test in that profile has a passing final-source result; the original
environment failure is retained separately.

The live suite contains 24 scenarios and 37,314 assertions. Independent MSVC
checks added three allocation/ownership scenarios with 707 assertions; the
combined independent run passed 27 scenarios and 38,021 assertions. Native
Windows and GCC controls both passed production Java comparison: 344 input bytes,
20 callbacks and 97 canonical rows. Java ran on Windows JDK 8 for both native
inputs; this does not claim a Linux JVM run.

The prior subscription writer retained its exact 317-byte output, observations
and HISTORY time reads; only its generated source provenance and manifest were
refreshed. Five prior tape-writer cases passed the production Java parser;
all 17 emitted corpus files matched between Windows and GCC. Production-only
consumers built and ran on both platforms with testing/JSON disabled and no
external dependency acquisition. Generated API reference built without warnings.

The Java observer uses QD 3.355 at the source revision in
[upstream.lock](../../../upstream.lock), unchanged prepared production classes
and a verified provider receipt. Source version and observed runtime advertisement
are separate facts. Only synthetic authorization is admitted by this observer;
nonempty authorization is redacted in the callback ledger.

Native control generation, callback comparison, source/provider hashes and repeat
verification must refer to the same final source. Existing subscription and tape
writer comparisons are repeated when their hashed shared codec sources change;
previous verification records remain historical evidence.

This scope does not qualify transport, TLS verification, callbacks, collectors,
reconnection, arbitrary descriptor extensions, `PART` reassembly, alternate tape
profiles or all provider records. Requested backing counters are not measured RSS.
Caller-created independent copies and cross-thread release are outside the
documented per-epoch ownership contract.
