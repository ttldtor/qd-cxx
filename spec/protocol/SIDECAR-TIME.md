# Bounded LONG/TEXT companion scanner

[Русская версия](SIDECAR-TIME.ru.md). English is authoritative.

The tool-private `SidecarTime` scanner decodes a finite logical `.time` stream. It owns bounded line
storage and returns owning timestamp/position marks. It does not open files, decompress containers,
schedule replay, invoke callbacks, select schemas or change protocol-reader timing. CLI integration
is separate work; protocol-header admission is selected independently below. This scanner alone does
not enable LONG/TEXT input in existing readers or change any existing profile/default.

## Representation and finite grammar

Java `TimestampedPosition.writeTo` produces LONG as decimal absolute milliseconds, `:`, signed
decimal logical data position and a platform newline. LONG is text, with no binary byte order.
TEXT uses `YYYYMMDD-HHmmss.SSS+HHMM` or `-HHMM`, except epoch zero is `0`; outside its calendar range
Java can emit decimal milliseconds. Both native format labels accept the same finite union:

- canonical nonnegative decimal milliseconds, from0 through INT64_MAX, without a leading sign or
  redundant leading zero;
- exactly24 calendar bytes with year0001..9999, a valid proleptic Gregorian date, hour00..23,
  minute/second00..59, millisecond000..999, and mandatory numeric zone hour00..23/minute00..59;
- a position in the full signed64 domain, with optional `+` or `-` and no surrounding whitespace.

Absolute preepoch TEXT is supported: `19691231-235959.999+0000:0` represents-1ms. Numeric negative
time is `UNSUPPORTED_TIME`: Java interprets it relative to its ambient clock rather than as an
absolute signed timestamp. The scanner has no ambient clock/timezone/locale. Relative durations,
zone-less dates, alternate Java-friendly date grammar, whitespace and BOM are outside this profile.
Nonmatching time/position and integer overflow produce typed errors, never floating-point values.

The last colon separates time and position. A line without a colon is ignored but still bounded.
CR, LF and CRLF terminate lines; CRLF counts as one line across arbitrary feed boundaries. A final
unterminated line is parsed once at logical EOF. Empty input and an empty valid companion are valid.
Calendar arithmetic is bounded by the admitted years. `SidecarMark::packed` preserves exact
`timestampMs` separately and projects Java cursor time using saturated signed32 seconds and
floor-mod1000 milliseconds, with the remaining sequence bits zero.

## Positions and chronological use

Positions refer to inflated logical DATA bytes, including its protocol header and descriptions,
before compression. Companion logical bytes have their own coordinate. Neither maps exactly back
to an arbitrary compressed byte; physical read-ahead, compressed consumption and inflation remain
separate container diagnostics.

A caller owns at most one pending mark. It submits existing data bytes up to that mark's position,
publishes complete parser results, then applies reached marks in source order. Negative, equal and
decreasing positions apply immediately; negative times from absolute TEXT and decreasing times are
permitted. `reached` compares signed positions safely without unsigned subtraction.

The effective time follows events: a reached mark replaces current external time; a later MESSAGE
heartbeat replaces it; a later mark replaces it again. A mark inside an incomplete frame affects
that frame when complete, including physically earlier records within that same frame. A mark at
the frame end affects following frames. A nonzero external packed value replaces delivered FIELD
packing; external zero falls back to the separately owned raw FIELD metadata. Ordinary business
`Time` fields are independent. The scanner performs none of this parser/sink integration itself.

## API, ownership and terminal states

`SidecarTime(format, limits)` is `noexcept`, allocation-free and I/O-free. It validates enum and
representation constraints immediately, storing a fixed sticky diagnostic for invalid configuration.
Zero limits are valid zero capacities. Line storage is allocated lazily only after input, line,
retained and work allowance checks; allocation failure becomes `ALLOCATION`.

`feed(span)` borrows bytes for the call only. It returns the consumed prefix and at most one owning
mark; the caller resubmits every unaccepted byte before reading more. It retains no caller span,
stream, decoder, schema or callback. Marks survive scanner and source destruction. Empty feed before
EOF does no work. Statuses are `NEED_INPUT`, `MARK`, `END`, `FAILED`; `FAILED` avoids the Windows
`ERROR` macro and the header is usable after `windows.h` without changing caller macros.

`endInput()` declares caller-confirmed logical EOF without I/O. It returns a final unterminated mark
once, then `END`; repeated successful `END` is idempotent. Feed after EOF is sticky `INVALID_ARGUMENT`.
`finish(dataLogicalBytes)` requires `END`, checks maximum nonnegative mark position against actual
data length and seals the first data EOF length. Successful repeated finish checks do no work and
ignore a later length argument. Early finish is sticky `NOT_FINISHED`. A later prohibited feed still
invalidates scanner success. Every first failure is sticky: later feed/endInput/finish return it,
accept no bytes, publish no mark and perform no input/allocation work. Destruction never drains or
closes sources. `END` and successful scanner finish are syntax/position proof only.

