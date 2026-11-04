# Schedule rules and timezone compatibility

[Русская версия](RULES-AND-TIMEZONES.ru.md). English is authoritative.

Status: **implemented constructor/calendar and owning Day/Session query contract** in the optional [schedule component](../../docs/SCHEDULE-QUERIES.md). Filters remain separate. The accepted [provider/data boundary](../../docs/decisions/ADR-0008-schedule-timezone-data-ownership.md) is integrated through the shared [timezone component](../../docs/TIME-ZONE-DATA.md); parser-only contexts initialize no provider.

## Authority and independent datasets

Use the revision in [upstream.lock](../../upstream.lock), specifically [Schedule](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/schedule/Schedule.java), its Day/Session/filter types, and [TimeUtil](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimeUtil.java). The observed primary JVM is Java8 1.8.0_422 with TZDB2024a. A source revision does not determine a JVM timezone dataset.

The market-default resource has date `20260323-170000+0000`, SHA-256 `dba44f5fbe985199230815a760eb9433348c9b0716c97b32c41c04b1d8822811`, 62,083 bytes, 100 venue definitions, and 75/53/6 named holiday/short/special-day lists. It is MPL2 data from QD. These lists define known dates, not timezone transitions or an unlimited promise about future holidays. XNYS Christmas 2026 is a holiday in that resource; Christmas 2030 is not. The absence of a date is the reference's ordinary-day input, not evidence that the real exchange will trade.

Public data identity distinguishes calendar date/hash, timezone version/hash, provider profile and compatibility-reference runtime. Current production data can legitimately differ from Java8's older dataset. Such differences need reviewed expected data outcomes; they are not a reason to change Calendar algorithms or silently rewrite an older fixture.

## Schedule text and defaults

A schedule is a venue followed by a parenthesized property list, for example `XNYS()` or `(tz=UTC;0=09001700)`. An instrument may contain several venue portions. The default factory selects the first portion; an explicit venue factory selects that named portion or reports absence. A known venue inherits its named defaults; inline properties override them. An unknown venue can work with sufficient explicit properties, while `UNKNOWN()` fails for its missing timezone.

Properties are separated by semicolons. The reference parser accepts multiple keys joined by `=` with the last cell used as their value; repeated keys use the last occurrence and its ordering. It does not perform general whitespace normalization. Unknown ordinary properties can be ignored, so a stricter generic key/value validator would change compatibility.

Portion selection and venue enumeration deliberately use different source algorithms. Factories select the first opening parenthesis and its next closing parenthesis, taking the venue after the preceding closing parenthesis or semicolon. Named selection scans these portions. Enumeration instead uses ASCII-word `\w*` regex-find semantics and excludes nested parentheses. The native parser preserves both paths.

| Property | Required behavior/source default |
| --- | --- |
| `name` | Optional name; defaults to the venue identifier |
| `tz` | Required after named-default merge; validated by the selected compatibility ID policy |
| `td` | Trading weekdays, default `12345`; Monday 1 through Sunday 7 |
| `de` | Day end, default `+0000`; day start is the same local clock on the preceding day |
| `rt` | Reset time, default day start; within the day, with the reference zero-length-day exception |
| `0`, `1`..`7`, `8` | Default trading-day definition, explicit weekday definitions and default non-trading definition; fill missing weekdays according to `td` |
| `hd`, `sd` | Holiday/short-day date lists or named list references; union, subtraction `-` and intersection `*` retain source ordering/semantics |
| `sds=ecHHMM` | Short-day early-close offset strategy |
| `hds=jntdN` | Join-next-trading-day strategy with the source single-digit parameter |
| `YYYYMMDD` | Explicit day-definition override |
| `ds.YYYYMMDD`, `ds.hd.name`, `ds.sd.name`, `ds.xd.name` | Ordered per-day/named-list strategies |

Day/session definitions accept repeated `+`/`-` day shifts followed by HHMM or HHMMSS clock fields. Session pairs may use `/` between endpoints. Prefixes `p`, `r` or no prefix, `a`, `d` and `rt` identify pre-market, regular, after-market, day boundaries and reset time. Trading sessions must be ordered; gaps are filled with NO_TRADING. After one prefixed pair, subsequent unprefixed pairs in the group are regular sessions. Deprecated `2400` has reference handling and does not make arbitrary hours valid.

Day strategies include `closed`, `normal`, `def:<day definition>` and `ec:<offset>`, including ordered combinations. Early-close values are offsets, not an invented absolute closing-clock convention. Unknown strategy names/parts have reference warning/ignore paths; malformed parameters for recognized strategies can fail. Zero-duration sessions and days are valid where the source admits them. A zero-duration REGULAR session still has a trading type even though it contains no instant.

The supported grammar is this complete pinned source family, including named/default rules, date-list algebra, day shifts and strategies. Implementations must preserve it rather than reduce support to one UTC interval or fixed offsets.

Direct `de`/`rt` time fields retain the source's character-arithmetic behavior without a digit precheck; session groups require decimal digits. Date-list tokens require eight decimal digits and a month from 1 through 12, while year zero and day values such as 00 or 99 remain admitted. These construction rules do not impose the later query methods' label ranges. The C++ defaults update accepts a typed comparison version and ordered entries; it does not expose the raw-byte Java `TimeFormat.GMT` header loader.

## Timezone and calendar policy

