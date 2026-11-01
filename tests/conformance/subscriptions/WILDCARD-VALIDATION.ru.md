# Валидация селекторов и canonical symbols

[English version](WILDCARD-VALIDATION.md). Английская версия является нормативной.

Дата: 2026-11-01 UTC. Область: неизменяемые [selector profiles](../../../spec/subscriptions/SELECTORS.ru.md),
stateless Penta encoder, явный STREAM All, canonical Exact и whole-event domain guards.
Ниже различаются native execution автора реализации и квалифицированные source/Java observations;
проверка документации связывает настоящие compiled excerpts, links, переводы и сохранённые прежние contracts.
Историческая desired-state/writer/collector validation сохраняет исходную область.

## Окончательные native checks

Оба полных настроенных test graphs использовали CLIENT ON, runtime OFF, JSON loader OFF и CONNECT OFF.
Это не квалифицирует полное subscription application с включёнными runtime/TLS/JSON.

| Профиль | Полный CTest | Целевые selector cases |
| --- | --- | --- |
| Windows MSVC 19.42, x64 Release | 48/48; exit 0 | 22 cases / 6 845 assertions; exit 0 |
| Ubuntu 24.04 GCC 13.3, x64 Debug ASan+UBSan | 48/48; exit 0 | 22 cases / 6 845 assertions; exit 0 |

ASan использовал `detect_leaks=0:halt_on_error=1`; UBSan — `halt_on_error=1:print_stacktrace=1`.
LSan исключён. Последнее test-only дополнение проверяет настоящий postdecoder Penta DATA star
с raw time 1, value 12 и flags 0; одобренная production implementation не менялась. Isolated TSan
прошёл 2 cases / 77 assertions на том же production code до test-only дополнения. Проверены новые
cross-thread immutable membership и compound detached-batch paths, а не конкурентные owner calls,
произвольные callbacks, все source algorithms или full-suite гарантия отсутствия races.

Закреплённый Uncrustify 0.83.0 / clang-format 20.1.8 fixed-point/check pipeline охватывает 18 выбранных
public C++ файлов. Doxygen 1.18 создал восемь проверенных API pages без warnings, включая encoder,
membership, state/writer, три collectors и queue profile contracts. Эти ограниченные результаты
не означают отсутствие warnings во всех builds или сохранение binary ABI.

## Квалифицированные production comparisons

Java reference — QD 3.355 на [закреплённом commit](../../../upstream.lock). Настоящие production
PentaCodec, BinaryQTPComposer/BinaryQTPParser и Matrix Stream paths указаны в
[контракте селекторов](../../../spec/subscriptions/SELECTORS.ru.md). Выбранные captures явно меняют
настоящий Stream wildcard switch; descriptor, local text spelling и remote permission ACK не выводятся.
Повторные captures сохраняют одинаковые provider/module source identities. Поздняя qualified edition
уточняет прежние named captures; старые наблюдения не переименованы молча в более широкую поддержку.

| Сравнение | Точная область каждой native платформы |
| --- | --- |
| Penta vectors | 154 vectors / 616 определённых coordinates |
| Полные QTP envelopes | 14 полных byte-identical файлов: шесть legacy Exact, шесть canonical Exact, STREAM All add/remove |
| Two-owner STREAM projection | 29 rows / 232 coordinates в 17 phases: девять настоящих postdecoder inputs и восемь output/count observations |
| Canonical TICKER/HISTORY Exact projection | 10 rows / 77 coordinates: все настоящие input ciphers/counts; выбранные outputs с оговорками ниже |
| Identical-input concrete DATA star | Три rows / 24 coordinates с настоящими parsed input, delivered output и count |

Итоговое strict selected-set comparison проверяет 949 semantic coordinates и все четырнадцать
полных envelopes на каждой ОС. Platform trace bytes совпадают. Сравниваются настоящий parser DATA
и настоящие native callback/collector observations; subscription aggregate API projections,
stored-symbol metadata и pre-parser operands не подменяют доставленные data.

