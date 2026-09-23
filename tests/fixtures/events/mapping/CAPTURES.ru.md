# Captures настраиваемых событий

[English version](CAPTURES.md). Английская версия нормативна.

[135 expected artifacts](expected/) побайтно скопированы из чистого capture A и воспроизведены
независимым clean capture B. Каждый запуск пересобрал неизменённый закреплённый upstream reactor
и прошёл неизменённый baseline tape Verify. Каждый из 21 сценария имел отдельную JVM.
Исходные файлы, UTF-16 units, timestamps и байты вывода не нормализованы. upstream.lock неизменён.

- [Capture A](evidence/a/event-metadata.json), [baseline build/provider/classpath provenance](evidence/a/metadata.json).
- [Capture B](evidence/b/event-metadata.json), [baseline provenance](evidence/b/metadata.json).
- [Сравнение captures](evidence/compare.txt).
- [14 отклонённых мутаций](evidence/negative-results.json), [неизменённый baseline](evidence/negative-baseline.txt).
- [Полный отрицательный Verify](evidence/negative/event-metadata.json), [ожидаемое отклонение](evidence/negative/event-verify.stderr).
- [Полный положительный Verify](evidence/verify/event-metadata.json), [PASS](evidence/verify/event-result.txt).
- [Неизменность expectations после отрицательной проверки](evidence/after-negative-unchanged.txt).

Соседние stdout/stderr, build logs, environment inventory и metadata сохраняют фактические UTC
timestamps, пути, команды, source/tool/input hashes, classpath content/dependencies и provider identities.
Сохранены 76 исходных FileWriter files, 38 отдельно построенных wire-omission files и все 21 observation TSV.
Прямые mapping probes явно обозначены phase. Наблюдения package-private flags/sequence вызывают
неизменённые upstream методы; harness не реализует delegates заново. Missing-required rejection —
сообщение harness об отсутствии совместимого production delegate, а не утверждение, что сам
DefaultRecord обязательно бросает исключение.

Исходный development run выявил, что синтетический XML тип string разрешается в полный137;
источник сохранён как unsupported-extension.xml, а фактическое наблюдение неподдерживаемого профиля —
xml-unsupported. Отдельный совместимый extension.xml использует utf_char_array полного10.
Исходные dxlink/default примеры не подгонялись под ограничения C++. Ранние development runs
не являются clean reproducibility доказательствами. Новый oracle отделён от исторических harness,
fixtures и lock. Частный tape не читался.

