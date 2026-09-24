# Авторская валидация QDCXX-030

[English version](VALIDATION.md). Английская версия нормативная.

Дата: 2026-09-24 UTC. U1–U4 назначенного плана
реализованы и проверены автором. **Независимая приёмка остаётся открытой.** Исходный HEAD:
`2ac1a85ea29c4553dd26db880e4e1fe0d2e03f50`; ветка `task/qdcxx-030-java-schema-export`.
Staging, commit, push и merge не выполнялись. [Полный список файлов](changed-files.txt) включает
изменённые отслеживаемые файлы и все новые результаты; временные материалы остаются в игнорируемом
`.work/schema-export-030/`. Все пути доказательств ниже, кроме ссылок, заданы относительно этого
временного каталога. Временные журналы не включаются в коммит и требуют повторного получения на другой машине.

## Реализованные границы и доказательства

- U1: [JAVA-EXPORT](../../../spec/schema/JAVA-EXPORT.ru.md) определяет точные типы, источники, lookup
  relation, логическую проекцию, бюджеты и публикацию. `ProjectionCharacterization.java` наблюдает
  закреплённые production objects независимо от экспортёра: накопленные aliases, scoped keys,
  wide/VOID/int/object slots, перенумерацию ID в исходном порядке и явные границы отказов.
- U2: повторно используемый экспортёр только для чтения и команда в свежей JVM используют настоящие
  пути `DXFeedScheme`, неподключённого endpoint и `DXScheme.Loader`. Полное проверенное отображение
  `DefaultRecord.fieldsByName` даёт lookups; проверенные физические хвосты описаны отдельно. XML closure
  проверяется и копируется в snapshot до upstream construction. Публикация записывает оба файла до
  перемещения нового каталога и сохраняет существующие назначения. Production C++ и native grammar не менялись.
- U3: [повторно используемые входы и новые ожидания](../../fixtures/schema/export/) охватывают настроенные
  Quote/Profile, смешанную XML-запись с дополнительным полем, scalar/control/supplementary names и пустую
  схему. Неизменённый `EventMappingOracle` предоставляет configured production `FileWriter`/`FileReader`
  и event observations. Дополнительный capture предоставляет production writer/reader observations
  для custom record. Генерация JSON, raw field observations и event projections — разные доказательства;
  Java RAW callback fanout не становится контрактом доставки C++.
- U4: [переносимые инструкции](../../../tools/schema-export/README.ru.md), документы сборки/статуса/задач
  и этот отчёт имеют синхронные EN/RU версии. Существующие загрузка JSON C++, общий просмотр и prepared
  API Quote/Profile используют сохранённые captures без повторной генерации Java.

Launchers проверили JDK 8 и закреплённые origin/tag/commit. Подготовка Windows выполнила чистую сборку
upstream Maven reactor с Zulu 8u422 и Maven 3.9.16; `prepare/classpath-receipt.json` фиксирует точные
деревья классов/ресурсов 14 модулей. Каждый export/capture заново проверял их хеши. Linux command tests
использовали Zulu 8u504, чистый checkout на Linux filesystem того же закреплённого commit и идентичные
проверенные переносимые байты классов/ресурсов из Windows-сборки. Экспортёр компилировался и выполнялся
на Linux; отдельная сборка Maven reactor на Linux **не выполнялась**. Небольшой сохраняемый
[capture provenance](../../fixtures/schema/export/expected/provenance.json) фиксирует фактические
хеши исходников, входов и classpath. Ограниченные правила `.gitattributes` сохраняют байты новых
исходников/captures между платформами.

## Выполненные проверки

| Проверка | Выполненный профиль и результат | Временные доказательства |
| --- | --- | --- |
| Java projection, exporter, sources и command | Windows JDK 8u422 и Linux JDK 8u504: прошли; восемь source scenarios, дополнительно symlink containment на Linux | `commands-windows-settled/`, `commands-linux-native/` |
| Граница launcher | Windows: 12 отрицательных случаев, равенство stdout и пустого stderr для no-argument/help, сохранение пустого аргумента как usage error: прошли | `launcher-tests-final/` |
| Два свежих production captures | Windows: 19:16:43Z–19:16:49Z и 19:16:52Z–19:16:57Z; точный Verify состава/байтов между запусками и с новыми fixtures прошёл | `capture-settled-a/`, `capture-settled-b/` |
| Намеренное изменение ожидания | Изменённый `custom/stream.tsv` отвергнут с `Capture mismatch`; получен ожидаемый ненулевой код | `mutated-settled/` |
| Полный CTest | Windows MSVC 19.42 Release, JSON и оба инспектора ON: **45/45 прошли**, 461.94 секунды | `windows-{configure,build,ctest}.log` |
| Полный CTest | WSL Ubuntu 24.04, GCC 13.3 Debug ASan+UBSan, JSON и оба инспектора ON: **45/45 прошли**, 565.48 секунды | `linux-{configure,build,ctest}.log` |
| Production-сборки | Windows Release и Linux Release; `BUILD_TESTING=OFF`, JSON ON/OFF, оба инспектора ON: все четыре прошли | `windows-prod-{ON,OFF}-{configure,build}.log`, `linux-prod-{ON,OFF}-{configure,build}-final.log` |
| Аудит процессов/зависимостей | 14 непустых traces configure/build/CTest: нет процессов Java, javac, Maven, Python или PowerShell; в production cache нет doctest; JSON OFF не содержит simdjson; нет production каталога получения `_deps` | соответствующие `*.exec.tsv`, `trace-audit.txt`, четыре production `CMakeCache.txt` |
| Форматирование | Закреплённый google-java-format 1.36.1 Format, затем Check с JDK 21.0.4: 24 файла прошли; native Uncrustify 0.83.0, затем clang-format 20.1.8 Format нового C++ теста, Check: 84 файла прошли | вывод форматтера и `cpp-format-final.log` |
| Итоговый состав | Проверены ссылки и whitespace авторских документов, список новых файлов, unstaged/staged diff и неизменность исторических/production путей | итоговый авторский аудит |

