# Явные timezone-данные и bindings

[English version](TIME-ZONE-DATA.md) является нормативной.

Всегда экспортируемый C++23 target `qdcxx_time_zone_data` предоставляет неизменяемый владеющий
`org::ttldtor::qdcxx::time::ZoneBinding` из `<qd/time/time_zone_data.hpp>`. Зависимостей от event values,
Schedule, runtime SDK и transport нет. Без backend доступны fixed identities закреплённой Java policy
и нормализованные custom IDs `GMT+HH:MM`/`GMT-HH:MM`. Offset выражен целыми секундами;
произвольные sub-second fixed offsets этот API не представляет.

Нативный backend включается `ORG_TTLDTOR_QDCXX_BUILD_TIME_ZONE_DATA=ON` либо
`ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON`. Другой consumer сам по себе его не включает.
`find_package(qdcxx REQUIRED COMPONENTS time_zone_data)` проверяет всегда доступный target;
для обязательного установленного backend запросите `time_zone_data_backend`. При его отсутствии
configure объясняет нужные параметры. С backend OFF `TimeZoneData::initialize` возвращает `NOT_READY`
без чтения пути, а допустимые non-fixed IDs без data возвращают `NOT_READY`.

## Одна инициализация, владеющие bindings

`ZoneBinding::create(u"GMT+05:30",nullptr,{0,128})` работает без нативной инициализации;
`getOffset(0)->totalSeconds` равен19800. Для named zones один раз вызовите
`TimeZoneData::initialize(absoluteDataDirectory,limits)` и сохраните успешную capability.
`data.bind(id,options)` эквивалентен `ZoneBinding::create(id,&data,options)`.
Результат владеет точным UTF-16 ID, raw-reference instant и общим неизменяемым provider profile.
Исходные ID view и data pointer не удерживаются. Копии и перемещения сохраняют владение,
перемещённый исходный handle остаётся пригодным. Binding живёт дольше исходной переменной data.
Для одновременной замены handle нужна внешняя синхронизация; запросы через стабильные копии совместимы.

Date/TZDB-типы не входят в public API. `getOffset(utcMillis)` возвращает копию
`TimeZoneOffset{totalSeconds,referenceRawSeconds}` без чтения часов/файлов, callbacks, workers и
удержания query state. `getQueryBounds()` возвращает включённые границы конкретного binding.
Fixed bindings покрывают весь signed64 millisecond domain. Named bindings — его минимум до конца
native year32766, оставляя запас следующего года; в nanoseconds моменты не переводятся.
До1900 применяется Java ZoneInfo final raw-base convention. SystemV использует captured legacy
transitions и source recurrence вместо современных US IANA rules. Three-letter aliases следуют
фактическому mapping закреплённой Java, а не новому IANA-only пути.

`rawOffsetReferenceTime` явно заменяет часы конструктора модели; без override текущее Unix milliseconds
захватывается один раз. Raw metadata использует standard history с
[обоснованными rearguard conventions](../resources/time-zone-data/calendar-offset-conventions.json),
включая negative SAVE eras. Это отдельное от actual historical offset значение;
универсальное `IANA offset - SAVE` без conventions неверно. До границы Java transition table либо
после native envelope reference metadata использует конечную standard base. Reference не ограничивает
даты запросов. Ошибки invalid-ID/not-ready/range/resource/allocation/data типизированы;
`maxIdUnits` проверяется до копирования ID.

## Точная offline-зависимость и данные

[Source manifest](../third_party/schedule-date-3.0.5/manifest.json) закрепляет Howard Hinnant date/tz
v3.0.5, полный commit `179a6b921ccd6179b5b04dce812a89f71a51b4df`, original/patched hashes,
MIT license и полный сопровождаемый patch. [Data manifest](../resources/time-zone-data/tzdb-2026f/manifest.json)
закрепляет IANA TZDB2026f, архив и отдельные файлы по hash. Исходники и данные отслеживаются;
configure ничего не приобретает и не разрешает floating versions. Builder проверяет manifests,
генерирует approved-byte backing bulk-операциями и компилирует приватный namespace
`org_ttldtor_qdcxx_date_305_v1`. Один static archive не изолирует глобальный native registry;
изоляцию обеспечивают namespace и controlled-input patch.

Native initialization проверяет явный абсолютный каталог, отвергает symlinked profile files
и читает ровно11 разрешённых файлов во владеющую память до native parsing. До роста проверяются bytes,
line/record counts и cumulative native insertions. Все344zones инициализируются и253links разрешаются
до READY native registry; public capability дополнительно разрешает полный631-ID Java mapping,
включая fixed/legacy identities, до публикации. Одна native admission attempt/generation на процесс.
Ошибка после native admission не публикует public capability и не запускает retry/reload/второе поколение.
Неверная длина пути, отвергнутая до native admission, не потребляет попытку.
Нет CURL, downloads, home/OS discovery, current-zone mapping и автоматического обновления.

Default bounds:11files,1100000bytes,1024bytes/line,35000lines,12000source records,600zones,400links,
10000rules,32768cumulative native insertions. Точный snapshot:855823bytes,19069lines,2616source records,
2596adjusted native rules. `getLogicalBytes()` считает отдельные owned model/ID поля binding;
shared profile/static backing и overhead стандартного allocator/control blocks/capacity/runtime
учитываются отдельно. Эти лимиты и измеренные allocation envelopes не являются configurable hard
native heap/RSS quota. Ordinary allocation failure закрывает операцию без неполного результата.

## Установка и перенос

Backend-enabled install содержит `share/qdcxx/time-zone-data/tzdb-2026f`, IANA license/manifest,
Java compatibility observations/conventions и MIT license/source manifest/patch/patched source
в `date-3.0.5-notices`. Копируйте установленное поддерево `share/qdcxx/time-zone-data` вместе с
приложением либо переносите SDK prefix целиком. Передавайте новый абсолютный каталог `tzdb-2026f`
явно. Установленные библиотеки/headers не содержат source-tree/private working paths и не ищут
каталог неявно. Политику пути executable/configuration выбирает приложение.

Обновление timezone требует maintainer-reviewed offline смены source/data/manifest/patch,
проверок, deploy и restart процесса. MPL market-calendar resource и типизированные календарные
поколения имеют отдельный механизм обновления. Сам parser identity table не квалифицирует native
lookup profile и не доказывает совместимость разных TZDB datasets.
