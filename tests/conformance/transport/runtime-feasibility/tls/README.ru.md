# Проверка осуществимости runtime

[English version](README.md). Английский текст является основным.

Изолированная техническая проверка измеряет предлагаемый профиль standalone Asio/OpenSSL
с выполнением на потоке вызывающей стороны. Это не производственный транспортный адаптер;
проверка не доказывает отсутствие получения зависимостей при отключении будущего адаптера.

Используются только временные IPv4 loopback-сокеты и синтетические сертификаты. DNS-запросов
и обращений к внешним endpoint нет. Отмена DNS проверяется отдельно. Ключи тестовые,
намеренно незашифрованные; сохраняйте их локально. Сертификаты действуют два дня; перед
повторным запуском создавайте свежий набор.

## Зафиксированные зависимости и сборка SDK

`dependencies.lock.json` содержит URL, SHA-256, commit тегов и лицензии.
[Официальная страница Asio](https://think-async.com/Asio/Repository.html) указывает GitHub-репозиторий.
[Официальные загрузки OpenSSL](https://www.openssl-library.org/source/) указывают архив и checksum.
Хеш архива OpenSSL сопоставлен с опубликованным checksum; архив Asio закреплён измеренным
хешем и commit тега. Проверка PGP-подписей не выполнялась. При распространении сохраняйте
лицензии исходников: BSL-1.0 для Asio и Apache-2.0 для OpenSSL.

Загрузите два точных URL из lock-файла в собственный каталог и проверьте SHA-256 до распаковки.
Например, используйте `curl --fail --location --output <archive> <locked-url>` и PowerShell
`Get-FileHash -Algorithm SHA256`. Конфигурация OFF по умолчанию ничего не загружает.

Собирайте OpenSSL вне дерева исходников с `no-shared no-tests no-docs no-autoload-config`,
явными собственными `--prefix` и `--openssldir`. Windows x64: `VC-WIN64A`, MSVC 14.42,
Perl, NASM, затем `nmake` и `nmake install_sw` из developer environment Visual Studio.
Ubuntu x86_64: `linux-x86_64`, GCC 13.3, Perl, затем `make -j4` и `make install_sw`.
Ассемблер включён на обеих измеренных платформах. Статический Windows SDK использует
`/MT /Zl`. Зафиксированная upstream-конфигурация намеренно отключает default CRT library
directives через `/Zl`, позволяя выбрать CRT потребителю. Против одного неизменного SDK
проверены `/MD` (`MultiThreadedDLL`, default репозитория) и явно выбранный `/MT`
(`MultiThreaded`), без подавления предупреждений linker и переопределения default libraries.
Обычный маршрут сохраняет `/MD` репозитория; менять CRT всего проекта не требуется.
Для `/MD`-приложения требуется доставка соответствующего Microsoft C++ runtime.
Произвольные версии CRT и allocation boundaries между чужими DLL этим не доказаны;
объекты OpenSSL по-прежнему создаются/освобождаются парными OpenSSL API. Это не FIPS-профиль.
Собственные тесты OpenSSL намеренно не входят в данную проверку. См. зафиксированную
[VC-конфигурацию upstream](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/Configurations/10-main.conf#L1579).

`no-autoload-config` экспортирует `OPENSSL_NO_AUTOLOAD_CONFIG` и отключает автоматическое
чтение системного `openssl.cnf`, не меняя глобальные флаги инициализации во время работы.
Обычный системный OpenSSL без этой настройки вне измеренного профиля. Компиляция проверки
отклоняет другие версии OpenSSL/Asio и отсутствие указанного макроса.

## Сборка и запуск

В примере PowerShell переменные содержат выбранные вызывающей стороной абсолютные пути:
`Source` — этот каталог, `AsioInclude` — `include` распакованного standalone Asio,
`OpenSslSdk` — собственный установленный SDK OpenSSL. CMake и Ninja уже доступны.

```powershell
cmake -S "$Source" -B "$Build" -G Ninja `
  -DORG_TTLDTOR_QDCXX_TLS_PROBE=ON `
  "-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=$AsioInclude" `
  "-DOPENSSL_ROOT_DIR=$OpenSslSdk" `
  "-DORG_TTLDTOR_QDCXX_TLS_FIXTURES=$Fixtures" `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build "$Build"
cmake "-DOPENSSL=$OpenSslSdk/bin/openssl.exe" "-DFIXTURES=$Fixtures" `
  -P "$Source/GenerateFixtures.cmake"
ctest --test-dir "$Build" --output-on-failure
```

На Linux используйте те же параметры с Linux-путями, без настройки MSVC runtime, и
`<sdk>/bin/openssl`. `ORG_TTLDTOR_QDCXX_SANITIZE=ON` включает ASan/UBSan для кода проверки
при GCC/Clang. Отдельно собранный SDK OpenSSL при этом не инструментируется.

Конфигурация OFF проверяется без SDK:

```powershell
cmake -S "$Source" -B "$OffBuild" -G Ninja `
  -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON `
  -DCMAKE_DISABLE_FIND_PACKAGE_Threads=ON `
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON `
  -DOPENSSL_ROOT_DIR=missing-sdk -DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=missing-sdk
cmake --build "$OffBuild"
```

Запуск без аргументов и `--help` печатает одинаковую справку и не запускает набор проверок.
Для запуска нужен `--run --fixtures DIRECTORY`. Коды: 0 — справка/успех, 1 — ошибка работы,
доказательства или вывода, 2 — неверные аргументы. Вывод содержит фактические версии
библиотеки/provider, backend, числа обработчиков и категории ошибок.

## Что проверяют сценарии

Каждый `io_context` и пользовательский обработчик выполняется одним вызывающим потоком.
Windows IOCP создаёт внутренний поток ожидания при создании первой очереди таймеров Asio.
Он ждёт kernel waitable timer и отправляет wake в IOCP; пользовательский код не вызывает,
внешнюю конфигурацию не читает. При shutdown host выставляет флаг остановки, взводит таймер
для пробуждения, исчерпывает собственные операции и join-ит помощника. Поэтому host живёт
до quiescence и уничтожается вне callback. Socket-provider fallback без ConnectEx может
создать дополнительный select-reactor helper; этот вариант вне измеренного профиля современного
Windows TCP. Помощники не образуют второй домен владения.
Буферы и SSL-объекты живут до возврата всех обработчиков. Отмена закрывает
только собственные сокеты; выполнение продолжается до исчерпания работы в `run()`.
`io_context::stop()` не используется как доказательство quiescence. Обработчик может
запросить остановку закрытием своих сокетов, не уничтожая и не join-ая execution host.

Для отмены чтения подключённый peer бездействует. Для записи peer никогда не читает;
буферы сокетов уменьшены, payload ограничен 32 MiB. Таймер подтверждает, что запись
не завершилась до отмены. Backlog-сценарий открывает не более 512 локальных соединений
и сообщает число незавершённых до отмены. Отдельная немедленная гонка connect/cancel
явно печатает `pending-proof=0`. Если платформа принимает все соединения, backlog-сценарий
не доказывает отмену pending connect; одного факта запроса отмены недостаточно.
Набор завершается ошибкой, если ограниченный backlog-сценарий не получил ни одного pending отменённого connect.

TLS-клиент использует один явный PEM CA-файл, обязательную проверку peer, минимум TLS 1.2
и максимум TLS 1.3. DNS identity задаётся `SSL_set1_host`, SNI, `NO_PARTIAL_WILDCARDS` и
`NEVER_CHECK_SUBJECT`; IP identity — `X509_VERIFY_PARAM_set1_ip_asc` без DNS SNI. У сертификата
намеренно неправильный CN и правильные DNS/IP SAN. Проверяются доверенные DNS/IP, TLS 1.2,
недоверенный издатель, неправильное имя, истечение срока, raw TCP peer с зависшим TLS handshake
и peer с зависшим TLS shutdown. Зависшее закрытие прерывается через 100 ms; успешным graceful
close оно не считается. Нет plaintext fallback, неявного OS trust, загрузки default trust paths
или обхода проверки сертификатов.

Контекст ограничивает сообщения certificate list величиной 64 KiB, а глубину проверки — четырьмя
промежуточными CA; record compression, certificate compression в обоих направлениях и
renegotiation выключены. Явные input/output record-буферы Asio имеют размер 17 KiB каждый.
Парные кольца OpenSSL BIO и собственные record/certificate state OpenSSL учитываются дополнительно.
Это не доказывает общий per-endpoint бюджет памяти/CPU SDK. OpenSSL
выделяет certificate/algorithm/session state вне буферов приложения; per-context allocation
quota и возобновляемого ограничения CPU-работы одного handshake-вызова нет. Замена глобального
allocator hook не входит в профиль. Будущий адаптер должен отдельно учитывать/ограничивать
число контекстов и SDK allowances, ограничивать вход и pending-буферы приложения, документировать
измеренную консервативную SDK allowance. Одни байты payload не выполняют весь ресурсный контракт.

`OSSL_LIB_CTX` и явно загруженный встроенный default provider живут дольше всех SSL context,
stream и pending operations. `SSL_CTX_new_ex` связывает контекст с данным libctx; native-handle
конструктор Asio принимает владение `SSL_CTX` и освобождает его. После исчерпания обработчиков
уничтожаются stream/context, затем provider/libctx. Проверка не вызывает `OPENSSL_cleanup`,
не меняет глобальный default libctx, не загружает глобальную конфигурацию и не меняет глобальные
trust/provider properties. Обычная внутренняя автоматическая инициализация OpenSSL происходит
при первом настоящем TLS-вызове.

Измеренный профиль предполагает, что приложение не загружало вручную глобальную OpenSSL
SSL-конфигурацию `system_default`. `SSL_CTX_new_ex` обращается к этому реестру даже с явным
libctx. `no-autoload-config` отключает автоматическую загрузку файла, но не явный глобальный
вызов host. Такая смешанная конфигурация вне проверки и требует отдельной интеграционной
политики перед производственным TLS-адаптером.

Преднамеренная отмена запрашивается через 100 ms; через три секунды таймер усиливает отмену
при сбое сценария. Внешний предел CTest — 30 секунд на процесс набора. Таймер не может
прервать произвольный блокирующий C++ callback; собственные обработчики здесь возвращаются
быстро. Профиль проверяется только на Windows x64/MSVC IOCP и Ubuntu x86_64/GCC epoll.
Другие архитектуры, macOS, io_uring, системные OpenSSL, OS-native trust, фоновый execution host,
FIPS, проверки отзыва/OCSP/CRL, зашифрованные ключи, прокси, реальные credentials и production-интеграция не доказаны
либо находятся вне профиля.

Первичные контракты: [Asio native SSL context](https://think-async.com/Asio/asio-1.38.2/doc/asio/reference/ssl__context/context.html),
[Asio stop](https://think-async.com/Asio/asio-1.38.2/doc/asio/reference/io_context/stop.html),
[OpenSSL SSL_CTX_new_ex](https://docs.openssl.org/3.5/man3/SSL_CTX_new/),
[library contexts](https://docs.openssl.org/3.5/man3/OSSL_LIB_CTX/),
[проверка имени](https://docs.openssl.org/3.5/man3/SSL_set1_host/)
и upstream `INSTALL.md` для `no-autoload-config`.