Source/command checks охватывают точные и превышенные на единицу пределы проекции/вывода/входов,
некорректные и неподдержанные records/types/maps, неизвестный/повторный выбор, приоритеты properties,
неактивные/неизвестные настройки, XML imports/overlays и pinned dxlink, remote/escaping/cyclic inputs,
DTD/entities/XInclude, ограничения schema location и неудачную публикацию. На Windows и Linux
проверены сохранность существующего файла, пустого/непустого каталога и очистка staging. На Linux
дополнительно проверен отказ при symlink escape; создание symlink было недоступно учётной записи
тестов Windows и не заявляется как выполненная проверка.

Два новых CTests проверяют production tapes в обоих прежних режимах, целиком и по одному байту,
TSV и human inspection, явные экспортированные event IDs, сохранённые prepared mappings после
уничтожения исходной схемы/JSON, custom/extra values, пустую схему/scalar names и обнаружение мутаций
full type/alias target/value. Существующие test helpers предоставляют независимый event comparator.

Обычные проверки использовали существующий native `qdcxx_no_python_guard --deny-interpreters` вокруг
подготовленного `qdcxx-run-utc --log ... -- <executable> ...`. Имена исполняемых файлов во всех
14 traces дополнительно проверены на Java/javac/Maven. ASan использовал `detect_leaks=0`; UBSan —
`halt_on_error=1:print_stacktrace=1`; GCC — `-fno-pie`/`-no-pie`. Это не покрытие LeakSanitizer.
Production JSON ON использовал ранее закреплённые локальные исходники simdjson 4.6.4; OFF их не получал.
Необязательные Java checks и PowerShell formatter отделены от обычных команд C++.

## Решения, ограничения и статус ревью

Отклонений, требующих изменения native contract, upstream lock или нового production-компонента,
не потребовалось. Точное локальное XSD declaration на JDK 8 требует upstream JAXP schema access `file`;
доступ DTD/stylesheet запрещён. Защищённая предварительная проверка принимает только это declaration
и копирует закреплённый XSD в частный snapshot. Эта граница для доверенных локальных входов описана;
изоляция произвольного враждебного XML и строгий предел памяти всего процесса не заявляются.
XML игнорирует `dxscheme.shares`, поэтому команда его отвергает; selectors принимают только описанное
подмножество с `*`. Полные default/dxlink schemes неподдержаны: выбор явный, без скрытой фильтрации.

Первый Windows bootstrap не получил доступ к SDK в ограниченном runner; разрешённый повтор прошёл.
Первый WSL classpath на `/mnt/c` превысил неизменённый бюджет endpoint-команды 30 секунд; полный
command suite прошёл после перемещения идентичного проверенного upstream classpath на Linux filesystem.
Переменная временного Linux matrix-скрипта ошибочно изменила один аргумент каталога сборки; исправлены
и повторены только production phases. Полный CTest без изменения кода не повторялся. После последних
упрощений исходников повторены затронутые Java checks и свежие captures; изменился только provenance
нового capture, байты production observations сохранились.

В перечисленных успешных проверках ошибок не обнаружено. GCC Release выдал существующее предупреждение
`-Wstringop-overflow` в неизменённом `src/core/wide_decimal.cpp:68`; оно не исправлялось и независимо
не диагностировалось в этой задаче. macOS, принудительное исчерпание JVM/авария при публикации и
параллельная враждебная модификация filesystem не проверялись. Частный tape не читался. Исторические
fixtures, upstream Java tools, `upstream.lock`, production `src/` и `include/` неизменны.

Автор просмотрел полный diff и новые файлы на корректность, границы, стиль/скобки и лишнюю сложность;
целевые замечания по упрощению применены и перепроверены. Это не закрывает независимую приёмку.
`ce-compound` рассмотрен и пропущен: повторно используемые решения уже записаны в нормативном контракте
экспорта и целевых тестах, отдельная заметка дублировала бы их. Следующая задача не начиналась.

Предлагаемый scope коммита: ровно [changed-files.txt](changed-files.txt), после независимой приёмки.
Предлагаемое сообщение: `feat(schema): export resolved Java schemes to native JSON`.
