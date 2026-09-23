# Авторская проверка oracle настраиваемых событий

[English version](VALIDATION.md). Английская версия нормативна.

Начальный HEAD: `e5a3764992ba3a90a07d3d092a4383e37a901de2`. Выполнено 2026-09-23 UTC с закреплённой
QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, Zulu JDK 8 и Maven 3.9.16.
Для форматирования использованы OpenJDK 23.0.2 и закреплённый google-java-format 1.36.1.
Это авторские проверки; независимое ревью ожидается. В успешных проверках ниже ошибок не обнаружено.

| Выполненная проверка | Результат |
| --- | --- |
| Java Format, затем Check | PASS, 17 обнаруженных Java sources; исторические Java файлы неизменны |
| Clean capture A `.work/event-mapping-final-a` | PASS, clean upstream build, прежний tape Verify, 21 fresh-JVM сценарий, 135 артефактов |
| Clean capture B `.work/event-mapping-final-b` | PASS, та же процедура; 135 артефактов побайтно совпали с A |
| `.work/event-mapping-negatives` | Все 14 мутаций отвергнуты; baseline проходит, hashes expectations неизменны |
| Полный отрицательный Verify с отдельной profile-flags expected copy | Ожидаемый exit1; изменённые event flags противоречат фактическим значениям delegate/FileReader |
| Полный baseline Verify | PASS с неизменёнными repository expectations; все 135 артефактов совпадают |
| Ссылки/пробелы авторских документов и состав принадлежащих файлов | Проверены отдельно от raw evidence; исторические доказательства не нормализовались |

[Ссылки на captures](CAPTURES.ru.md) сохраняют фактические журналы и metadata.
[Команды воспроизведения](../../../../tools/upstream/event-mapping-oracle/README.ru.md) используют
JDK 8 для oracle и отдельный JDK 21+ для форматирования. Каждый сценарий имеет heap 256 MiB,
таймаут 30 секунд и лимит 64 callback records. Verify не переписывает expected files.
Все сохранённые source hashes описывают реальные запуски; поздние дополнения tools/reports
не переписывают старые metadata, будто они существовали при более ранних captures.

При разработке empty-wire probe сначала использовал символ, требующий text, с null text argument;
это исправлено до clean captures. Синтетический XML string137 сохранён отдельным неподдерживаемым
примером. Поздний повтор для сохранения журнала пересёкся с clean rebuild тех же upstream classes
и завершился ClassNotFoundException; этот запуск исключён из acceptance. Требуются последовательные
запуски, и финальные сохранённые Verify runs выполнены последовательно. Эти ошибки организации
проверок отделены от намеренного семантического отрицательного Verify.

Выводы об optional/default/representation, properties/XML, strings/flags/time и rejection ограничены
[покрытием](COVERAGE.ru.md). Отчёт не заявляет результаты C++ compiler/sanitizer/CLI, произвольные
providers, дополнительные типы, индексные схемы или полную поддержку частного tape.
Существенные различия configuration timing и local absence/wire omission записаны в двуязычных
контрактах; дублирующая ce-compound learning не создана. Staging и commit не выполнялись.

