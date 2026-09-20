# Компонент compact integer на C++23

[English version](CPP-BUILD.md). Английский документ является основным.

QDCXX-010 добавляет только библиотеку `qd_encoding` без сторонних зависимостей с предварительным API
в [compact.hpp](../include/qd/encoding/compact.hpp). Совместимость определяют
[контракт примитивов](../spec/encoding/PRIMITIVES.ru.md) и неизменные Java QD 3.355 vectors.
UTFString, PentaCodec, importer, schema runtime, QTP framing, transport, packaging и заявления
о производительности остаются вне этого компонента.

## Сборка и тестирование на Windows

Проверено с CMake 4.4.2, генератором `Visual Studio 17 2022`, x64, Windows SDK 10.0.26100.0,
MSBuild 17.14.51, MSVC 19.42.34444.0 (каталог инструментов 14.42.34433). Второй компилятор —
установленный ClangCL 19.1.5 с той же стандартной библиотекой MSVC и SDK. Тесты используют
doctest 2.4.12 через CMake FetchContent; URL архива и SHA-256 закреплены в
[CMakeLists.txt](../CMakeLists.txt). Только `compact_tests` связывается с doctest; `BUILD_TESTING=OFF`
не загружает и не подключает его. Загрузки хранятся в `_deps/` игнорируемого каталога сборки;
первая конфигурация тестов требует сети. Менеджер пакетов не нужен. doctest 2.4.12 выдаёт
нефатальное предупреждение CMake об устаревшей минимальной версии policy; загруженный исходник не менялся. Требуется CMake 3.25+; цели наследуют `cxx_std_23` от библиотеки,
расширения отключены. Для этих toolsets CMake выбирает `/std:c++latest` для поддержки C++23.

Из корня репозитория в PowerShell:

```powershell
python tools/run_utc.py --log .work/msvc-configure.log -- cmake -S . -B .work/msvc -G "Visual Studio 17 2022" -A x64
python tools/run_utc.py --log .work/msvc-debug-build.log -- cmake --build .work/msvc --config Debug
python tools/run_utc.py --log .work/msvc-debug-test.log -- ctest --test-dir .work/msvc -C Debug --output-on-failure
python tools/run_utc.py --log .work/msvc-release-build.log -- cmake --build .work/msvc --config Release
python tools/run_utc.py --log .work/msvc-release-test.log -- ctest --test-dir .work/msvc -C Release --output-on-failure
python tools/run_utc.py --log .work/clang-configure.log -- cmake -S . -B .work/clang -G "Visual Studio 17 2022" -A x64 -T ClangCL
python tools/run_utc.py --log .work/clang-debug-build.log -- cmake --build .work/clang --config Debug
python tools/run_utc.py --log .work/clang-debug-test.log -- ctest --test-dir .work/clang -C Debug --output-on-failure
python tools/run_utc.py --log .work/clang-release-build.log -- cmake --build .work/clang --config Release
python tools/run_utc.py --log .work/clang-release-test.log -- ctest --test-dir .work/clang -C Release --output-on-failure
```

Сборка выполняется вне корня исходников; `.work/` и `build/` игнорируются. Конфигурация внутри
корня исходников отклоняется. `BUILD_TESTING=OFF` собирает только библиотеку. `QD_ENCODING_FIXTURES`
может указывать на временные копии fixtures для негативных проверок; по умолчанию используются
эталоны репозитория. Первая попытка обнаружения SDK через MSBuild в песочнице получила отказ
в доступе к локальному каталогу Microsoft SDKs; разрешённые configure/build выше успешно выполнены
с повышением разрешений песочницы. Это ограничение окружения, а не ошибка codec.

## API и владение

Все четыре функции находятся в `qd::encoding`, заимствуют принадлежащий вызывающему `std::span`
только на время вызова, не выделяют память и не сохраняют указатели. Результаты владеют значениями
и счётчиками. Вызывающий обеспечивает валидность span на протяжении вызова. Порядок байтов явно
big-endian; чтение не требует выравнивания.

