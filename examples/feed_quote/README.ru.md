# Пример получения типизированного Quote

[English version](README.md) · [Руководство public feed API](../../docs/FEED-API.ru.md)

Конечная консольная программа получает полные события `Quote` через публичный
endpoint/feed/subscription API. Библиотека владеет сетевым продвижением и рабочим
потоком; главный поток следит за временем/прерыванием и ожидает завершения.
[main.cpp](main.cpp) содержит полное приложение.

## Собрать с установленным пакетом

Нужны CMake 3.25+, поддержанный компилятор C++23 и пакет qd-cxx, собранный с
`ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` и явно включёнными CLIENT, RUNTIME и RECOVERY.
Следуйте [инструкции сборки пакета](../../docs/CPP-BUILD.ru.md#public-feed-api-build-ru),
включая подготовку закреплённых Asio/c-ares. TLS и JSON не требуются. Приложению
с рекомендованным TLS-OFF пакетом не нужны исходники SDK и SDK headers.
Если установленный пакет собран с TLS, его конфигурация также требует точную
закреплённую установку OpenSSL, даже при линковке только plain feed API: добавьте
к configure ниже `-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified`,
подставив реальный путь [подготовленного TLS SDK](../../docs/CPP-BUILD.ru.md#tls-source-profile-cpp-build-ru).
Исходники Asio/c-ares приложению не требуются.

### Presets для установленного пакета {#sample-build-profiles-ru}

[Профили сборки Feed](../../docs/CPP-BUILD.ru.md#portable-feed-profiles-ru) готовят
устанавливаемый plain-пакет без повторения внутренних переключателей библиотеки.
После `cmake --install build/qdcxx-feed-windows` из корня репозитория выполните
следующие команды в `examples/feed_quote`:

```text
cmake --list-presets
cmake --preset qdcxx-sample-windows
cmake --build --preset qdcxx-sample-windows
```

На Linux используйте `qdcxx-sample-linux`. Видим только sample preset текущей ОС;
[общие скрытые профили](../../cmake/BuildProfiles.json) задают те же Release/MD
или Debug ASan/UBSan настройки, что и у библиотеки. Нужны CMake 3.25+, Ninja и
терминал квалифицированного компилятора. Пример не конфигурирует/загружает
библиотеку или SDK. Он вызывает `find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)`.

Префиксы пакета по умолчанию — `build/install/qdcxx-feed-windows` и
`build/install/qdcxx-feed-linux` от корня checkout. Для другого установленного или
перемещённого пакета задайте `cmake --preset qdcxx-sample-windows -DCMAKE_PREFIX_PATH=/your/prefix`
(либо имя Linux-профиля). Compiler/CRT/sanitizer настройки должны соответствовать
пакету. Binary-каталоги — `build/qdcxx-sample-windows` или `build/qdcxx-sample-linux`
от корня checkout; build presets используют два задания с override `--parallel N`.
Программа называется `qdcxx-feed-demo.exe` на Windows и `qdcxx-feed-demo` на Linux.
`--help` выводит offline-справку. Для прямого Linux-запуска задайте
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`,
если сохраняете квалифицированное sanitizer-окружение: presets не меняют ваш shell.

Ручные команды configure/build ниже остаются поддержанными.
Из корня исходников qd-cxx, подставив реальный префикс установки:

```text
cmake -S examples/feed_quote -B build/feed-quote -G Ninja -DCMAKE_PREFIX_PATH=C:/sdk/qd-cxx -DCMAKE_BUILD_TYPE=Release
cmake --build build/feed-quote --config Release --parallel 2
```

[Самостоятельный CMake-проект](CMakeLists.txt) использует:

```cmake
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
add_executable(qdcxx_feed_demo main.cpp)
target_link_libraries(qdcxx_feed_demo PRIVATE qdcxx_feed)
```

Обязательный компонент `feed` даёт раннюю понятную ошибку поиска пакета, если
Feed не установлен. `qdcxx::feed` — равнозначный alias `qdcxx_feed`; прежнее имя
target и поиск пакета без компонентов остаются поддержанными. Для необязательного
поиска используйте `find_package(qdcxx CONFIG REQUIRED OPTIONAL_COMPONENTS feed)`
и проверяйте `qdcxx_feed_FOUND` перед линковкой. Единственный поддержанный
именованный компонент — `feed`.

C++23 и нативные статические зависимости наследуются от пакета. Используйте
совместимые компилятор, стандартную библиотеку и CRT установленного пакета.
На Linux замените префикс путём своей установки. Windows/Ninja создаёт
`build/feed-quote/qdcxx-feed-demo.exe`, Linux — `build/feed-quote/qdcxx-feed-demo`.
Visual Studio помещает программу в подкаталог `Release`.

Корневой проект собирает target `qdcxx_feed_demo`, когда включены feed API и
`ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES`. Примеры по умолчанию включены при основной
сборке qd-cxx и выключены при подключении qd-cxx через `add_subdirectory`.
Задайте `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=OFF`, чтобы собрать библиотеку без этого
исполняемого файла; feed API и его библиотечные тесты остаются включёнными.
Показанный выше отдельный проект собирает пример с установленным пакетом
независимо от значения этой опции при сборке самого пакета.

## Запустить

```powershell
./build/feed-quote/qdcxx-feed-demo.exe --help
./build/feed-quote/qdcxx-feed-demo.exe --help-ru
./build/feed-quote/qdcxx-feed-demo.exe --run --address demo.dxfeed.com:7300 --symbol AAPL --duration-ms 10000
```

На Linux используйте программу без `.exe`. Без аргументов также выводится английская
справка. Справка не создаёт runtime и не выполняет DNS/подключение. Для соединения
необходим `--run`. Demo-адрес — пример входных данных; эти команды не подтверждают
доступность внешнего сервиса и доставку событий. Данные источника могут быть задержанными.

| Параметр | Значение по умолчанию и ограничение |
| --- | --- |
| `--address HOST:PORT` | `demo.dxfeed.com:7300`; до 4096 байт; обычный TCP-адрес |
| `--symbol SYMBOL` | `AAPL`; точный printable ASCII до 128 units, без wildcard/All |
| `--duration-ms N` | `10000`; целое 1..600000 миллисекунд |
| `--help`, `--help-ru` | Английская/русская справка, только отдельный вызов |

Строки вывода содержат символ и `bid`, `bidSize`, `ask`, `askSize`, используя Java-подобные
business getters с binary64 точностью `max_digits10`. Так сохраняется прямое преобразование LONG;
отдельные WideDecimal поля доступны как raw decimal views. См.
[преобразования полей](../../spec/events/FIELD-CONVERSIONS.ru.md). `ApiRuntime` получает системные IPv4 DNS-серверы;
см. [правила DNS и жизненного цикла](../../docs/FEED-API.ru.md#feed-api-ru).
Программа использует TICKER по умолчанию и обычный TCP без credentials.

Код 0 требует хотя бы один реальный Quote, фактический local drain без истечения
deadline и неустранённого gap, а также отсутствие ошибки подписки. Сохранённая
диагностика endpoint всегда выводится статическими/числовыми полями с terminal flags,
в том числе когда код 0 подтверждает этот локальный префикс; прежние ошибки endpoint
остаются видимыми. Код 1 означает ошибку, сбой вывода или отсутствие данных;
код 2 — неверные аргументы; код 130 — прерывание. Ctrl+C запрашивает закрытие, затем
программа ожидает завершения своих ресурсов. Drain относится к принятому локальному
префиксу, а не к ACK peer или полноте ещё не прочитанных сетевых данных.

Запуск создаёт рабочий поток и сокеты, читает системные настройки DNS, пишет stdout
и диагностику. Входные файлы и глобальные настройки не изменяются. Блокирующий
вывод/обработчик может задержать завершение: duration и время drain не задают
жёсткий deadline завершения процесса.

Интервал получения ограничивает `awaitState` со сроком steady clock и stop token;
цикла опроса состояния/`sleep` нет. Сбой подписки запрашивает тот же token
и остаётся кодом 1. SDK не устанавливает обработчиков сигналов процесса.
Этот CLI владеет scoped bridge: POSIX `sigwait` для SIGINT/SIGTERM с полным
сохранением/восстановлением signal dispositions и маски потока-создателя либо
reader события Ctrl+C/Break Windows. `request_stop` вызывает только обычный reader;
SIGPIPE изменяется на POSIX только пока живы вывод и native-работа. SDK callbacks
завершаются до восстановления bridge на обычных выходах и при исключении.
Закрытие консоли/logoff/shutdown Windows сохраняют прежнее поведение ОС.
Реальные pending POSIX signals при гонке с восстановлением сохраняют прежнюю
семантику ОС. См. [полный контракт ожидания и обработки сигналов](../../spec/subscriptions/FEED-WAITS.ru.md).
CLI создаёт один application signal-reader thread помимо SDK worker; оба
завершаются до уничтожения runtime bootstrap.

Для owning Trade используйте тот же endpoint и createSubscription<events::Trade>() с getPrice()/getSizeAsDouble(); [короткий пример](../../docs/FEED-API.ru.md) использует SDK progress без клиентского polling. Built-in Trade composite; региональная запись требует явного registry, см. [Trade](../../spec/events/TRADE.ru.md). Этот console executable сохраняет Quote output; публичное live-подключение не подменяет deterministic installed-consumer checks.

Для отдельного extended-hours snapshot используйте createSubscription<events::TradeETH>() с теми же
business getters. Консоль остаётся примером Quote; [TradeETH](../../spec/events/TRADE-ETH.ru.md)
описывает отдельные обязательные flags и региональную регистрацию.

Для session snapshot используйте createSubscription<events::Summary>() с getDayOpenPrice(),
getPrevDayVolume(), getOpenInterest(); [Summary](../../spec/events/SUMMARY.ru.md) задаёт точную
business-int конверсию и региональное отсутствие OpenInterest. Этот executable сохраняет Quote output.
