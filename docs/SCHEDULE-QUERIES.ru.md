# Дни и сессии расписания

[English version](SCHEDULE-QUERIES.md) является нормативной.

`qdcxx_schedule` строит владеющие `Day` и `Session` по полной [грамматике правил](SCHEDULE-RULES.ru.md).
Фильтры — отдельная возможность. Запросы используют [явный общий timezone-компонент](TIME-ZONE-DATA.ru.md);
контекст только для parser остаётся без файловых обращений.

Создайте один `ScheduleTimeZoneData::initialize(absolutePath)`, передайте успешную capability в
`ScheduleDataContext::create(data)`, затем используйте `Schedule::getInstance(context,u"XNYS()")` и
`getDayByYearMonthDay(20241104)`. Каждый шаг возвращает `std::expected`. Capability можно копировать
в несколько независимых календарных контекстов. Нативная инициализация допускает одну попытку на процесс:
повторно используйте успешную capability. [Полный пример](../examples/schedule_queries/README.ru.md)
принимает Windows wide path, печатает сессии и содержит EN/RU-справку без рабочих входов.

## Идентичность, снимки и обновления

`Schedule` — живая identity. `Day` владеет ею и неизменяемым поколением календарных правил;
`Session` владеет Day и индексом неизменяемой записи. Цикла владения нет. Копии и перемещения
удерживают модели без дублирования записей; перемещённый исходный handle остаётся пригодным.
Metadata и прямые запросы Schedule видят текущее полностью опубликованное поколение.
Значения Day и её навигация остаются на исходном поколении. `Day::getSchedule()` возвращает
владеющий живой Schedule; новый прямой запрос через него может увидеть обновление.

Java сохраняет поля старого Day, но навигация после обновления проходит через живой Schedule.
C++ явно закрепляет и навигацию, расширяя стабильность снимка. Actual hot-update oracle сохраняет
старые/новые Java-результаты; тест проверяет это отличие, не переписывая Java.
Равенство Day соответствует identity плюс day-id даже между поколениями, поэтому равные Day могут
содержать разные версионные значения. Session дополнительно сравнивает тип и endpoints.
Когда важно поколение, используйте `getDefaultsVersion()`.

Типизированный `setDefaults` перестраивает удерживаемые identities и публикует один полный bundle.
При ошибке живые identities, metadata и defaults generation не меняются. Обычная фабрика может
предварительно удалить неактивные cache entries, а затем отказать: rollback cache/accounting не обещан.
Day/Session удерживает Schedule как живой handle, защищая identity от удаления как неактивной.

## Календарное поведение и диапазоны

Сохранены Calendar noon anchor, пролептические метки DayUtil, все weekday/date overrides,
holiday/short-day стратегии, early close и объединение нерабочих дней. Несуществующее локальное время
переносится вперёд через gap; overlap выбирает позднее вхождение. Пропущенная гражданская дата
сохраняет метку Schedule. Нулевая trading-запись остаётся в списке и торговой по типу.
`Session::isEmpty()` означает `start >= end`: прямые no-trading определения могут дать инвертированный
интервал в DST gap. Пустые/инвертированные интервалы не содержат моментов.

| Фабрика | Включённый диапазон аргумента |
| --- | --- |
| `getDayById` | -719160..2932894 |
| `getDayByYearMonthDay` | 10103..99991229; неверные поля нормализуются как в source |
| `getDayByTime` / `getSessionByTime` | -62135424000000..253402128000000 Unix milliseconds |

Верхний time может вернуть метку 99991230, которую YMD/id-фабрики отвергают. Previous/next Day
применяет диапазон прямого day-id. Навигация Session сохраняет порядок записей, включая пустые,
затем переходит в соседний закреплённый Day. Принадлежность полуоткрытая `[start,end)`.
`Day::getSessionByTime` возвращает `NO_SESSION`, если момент не входит ни в одну запись.

Один raw-reference instant конструктора модели захватывается вне publication locks. Без override
новая модель один раз читает текущие часы; cache hit и запросы дат часы не перечитывают.
Весь atomic defaults rebuild использует одно значение. Для детерминированного сравнения задайте
`ScheduleQueryOptions::rawOffsetReferenceTime`. Reference влияет на raw metadata/noon anchor
конструктора, но не ограничивает даты запросов. Фактический offset до 1900 использует конечную
standard base независимо от исторического reference конструктора.

## Конечная работа и удержание

`ScheduleLimits` отдельно ограничивает работу запросов/конверсий, накопленные staging bytes,
записи на Day, удерживаемые Day и session records, weak cache entries и снимки cache.
Нулевой `dayCacheEntries` отключает cache. Старые поколения используют тот же ledger контекста:
для новых моделей освобождайте handles либо задавайте достаточные лимиты. При конкуренции cache
может вернуть полный leased Day без сохранения в cache. Нет workers, callbacks, неявного чтения файлов
и бесконечных retry. Каждый поиск ограничен `queryWork`; не содержащий момент результат
time lookup возвращает `NO_SESSION` согласно containing-day postcondition.
Владеющие фабрики возвращают типизированные resource/allocation/range/data ошибки.
`Day::getSessions()` создаёт caller-owned vector, ограниченный числом записей Day, и может бросить
`std::bad_alloc`; `getFirstSession` и `getLastSession` копируют только handles.

Logical model accounting не включает overhead стандартного allocator/control blocks/capacity
и общее native timezone backing; это не configurable exact heap/RSS cap. Произвольные caller-owned
копии handles/vector не считаются отдельными моделями контекста. Ошибка кандидата не публикует
неполный Day/Session. Инициализация и native backing описаны отдельно в timezone-руководстве.

## Доказательства совместимости

[Actual Java oracle](../tools/upstream/schedule-query-oracle/README.ru.md) фиксирует 2303 проекцию
закреплённых production Schedule/Day/Session, все 631 допустимые identity и полные SystemV transition facts.
[Неизменённый Java fixture](../tests/fixtures/schedule-query/oracle.tsv) использует Java8 TZDB2024a.
Ровно 36 [контекстных ожиданий данных](../tests/fixtures/schedule-query/tzdb2026f-provenance.json)
классифицируют отдельные результаты Canada, Cuba, Morocco и Paraguay по исходникам TZDB2026f.
Это различия данных/reference отдельных случаев, не общая поблажка и не подтверждённые ошибки Java.
Покрыты DST gaps/overlaps, исторические/pre-1900 и 1582 метки, границы и нормализация,
сдвиги 60000 дней, стратегии, session boundaries и hot updates. Полное сравнение каждого transition
во всех зонах двух наборов данных не заявлено.

[Исправление containing-day](../tests/fixtures/schedule-query/containing-day-correction.json)
фиксирует подтверждённое нарушение public contract отдельно от timezone data differences.
Java `getDayByTime(0)` для `(tz=UTC;td=1234567;0=d00000000)` возвращает пустой Day1, не содержащий0,
вопреки Javadoc метода. C++ проверяет postcondition после того же two-pass lookup и возвращает
`NO_SESSION`. Прямые id/YMD-запросы сохраняют допустимый пустой Day; session lookup уже отвергает
этот момент. Actual Java witnesses и native результаты до исправления сохранены без правки Java
fixture. Дополнительного поиска или ограничения parser нет.
