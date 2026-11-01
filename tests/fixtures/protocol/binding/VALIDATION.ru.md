# Валидация Java binding

[English version](VALIDATION.md). Английская версия является основной.

Зафиксированные запуски, 2026-09-21 UTC; не независимо повторённое ревью. QD 3.355 / locked commit
0ddc0a09779fb61d74cc0b37c6449ff47dcb715f, Zulu JDK 8.0.422 и Maven 3.9.16. Чистые native reactor builds
выполнялись последовательно, cache отключён, upstream tests пропущены, source/POM не исправлялись.
Нормализован только CRLF.

- [capture-a](captures/capture-a/metadata.json) и [capture-b](captures/capture-b/metadata.json)
  независимо воспроизвели все 13 сценариев; normalized binding.tsv побайтно совпадают.
- [mismatch](captures/mismatch/result.txt) обнаружил временное изменение доставленного x со 101 на 999.
- [baseline](captures/baseline/result.txt) прошёл точный Verify с неизменными эталонами.
- [proof](captures/proof.json), [лог первого capture](captures/binding-capture-a.log),
  [лог последовательной проверки](captures/binding-evidence.log) сохраняют выполненный процесс.
- Исторические хеши покрывают все ранее существовавшие fixtures и
  upstream.lock. Результат аудита проверяет неизменность,
  текущие хешируемые probe/input/helper sources, равенство capture и соответствие проекций доставке.

Каждый запуск сохраняет raw stdout/stderr, environment.tsv, metadata, build/compiler logs и normalized
observations. Профили окружения и хеши helpers являются достоверным новым provenance; исторические sources,
captures и expectations не изменены. Первая попытка разработки не скомпилировалась из-за неверной константы
ProtocolOption; все четыре сохранённых запуска использовали исправленный отформатированный исходник.
Исходная диагностика компилятора
служит только для разбора сбоя, не является успешным capture.

В этих завершённых проверках ошибок не обнаружено. Сценарий undescribed намеренно сообщает один
corrupted-message callback, неизвестные записи намеренно логируют пропуск. Штатные наблюдения обычного
типа 8 поддерживают [ограниченные утверждения](COVERAGE.md); промежуточные повторные записи подтверждены
исходниками, а не внешним наблюдением. Upstream suite, provider/property matrix, metadata, conversions,
defaults, reset, transport и C++ event decoder не проверялись и не добавлялись.
