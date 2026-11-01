# проверка tape reader

[English version](VALIDATION.md). Английская версия является основной.

исходные запуски 2026-09-22 UTC. Независимое ревью ожидается. [Покрытие](COVERAGE.ru.md), [Java-доказательства](../../../fixtures/protocol/tape-reader/VALIDATION.ru.md),
[контракт](../../../../spec/protocol/TAPE-READER.ru.md), [архитектура](../../../../docs/decisions/ADR-0002-bounded-tape-reader.ru.md).

## Выполненная матрица

Все **13 CTest-наборов** прошли в каждом профиле: десять прежних плюс protocol_binary_tape,
tape_inspection_io и tape_inspection_cli. Семь reader-кейсов / 18 061 утверждение прошли в дополнительном
прямом MSVC Debug-запуске; два IO-кейса и процессные утверждения CMake учитываются отдельно.

| Профиль | Доказательства сборки / тестов |
| --- | --- |
| MSVC 19.42.34444 Debug | сборка, тесты |
| MSVC Release | сборка, тесты |
| ClangCL 19.1.5 Debug | сборка, тесты |
| ClangCL Release | сборка, тесты |
| WSL GCC 13.3.0 Debug | сборка, тесты |
| WSL GCC Release | сборка, тесты |
| GCC Debug ASan+UBSan | сборка, тесты |

Логи конфигурации рядом; каждый UTC-лог начинается точной командой, cwd и TZ и заканчивается кодом
завершения. Windows использует Visual Studio 17 2022, x64, SDK 10.0.26100.0; GCC — Ubuntu-24.04 WSL x86_64.
Флаги санитайзеров: `-fsanitize=address,undefined -fno-omit-frame-pointer`. В выполненных тестах сообщений
санитайзеров об ошибках не было. Другие платформы, архитектуры и производительность не заявляются.

BUILD_TESTING=OFF прошёл во всех четырёх комбинациях QTP/TAPE: OFF/OFF,
OFF/ON, ON/OFF,
ON/ON. Проверены логи конфигурации и caches: нет каталога _deps и записей
поиска Python3. Нет новой сторонней runtime-зависимости, нового Python-скрипта или зависимости.
Неизменённые run_utc.py и прежние процессные Python-наборы используются согласно поэтапной миграции.
Новые тесты — C++/doctest, процессные проверки CMake и Java/PowerShell-валидация доказательств.

## Результаты и воспроизводимость

Исходная constrained.qtp доставляет ровно семь записей с локальными ID, символами и значениями Java.
Исходные default.qtp и text.txt явно отклоняются с кодом 4; исходная truncated.qtp завершается кодом 5
с завершённым префиксом. Байты tape не удаляются/не переставляются. Прежние TSV/human/профиль/коды
инспектора проходят старые наборы во всей матрице. Неизменённый базовый Java tape Verify также
воспроизводит все 22 исторических артефакта при каждом новом чистом oracle-запуске.

[Команды сборки/API](../../../../docs/CPP-BUILD.ru.md), [CLI](../../../../docs/TAPE-INSPECT.ru.md).
Примеры команд (точные пути и полные параметры компиляторов находятся в логах):

```powershell
cmake -S . -B .work/tape-msvc -G "Visual Studio 17 2022" -A x64 -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON
cmake --build .work/tape-msvc --config Debug
ctest --test-dir .work/tape-msvc -C Debug --output-on-failure
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Format
pwsh -NoProfile -File tools/format/Format-Cpp.ps1 -Mode Check
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

Закреплённые Uncrustify 0.83.0, затем clang-format 20.1.8 прошли Format/Check для 37 файлов. Начальная
компактная запись потребовала второго полного прохода для сходимости; итоговый Check прошёл. Java Format
предшествовал захватам; финальный Check прошёл для 12 файлов с google-java-format 1.36.1/JDK21; oracle
использовал JDK8. Первым Windows-сборкам требовался доступ к SDK вне sandbox; два старых Python-набора
сначала выбрали недоступный Python 3.14. В итоговой конфигурации явно указан установленный Python 3.11,
все тесты прошли. Эти повторные попытки из-за окружения не считаются успешными тестовыми запусками.

Исторический аудит сравнивает 1167 исторических fixture/lock-файлов, кроме Markdown,
с Git-архивом HEAD: 1083 побайтово идентичны, у 84 только существующее представление LF/CRLF в checkout,
иных различий нет. Ничего не нормализовано и не переписано. Исторические tracked fixtures и upstream.lock
не имеют Git diff; исходная tape также проходит raw oracle comparator. Созданный тестами tracked Python
cache восстановлен из точного HEAD blob; generated cache не входит в изменение.
Итоговый аудит содержит хеши исходников/инструментов, проверки пробелов/ссылок и staged/unstaged.

В этих проверках ошибок не обнаружено. Публичный API предварительный. Не заявлены инъекция ошибок
аллокатора, огромные ресурсы, производительность, authenticated-footer, совместимость default TapeConnector/
RAW_DATA, Quote, сжатия, временных sidecar, импортера, сети и общих sessions. Следующая веха реального
события — неизменённый default Quote с точными типами 56/2/120. Это исходная передача на ревью.
