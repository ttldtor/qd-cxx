# Event text validation

[Русская версия](EVENT-TEXT-VALIDATION.ru.md). English is authoritative.

Status: UTC/fixed and named-context qualification for all five supported event types. Final package qualification passed; independent API/context/lifetime review remains pending. This record does not establish release.

## Accepted numeric checkpoint

The following earlier focused results cover the independently reviewed numeric core and the original four event leaves. The complete numeric/helper body, double formatter and shared numeric dependencies remain unchanged through the additive Summary integration. The full numeric campaign was not repeated for that integration.

| Profile | Result |
| --- | --- |
| Windows x64 MSVC 14.42 Release /MD | Six focused CTest checks passed: production oracle/API, four rounding modes, ordinary-allocation fault sweep, oracle input-free help, example input-free help and example run |
| Ubuntu 24.04 x86-64 GCC 13.3 Debug ASan/UBSan, LSan enabled | Five focused CTest checks passed; the Windows PowerShell oracle-help check is not registered in this Linux profile |
| Original public API corpus | Four test cases, 421249 assertions passed on both C++ profiles |
| Four caller rounding modes | 115070 actual Java double/WideDecimal observations per mode on each OS, zero differences; only the disposable test process changes/restores its thread environment |
| C++ formatting | Explicit owned-file Uncrustify 0.83.0 then clang-format 20.1.8 Format/Check passed |
| Formatter wrapper | Existing positive/negative/pinned-pipeline/mutation regressions passed after adding the exact new example directory |

## Additive Summary qualification

| Profile | Result |
| --- | --- |
| Windows x64 MSVC 14.42 Release /MD | Focused Summary text check passed: two test cases, 498 assertions |
| Ubuntu 24.04 x86-64 GCC 13.3 Debug ASan/UBSan, LSan enabled | The same focused Summary check passed: two test cases, 498 assertions |
| Actual Java | Capture and subsequent Verify passed for 181661 observations, including 168 new Summary rows under the six explicit contexts and complete en_US/fr_FR/ar_EG locale checks |
| Summary scenarios | Default/filled values, all PriceType combinations/raw flag patterns, direct long OpenInterest setters including 3000000000 and both signed64 extrema, strict UTF-16/UTF-8 and exact finite output bounds |

No errors were detected in these focused checks. [Provenance](../fixtures/event-text/provenance.json) binds current Java sources/classes, harness/launcher and captures. The [reference decision](../../docs/decisions/ADR-0011-java-event-text-reference.md) separates 880 numeric runtime-version difference rows from three confirmed TimeFormat cache-defect exceptions; both exact proof fixtures remain byte-identical to the accepted numeric checkpoint. Ordinary rows require strict equality; cache exceptions compare C++ with actual fresh-instance Java results.

The [coverage record](../fixtures/event-text/README.md) describes finite inputs. The [installed consumer](event_text_consumer/CMakeLists.txt) compiles every installed project header separately before OS headers, and exercises the UTF-8 example and Summary long text. Final package results follow below. Earlier numeric review does not replace final independent API/context/lifetime review.

## Shared named-context integration

The actual Java corpus now contains 182957 observations: the earlier 181661 plus 1296 strict observations across eight named zones. Separate Java 8 and Java 25 captures for those eight profiles were byte-identical. MSVC 14.42 Release /MD passed six relevant checks; GCC 13.3 Debug ASan/UBSan with LSan enabled passed the five applicable checks. Named tests compare complete UTF-16 events and timestamp boundaries after destroying the original data handle and zone-ID strings, then exercise copied/moved contexts concurrently. The accepted numeric helper body and double-format header remain unchanged.

The focused GCC Summary subprocess initially reached its former 30-second limit during cold backend admission; it passed in 44.26 seconds after using the same 120-second bound as the main oracle test. The installed named consumer uses that bound too. These limits apply only to disposable checks, not to the synchronous public API.

A real Windows SDK min/max-macro probe reproduced six invalid numeric_limits calls in serial_field_type.hpp. Parenthesizing only those member names fixed the same translation unit; event_text.hpp also compiled with Windows.h first and min/max macros active, without NOMINMAX. The public header declares no macro suppression.

The initial generated-documentation build passed with 710 linked pages and 2664 assets. Its 35 existing duplicate-anchor warnings contain no EventText/ADR-0011 diagnostics.
## Final package and disabled-backend checks

| Profile | Result |
| --- | --- |
| MSVC 14.42 Release /MD, backend/feed enabled | Installed and physically moved consumers each passed 3/3 checks; all 53 installed project headers compiled standalone; 24 project archives plus the reused qualified c-ares archive were installed |
| GCC 13.3 Debug ASan/UBSan, LSan enabled, backend/feed enabled | The same installed/moved 3/3 checks and 53 standalone headers passed; all producer project archives were built |
| MSVC backend OFF | Six native EventText checks passed; installed/moved consumers each passed 3/3, with all 44 installed project headers compiled standalone and 19 project archives installed |
| GCC sanitizer backend OFF | Five native EventText checks and installed/moved 3/3 checks passed; all 44 installed project headers and all 19 producer project archives compiled |
| Windows SDK macro regressions | EventText and serial_field_type public headers compiled after real Windows.h with active min/max macros, without NOMINMAX; both focused units are part of the installed/moved Windows build |
| Actual Java reference | Capture followed by Verify passed for the final 182957-row fixture, 14 explicit profiles and three locales; no fixture differences |

Actual producer caches were checked for backend/feed ON/ON and OFF/OFF. Disabled-backend build graphs contain no date-provider source; UTC/fixed contexts and typed unavailable-named errors remain usable. Named consumers initialize only the explicit installed/moved data root. No host timezone discovery is exercised.

The all-header result establishes standalone self-containment before OS headers. It does not establish Windows-first interoperability for every optional Client header: the existing ProgressKind::CALLBACK name collides with the Windows SDK CALLBACK macro. No Client API rename or macro suppression is part of this change. Final independent review remains separate from these author checks.