STREAM schedule проверяет двух All owners, same-client Exact overlap, одинаковые последовательные
records, новый unencodable future symbol, другую запись, удаление первого/последнего All и сохранённый
Exact demand. Canonical Penta DATA star наблюдается как одна concrete delivery; literal FD star
остаётся другим mapping-domain case вне native canonical incoming profile. Legacy FD Exact и
canonical Penta Exact не становятся collector-key equivalent только из-за одинакового decoded UTF-16
text. Это ограничение документировано, а не обобщено на произвольных providers.

TICKER/HISTORY comparisons используют выбранные canonical Exact positives и fixed HISTORY `hs`
snapshot profile, lower bound 0, times 3/2/0, flags 4/0/10. Все настоящие INPUT ciphers и count scalars
сравниваются. HISTORY non-count projected OUTPUT ciphers исключены: synthetic END не имеет raw source
cipher. Native drained `completeRequestedRange` проверен отдельно. Java rejection/API parity,
arbitrary-adapter normalization и full native Java HISTORY producer equivalence не доказаны.

Свежий default subscription/control oracle capture и Windows/Linux native verification также прошли
после обновления source-binding provenance. Прежние control/callback bytes сохранены. Это default-path
regression evidence не расширяет выбранные All/canonical comparison domains.

## State, failure и finite-resource coverage

[Native tests](../wildcard_subscription_tests.cpp) проверяют legacy defaults и все immutable profiles,
явный All против Exact-empty/reserved leading star, literal `AB*`/`A?`, неподдерживаемый TICKER/HISTORY
All, duplicate/foreign contexts, whole writer bytes, sparse IDs и настоящие incoming cipher/text domains.
Matched Exact/All activations сохраняются один раз; удаление одной оставляет другой исходный choice,
а remove/re-add/reset не оживляет его. Structural `anyOf` проверяется отдельно от настоящего matching.

All считается одной entry с нулём desired symbol text; полные input/payload/queue/work limits сохранены.
Whole-event domain/work/arithmetic refusals проверены до ownership/candidate/epoch/stale changes,
включая сохранённые offers и HISTORY resource refusal без перехода в STALE. Queue tests различают
caller-owned direct refusal и уже принятый UNSTAGED shared-source failure с typed nested causes и
обычными gaps/discards. Неограниченная symbol enumeration, hidden credit refill и новый cache/snapshot
algorithm не выводятся. Source-compatible aggregate initialization не сохраняет object sizes:
пересобранные consumers учитывают текущие selector/membership metadata.

## Installed consumers и relocation

Свежие Release production-only packages использовали runtime/JSON OFF на Windows и Linux. Два CLIENT
пакета содержат по 43 файла, два pure CLIENT OFF — по 38. Все четыре настоящих installation directories
переименованы до свежего consumer configure/build/run; исходные четыре пути отсутствовали. Каждый
installed hash сохранён при переносе, все четыре consumers завершились с exit 0. Четыре relocated
prefixes содержат всего 162 файла; исходные копии не считаются повторно.

Настоящие imported archive locations, compile includes и link dependencies указывают на relocated
prefixes без original/source SDK leaks. Отсутствующие optional inherited-include/leaf-link-closure
properties означают отсутствие свойства, а не imported archives. Pure packages не экспортируют
queue/client targets и headers, не требуют networking SDK и Threads discovery.

[Документированные queue/collector functions](../../../spec/subscriptions/SELECTORS.ru.md#проверенный-stream-queue-fragment)
дословно извлечены из настоящего compiled consumer. Preamble использует известную запись `Sample`
с тремя полями полного type 8 и двумя Penta `ALPHA` STREAM rows, time 1/value 10. Exact и All
доставляют каждый concrete input один раз. CLIENT example сохраняет callback data, завершает конечный
local cutoff и освобождает immutable backing в другом потоке после уничтожения queue/callback owner;
pure example принимает целый admitted event и подтверждает ограниченные borrowed offers. Fragments
требуют явного caller context и не предоставляют самостоятельную `main` либо transport completion.

В выполненных проверках ошибок не обнаружено. Final integration review, приёмка владельцем и release
отделены. Throughput, RSS, hard wall-clock bound, полный network facade, peer permission-negotiation
и совместимость всех providers из этих проверок не следуют.