Zone IDs are case-sensitive. Preserve the accepted Java-compatible identifiers and aliases, including `UTC`, `US/Eastern`, `PST`, `EST`, both `Asia/Kathmandu` and `Asia/Katmandu`, and normalized custom IDs such as `GMT+05:30`. Reject unknown IDs and IDs whose normalized Java form differs from the requested spelling; retain the requested alias as the public identity. Do not derive the accepted-ID policy from platform/provider aliases. Custom GMT offsets require explicit compatibility handling.

The parser's tracked [identity manifest](../../resources/schedule/manifest.json) records all 631 accepted named IDs observed from the pinned Java 8 runtime, plus the normalized custom-offset algorithm. This identity table is deterministic validation data and contains no timezone transitions.

Calendar local-time conversion moves a nonexistent local time through the gap by its duration and selects the later occurrence in an overlap. Examples: New York 2024-03-10 02:30 maps to 07:30Z; New York 2024-11-03 01:30 maps to 06:30Z. Lord Howe's 2024-10-06 02:15 half-hour gap maps to 2024-10-05 15:45Z, and its 2024-04-07 01:45 overlap selects 2024-04-06 15:15Z. Apia's skipped 2011-12-30 at noon maps to 2011-12-30 22:00Z, preserving the next local date's noon result.

Schedule day construction is distinct from a generic local-time conversion. It creates a day-id noon anchor using the zone raw offset, applies Calendar field/day changes, and retains the DayUtil label. For `(tz=America/New_York;td=1234567;0=02300330)` on the spring transition, both REGULAR endpoints are 07:30Z and the zero-duration session remains present. For `(tz=Pacific/Apia;td=1234567;0=0000+0000)`, label 20111230 is retained and the interval is `[2011-12-29T10:00Z, 2011-12-30T10:00Z)`. Do not replace this with a rule that skips to the next civil date.

Timezone data version and Calendar compatibility are separate concerns. TZDB 2026f differs from the older Java 8/TZDB 2024a reference for examples including Havana and Winnipeg; document and test these as data-version differences. Do not use them to relax the Calendar or raw-offset algorithm. Historical New York Schedule behavior also uses its raw-offset/date semantics (for example, -05:00 in 1800), which need not equal a historical IANA local-mean-time offset.

Schedule/event time is signed 64-bit Unix milliseconds without leap-second insertion. Preserve the project's day-number/calendar arithmetic. Do not route through a native nanosecond `system_clock::time_point` if that narrows the supported range. UTC day labels remain consecutive through the 1582 dates; constructing a fresh default-cutover GregorianCalendar from YMD is not equivalent to Schedule's algorithm.

## Method-specific ranges and interval boundaries

The UTC YMD factory admits `0001-01-03` through `9999-12-29`; immediately outside these endpoints it throws. Its human-readable error mentions the wider dates 0001-01-02 and 9999-12-30; operational bounds come from behavior, not that message. Day-id lookup admits `[-719160, 2932894]`, and rejects both neighboring IDs.

Time lookup admits the inclusive millisecond range `[-62135424000000, 253402128000000]`. The upper instant returns a day labelled 99991230 even though that label/day-id cannot be requested directly through the other factories. Both neighboring milliseconds are rejected. Preserve these method-specific limits rather than unifying them into a guessed common range.

Invalid YMD values can normalize to the lowest valid later label: 20240230 becomes 20240301 and 20241301 becomes 20250101. Session/day membership is half-open `[start,end)`; zero-length intervals contain no time. Filters and nearest/next/previous search must preserve the complete supported grammar and these boundary semantics.

## Provider boundary and explicit Schedule contexts

The selected provider/data profile uses Howard Hinnant `date/tz` v3.0.5 at commit `179a6b921ccd6179b5b04dce812a89f71a51b4df` and IANA TZDB 2026f. The maintained product integration and shared CORE binding are described in [timezone data](../../docs/TIME-ZONE-DATA.md); [owning Day/Session queries](../../docs/SCHEDULE-QUERIES.md) retain independent calendar generations. Filters remain separate.

Initialization accepts an explicit data root and validates a fixed snapshot before parsing that owned snapshot. It performs no ambient home/OS lookup, CURL or remote access, or runtime reload. One private provider generation is initialized per process. All lazy provider work completes before `READY`; published queries use immutable provider state. Initialization failure is terminal and bounded, while an invalid argument rejected before compare-and-set admission does not consume the initialization attempt.

The provider profile bounds admission to 11 files, 1,100,000 source bytes, 1,024 bytes per line, 35,000 lines, and 12,000 source records; native construction is bounded to 600 zones, 400 links, 10,000 rules, and 32,768 cumulative model insertions. It uses the standard allocator and makes no configurable hard heap/RSS-cap claim; measured peak memory is not a guaranteed limit. Provider initialization failure publishes no usable context. Calendar data contexts and retained calendar generations are separately owned; the provider's one-generation-per-process rule does not cap the number of public `ScheduleDataContext` instances.

Timezone provider/data updates require an offline, maintainer-reviewed update followed by deployment and process restart. Market-calendar defaults remain independently versioned. The implemented typed context update validates a complete generation before publication. Immutable Day/Session values retain their original generation, including navigation; getSchedule returns the live identity. One constructor raw-reference instant is captured per model, or once per complete defaults rebuild, with an explicit deterministic override. Pre-1900 actual offset uses the final standard base independently of a historical reference. The actual Java oracle and the explicit36 contextual2026f expectations remain separate.

The [containing-day postcondition correction](../../tests/fixtures/schedule-query/containing-day-correction.json) is a confirmed Java public-contract violation, separate from data differences: a non-containing result from the original two-pass time lookup returns typed NO_SESSION. Direct empty-day factories remain valid; no new search or parser narrowing occurs.