## Explicit protocol-header admission

`TapeExternalTimePolicy::{DISABLED,SIDECAR}` selects admission only. Required-final overloads are:

```cpp
TapeReader(schema, limits, mode, fieldTimePolicy, externalTimePolicy);
TapeReader(limits, discovery, fieldTimePolicy, externalTimePolicy);
```

`SIDECAR` requires `BINARY_CATALOG_V1` and admits exactly `LONG`, `long`, `TEXT`, `text` in initial
headers and active descriptor deltas. NONE/MESSAGE admission is unchanged; FIELD still independently
requires `FieldTimePolicy::READ`. Mixed case and other tokens retain typed TIME refusal. Raw time
text is owned without normalization. All old constructors delegate with `DISABLED` and keep their
original call signatures/defaults; unrelated factories are unchanged. Invalid enum/mode selections
fail before owning schema/framing allocation.

The policy retains only a fixed enum tag. It adds no source/provider, clock, I/O, external timing
state or setter. It does not change raw FIELD values, MESSAGE heartbeat timing, partial-frame
publication or parser finish. The owning tool still applies chronological sidecar/heartbeat events
and validates companion/container/physical closure. A LONG/TEXT header alone proves none of these.

## Resource charges

| Bound | Finite default | Charge |
| --- | --- | --- |
| Input | 4MiB | Consumed logical companion bytes, including CR/LF |
| Line | 128bytes | One line body, including ignored lines |
| Lines | 262144 | Attempted terminated/final unterminated lines |
| Marks | 262144 | Returned owning marks, including repeated positions |
| Work | 8MiB | Input examination plus admitted parse-character work |
| Retained | 128bytes | Configured line scratch; fixed object state excluded |

All zeros are literal, never unlimited/default. An empty source can finish with zero capacities;
nonempty input with zero line/retained allowance refuses before scratch allocation or examination.
Counters never refund skipped, failed or already returned work. No untrusted line/position controls
reserve size. Decimal accumulation checks overflow before multiplication/addition. Unsigned charges
check remaining allowance before addition; no signed subtraction compares data/mark positions.
Counters distinguish consumed bytes, work, attempted lines, returned marks and maximum nonnegative
position. Diagnostics own scalar line/column/logical companion offset checkpoints and a static stage.

Errors distinguish configuration, input/line/count/mark/work/retained/resource bounds, allocation,
malformed time/position, unsupported time, position beyond data and premature finish. Physical
source I/O, container CRC/trailer, physical close and sink flush remain separate owner errors.

## Closed-pair owner boundary

A closed-pair owner validates ALL companion syntax and every nonnegative mark against final data
length. Missing required companion is a source failure; marks beyond data EOF and malformed unused
tail refuse completion. This is deliberately stricter than Java FileReader, which may ignore a
future mark's tail and logs some open/close errors without throwing. Empty valid companion remains
accepted. FileReader's normal return is not authenticated completion of both files.

The owner destroys both borrowed container adapters before closing either physical stream. Every
opened member receives its close attempt even when the first close fails or the other open failed.
The first observed parse/source/cancellation cause remains primary through cleanup. Separate bounded
close/flush diagnostics retain secondary failures; cleanup cannot overwrite a prior cause or restore
success. Without an earlier cause, the first cleanup failure becomes primary.

COMPLETE requires parser finish, both container integrity/physical EOF checks, scanner END/position
finish, required segment closure, both physical closes and sink flush. A container may return bytes
and a late error together: valid produced bytes are submitted first, while the detected source error
remains recorded. Cancellation/sink refusal stops further reads/callbacks, companion drain and file
transition. Cleanup still releases adapters and closes opened streams without hidden reads.
The focused native owner harness exercises this lifecycle separately from future CLI integration.

Java derives `a.qtp`=>`a.time`, `a.qtp.gz`=>`a.time.gz`, `a.qtp.zip`=>`a.time.zip`; each companion has
its own container/integrity/budget state. Native owner path resolution remains outside this scanner.
Actual Windows/POSIX Java naming uses the native separator: a slash on Windows or backslash on
POSIX is not interchangeable in its extension helper. Remote URLs, wildcard discovery and general
Java filename compatibility are outside the explicit local closed-pair profile.

Tests: [scanner](../../tests/conformance/sidecar_time_tests.cpp),
[protocol admission](../../tests/conformance/sidecar_policy_tests.cpp),
[Windows include order](../../tests/conformance/sidecar_windows_include_test.cpp),
[allocation ordering](../../tests/conformance/sidecar_allocation_test.cpp), and
[paired owner](../../tests/conformance/sidecar_owner_tests.cpp). Production-source basis:
`TimestampedPosition`, `FileReader.process`, `FileWriterImpl.writeData`, `FileUtils`,
`TimeFormat` and `TimeSequenceUtil` at the versions pinned in [upstream.lock](../../upstream.lock).