- `readCompactInt` / `readCompactLong` возвращают `ReadResult<int32_t/int64_t>`: `value`, `consumed`,
  `status`. Успех потребляет ровно одно значение; хвост доступен через `input.subspan(consumed)`.
  Избыточные формы принимаются; int возвращает младшие 32 бита со знаком даже для long-кодировки.
- Пустой/неполный ввод возвращает `INSUFFICIENT_INPUT`, нулевое значение и всю длину ввода в
  `consumed`. Это соответствует наблюдаемой категории/позиции Java EOF, но не строкам класса/сообщения
  исключения. Отката нет: сохраните исходные байты и повторите чтение с начала после поступления
  данных. Частично потреблённая позиция не является состоянием для продолжения; транзакционный
  framing относится к будущему слою.
- `writeCompactInt` / `writeCompactLong` возвращают `WriteResult`: `written`, `status`. Они создают
  канонические Java bytes (не более 5/9). Недостаточная ёмкость, включая нулевую, возвращает
  `INSUFFICIENT_OUTPUT`, нулевой written и оставляет весь выход неизменным. При успехе изменяются
  только первые `written` байтов. Это политика ограниченного C++ writer, а не утверждение о сбоях
  вывода в Java stream.

Беззнаковое накопление/маскирование и восстановление битов фиксированной ширины сохраняют знак
без signed overflow, отрицательных сдвигов, сдвигов на ширину типа и зависимости от порядка байтов
хоста. Выбор размера writer использует положительные представимые степени двойки. Проверка строгой
каноничности reader не добавлена.

## Проверки и форматирование

[Покрытие](../tests/conformance/encoding/COVERAGE.ru.md) и
[валидация](../tests/conformance/encoding/VALIDATION.ru.md) фиксируют включённые/исключённые случаи,
ошибки и ограничения. [Инструкции форматирования](../tools/format/CPP.ru.md) определяют закреплённый
pipeline из двух инструментов; имена и содержательный Doxygen проверяются отдельно.

## Linux / WSL и журналы UTC

Также проверено в WSL2 Ubuntu 24.04.4 LTS, x86_64, kernel 6.18.40.1-microsoft-standard-WSL2,
GCC 13.3.0 (libstdc++), CMake 4.4.2 и Ninja 1.11.1. Debug и Release проходят те же два случая doctest
(28 358 assertions) и неизменные 832 выбранных Java observations. Конфигурация/сборка только библиотеки
с `BUILD_TESTING=OFF` успешна без создания `_deps/`. Негативные fixture/parser/CTest проверки Linux
успешны; `--skip-format` явно пропускает проверки Windows-форматтеров, уже выполненные на Windows.
Sanitizers, другие дистрибутивы Linux, macOS и другие архитектуры не проверялись.

Из Ubuntu WSL (`wsl -d Ubuntu-24.04`), в смонтированном репозитории:

```bash
cd /work/qd-cxx
for config in Debug Release; do
  build=.work/linux-${config,,}
  python3 tools/run_utc.py --log "$build-configure.log" -- cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE="$config"
  python3 tools/run_utc.py --log "$build-build.log" -- cmake --build "$build"
  python3 tools/run_utc.py --log "$build-test.log" -- ctest --test-dir "$build" --output-on-failure
done
python3 tools/run_utc.py --log .work/linux-negative.log -- python3 tests/conformance/check_compact_negative.py .work/linux-release/compact_tests --skip-format --report .work/linux-negative-proof.json
cmake -S . -B .work/linux-library -G Ninja -DBUILD_TESTING=OFF
cmake --build .work/linux-library
```

[run_utc.py](../tools/run_utc.py) требует Python 3.11+, ставит каждой сохраняемой строке вывода,
запуску и завершению команды время ISO 8601 UTC (`Z`) и задаёт `TZ=UTC` дочерним процессам.
Это также переводит создаваемый CTest LastTest.log на UTC. Текст вывода и код возврата дочерней
команды сохраняются; часовой пояс хоста не меняется. Текущие C++-журналы созданы новыми запусками
в UTC, а не переобозначены; исторические Java oracle captures сохраняют исходные байты/часовые пояса.
Новые JSON-отчёты негативных проверок содержат `executedAtUtc`. Публичные функции и тестовые утилиты
теперь описывают каждый параметр/параметр шаблона и результат, отличный от void.
