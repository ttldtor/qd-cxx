# Валидация офлайн-доказательств Java

[English version](VALIDATION.md). Английская версия является нормативной.

Проверки выполнены 2026-09-21 UTC до реализации CLI; это не независимое ревью.
В финальных проверках ниже ошибок не обнаружено. [Покрытие](COVERAGE.ru.md) ограничивает выводы.

Две последовательные clean native reactor сборки locked QD 3.355 под JDK 8, с отключёнными
upstream-тестами и build cache, дали одинаковые сырые .qtp байты и нормализованные наблюдения:
[metadata capture-a](captures/capture-a/metadata.json), [metadata capture-b](captures/capture-b/metadata.json).
Сохранены build logs, compiler/JVM environment, команды, хеши исходников/входов, harness classes,
содержимого classpath и зависимостей. Нормализован только TSV CRLF→LF; бинарные байты неизменны.
Workflow composer/parser выдал семь записей из восьми входных, включая пропуск неизвестной записи.

Verify отклонил временную мутацию ожидаемого значения 22,44,0→22,45,0:
[результат mismatch](captures/mismatch/result.txt), [metadata](captures/mismatch/metadata.json).
Затем прошёл неизменённый baseline: [результат](captures/baseline/result.txt),
[metadata](captures/baseline/metadata.json), [proof](captures/proof.json),
[UTC-лог workflow](captures/offline-evidence.log), [лог первого clean capture](captures/offline-capture-clean-a.log).
Фактические наблюдения всех четырёх запусков совпадают с неизменяемыми ожиданиями; отрицательный
запуск сравнивает их с отдельным намеренно изменённым каталогом. Baseline и временные ожидания
не перезаписывались.

Начальная попытка harness передавала null в visitHeartbeat и завершилась ошибкой до наблюдений;
[диагностика](captures/initial-failed/stderr.txt), [лог](captures/initial-failed/run.log).
Она не считается успешным capture. Финальный исходник использует пустой HeartbeatPayload
(байты не выдаются), отформатирован до обоих clean captures и прошёл финальный format Check.
[Лог Java Format](captures/offline-java-format.log). Upstream-исходники не исправлялись.

Повторение с PowerShell 7.4+, JDK 8, Maven и чистым pinned checkout:

```powershell
python tools/run_utc.py --log <new-log> -- pwsh -NoProfile -File tools/upstream/offline-inspection-oracle/Invoke-Oracle.ps1 -Mode Capture -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <new-directory>
python tools/run_utc.py --log <new-log> -- pwsh -NoProfile -File tools/upstream/offline-inspection-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream <checkout> -JavaHome <JDK8> -Maven <mvn.cmd> -RunDirectory <another-new-directory>
```

Для negative Verify скопируйте expected во временный каталог, измените TSV-значение и передайте
`-ExpectedDirectory <copy>`; требуется VERIFY FAILED, затем повторный исходный baseline. Capture
не продвигает ожидания автоматически. Origin/commit/tag/version и чистота исходников проверяются
до и после сборки. RunDirectory должен быть новым и отдельным от expected.

Исторические хеши сохраняют все прежние tracked fixtures
и upstream.lock. Хеши артефактов покрывают новые сохранённые captures.
Аудит conformance проверяет текущие
хеши harness/helper/input по фактической metadata, равенство captures, историю и финальные исходники.
Нет заявлений о полном запуске upstream-тестов, Java callback parity ошибок C++, огромной памяти или timing.
