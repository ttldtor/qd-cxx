# Зависимость и владение при загрузке IPF

[English version](TRANSFER.md). Английская версия является основной.

## Решение и текущие границы

Для слоя загрузки выбраны **заголовки HTTP/1 parser Beast из Boost 1.92.0 и скомпилированная
статическая библиотека Boost.URL**. Boost является приватной зависимостью отдельного optional слоя
загрузки. Он не должен становиться зависимостью, шагом поиска или загрузки независимого target
IPF values/local reader/writer либо `qdcxx_feed`. Optional реализация — [InstrumentProfileReader](URL-READER.ru.md) в
`qdcxx_ipf_transfer`. Она использует явно выбранный квалифицированный SDK; локальные API
сохраняются в [READER](READER.ru.md) и [WRITER](WRITER.ru.md). Pure IPF/feed без этой опции
не выполняет поиск или загрузку Boost.

Решение охватывает требования к HTTP/HTTPS/file sources. Сжатие и live collector реализуются
отдельно; их отсутствие не является принятым исключением из полного клиента. Решение не добавляет
реализацию proxy, обнаружение proxy из окружения или неявное хранение и повторную отправку cookies.

## Требования эталона

Источник фактов — закреплённый QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`:

| Источник | Существенное наблюдаемое требование |
| --- | --- |
| [InstrumentProfileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileReader.java) | Snapshot GET задаёт `X-Live: no`; явный token приоритетнее URL userinfo; проверка HTTP требует 200; доступен Last-Modified |
| [InstrumentProfileConnection](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java) | `X-Live: yes/provided`, условные If-Modified-Since/304, обновление timestamp после успешной обработки, отдельное live completion |
| [URLInputStream](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/URLInputStream.java) | Локальные пути/file URLs и HTTP connections; приоритет явного token; Java defaults connect/read 30000/60000 ms получаются из свойств процесса |
| [AuthToken](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/auth/AuthToken.java) | Basic использует UTF-8, затем Base64; также представимы Bearer/custom tokens |

Сокращение host:port в эталоне разрешается в `http://host:port/ipf/all.ipf.gz`; разрешение адреса
отдельно от поддержки сжатия. Прерывание Java thread в connection.close не доказывает быструю
отмену блокирующей I/O или завершение всех callbacks/native operations. Поведение redirects,
делегированное JDK, нельзя считать эквивалентным новому клиенту без production comparisons.

Native transfer должен иметь явные конечные options, независимые status/metadata и владеющий
префикс profiles. Завершение HTTP message, освобождение native operations, parser EOF и marker
завершения IPF — разные события.304 означает отсутствие изменений, а не новый успешно прочитанный
пустой snapshot. Ошибка после IPF marker не превращает незавершённую/неуспешную загрузку в успех.
Conditional metadata не должны тихо фиксировать неуспешное частичное содержимое. URL с .gz сам по
себе не означает успешную поддержку сжатия.

## Обоснование зависимости

Beast предоставляет низкоуровневый HTTP/1 parser, а не готовый клиент загрузки.
[Исходник parser точного release](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/basic_parser.hpp)
включает типы буферов Boost.Asio: это не готовая замена socket API standalone Asio. Выбраны только pure parser
и типы представления буферов. Владение native operations сохраняют standalone Asio 1.38.2 и
квалифицированный c-ares 1.34.9+qdcxx2. Boost socket/TLS streams, алгоритмы HTTP read, coroutines и
thread pools не выбраны.

Альтернативой был multi interface libcurl. Он предоставляет значительную часть загрузки, но
добавляет compiled C SDK и граф владения network/DNS/cache/global/callback, который нужно
квалифицировать. Его [global initialization](https://curl.se/libcurl/c/curl_global_init.html) и
[SSL context callback](https://curl.se/libcurl/c/CURLOPT_SSL_CTX_FUNCTION.html) не доказывают
повторное использование квалифицированного TLS engine проекта или изоляцию общего OpenSSL image.
[Лицензия curl](https://curl.se/docs/copyright.html) разрешительная, основана на MIT; выбор против
libcurl не сделан лишь из-за лицензии. Выбранная граница уменьшает объём новой инфраструктуры
network owner, сохраняя явную работу по adapter/provisioning. Это инженерное суждение, не benchmark.

## Точные исходники, лицензия и provisioning

| Выбранный вход | Идентичность |
| --- | --- |
| [Стабильный release](https://www.boost.org/releases/1.92.0/) | Boost 1.92.0 |
| [Официальные metadata архива](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz.json) | `boost_1_92_0.tar.gz`; SHA-256 `c4a3b310ddd2472416e091067166b0713be97c63f38c212c484ada022fd296ce` |
| Boost release commit | `afdfa32505af73e3d208144b3f623f0096cb62b6` |
| [Beast release source](https://github.com/boostorg/beast/tree/7c1e061f91e2ef542217b76286c314d006c0c8fc) | `7c1e061f91e2ef542217b76286c314d006c0c8fc` |
| [URL release source](https://github.com/boostorg/url/tree/a34f3a4a1a0e695f679453af71dfc050a59170e6) | `a34f3a4a1a0e695f679453af71dfc050a59170e6` |
| [Лицензия exact release](https://raw.githubusercontent.com/boostorg/boost/boost-1.92.0/LICENSE_1_0.txt) | Boost Software License 1.0; обязательные notices/license сохраняются при распространении входов/артефактов |

Для Boost.URL 1.92.0 нужна compiled library: release
[`src.hpp`](https://raw.githubusercontent.com/boostorg/url/boost-1.92.0/include/boost/url/src.hpp)
явно прекращает прежнюю one-TU integration. Нельзя применять старые инструкции header-only.
Для static archive, полного transitive source/header closure, лицензии, compiler, CRT flags и
build options нужны явные source/artifact hashes и квалификация до использования. При подготовке
этого решения архив/SDK не получали, SDK не собирали и native qualification не выполняли.

Provisioning должен принимать явное проверенное расположение source/artifacts, понятно отклонять
неверную/отсутствующую идентичность и не требовать globally installed Boost, package manager или
скрытую загрузку. Digest source archive идентифицирует вход, а не квалифицированный binary и не
доказывает конечность allocations. Выбранные ON profiles и installed/relocated consumers должны
соответствовать actual MSVC 19.42 x64 Release/MD и GCC 13.3 x86-64 Debug/sanitizer artifacts. OFF не
должен искать/link/fetch Boost или network SDK через local IPF/feed targets. Глобальные настройки
не меняются.

## Интеграция HTTPS

HTTPS сохраняет действующий [controlled TLS profile](../transport/TLS-RUNTIME.ru.md) и
[resource/host requirements](../transport/TLS-RESOURCES.ru.md): exact OpenSSL 3.5.9 image/
configuration, явный trust и проверенную reference identity. Не выбраны system roots/Schannel,
дополнительный TLS profile, обход проверки или plaintext fallback.

Public TlsRuntime сейчас владеет QTP session, а не универсальным HTTP stream. Его private
byte-engine seam даёт возможность повторного использования, но не является готовым public loader.
Реализация должна квалифицировать bounded protocol-neutral native owner/seam, сохраняя поведение
прежнего QTP owner и точный путь TLS policy/WANT/closure. Новый transfer участвует в shared/thread/
history/failure reservations того же controlled image. Private contexts не дают бесплатной
изоляции; нельзя тихо дублировать или возвращать shared host preflight/storage.

## Владение источником и credentials

Исходный источник допускает только предусмотренные формы local path, file URI, HTTP или HTTPS.
Неизвестные URI schemes отклоняются до acquisition/I/O, без fallback в локальный путь. Точное
написание, отображение local path/file URI, Windows drive forms, source-shortcut behavior и
канонизацию реализация должна квалифицировать сравнением с закреплённым эталоном; это решение не
задаёт новый URL API или правила угадывания Windows paths. Локальный reader сохраняет владение
собственным binary file. После relative resolution redirect target допускает только HTTP(S);
file и все остальные схемы отклоняются до acquisition/I/O. Network redirect не даёт права
открывать локальный файл.

Выбрана отдельная ограниченная владеющая Authorization provider для Basic/Bearer/custom HTTP
values. Она не меняет прежний QTP Basic provider и не обещает универсальную эквивалентность каждому
Java AuthToken. До публикации проверяются размер, scheme/header syntax и CR/LF. Код приложения
для получения credentials вызывается лишь после проверки нужного TLS origin. Anonymous HTTP
допустим; plaintext HTTP с секретами отклоняется.

Исходный HTTP(S) URL userinfo поддерживается как owned Basic credential source. Конечный вход
вызывающей стороны разбирается по borrow; только userinfo копируется в отдельное owned secret
storage, после чего transport/address representation строится из компонентов без userinfo.
Query остаётся непрозрачными данными транспорта; удаление userinfo не делает URI безопасным для логирования.
Нельзя сохранять owning copy полного credential URL или view в caller URL после вызова.
Percent/colon/UTF-8 semantics до заявлений реализации должны следовать production observations
закреплённого эталона; нельзя тихо декодировать percent sequences.

Явный Authorization provider/token приоритетнее исходного userinfo. Отказ provider не включает
fallback на userinfo. Userinfo в redirect Location отклоняется. Cross-origin redirect не наследует
исходный secret/provider автоматически; переход HTTPS→HTTP запрещён. Origin — это tuple из scheme,
canonical host и effective port, независимо от DNS address или resolved route. Точное написание и
канонизация требуют квалификации реализации. Credential может авторизовать лишь свой ещё живой attempt. Полные
URI/query, userinfo, token и текст provider exceptions не попадают в diagnostics/logs. Управление
памятью caller и secure erasure обычного C++ storage не обещаны.

Операция удерживает собственное стабильное состояние и provider lease во время user code, не
вызывает его под owner locks, затем до использования результата снова проверяет public lifetime,
attempt, cutoff и deadline. Provider может запросить close или уничтожить/заменить public owner,
не разрешая stale attempt; callback/work-buffer borrows не выходят за объявленное время жизни.

## Границы, повторная работа parser и отмена

Отдельные конечные limits должны учитывать URI/Location/secret bytes, status/header/trailer bytes
и counts, wire/framing/chunk-extension bytes, payload bytes, parser/profile output, попытки
redirect/informational responses, перекрытие storage и cumulative/turn work вместе с прежними
DNS/native/TLS scopes. Caps проверяются до соответствующих allocation/copy/growth; overflow/refusal
не должны опираться на предположение о нулевом allocator overhead или жёстком RSS/latency bound.

Выбранный [basic_parser source](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/basic_parser.hpp)
принимает single-contiguous-buffer input и имеет header/body limits.
[Buffer-sequence path](https://raw.githubusercontent.com/boostorg/beast/boost-1.92.0/include/boost/beast/http/impl/basic_parser.hpp)
может выделять flattening storage. Нужны собственный bounded contiguous input и ограниченное
storage производных fields; это не доказательство нулевого heap для всего owner. Неиспользованные
structured prefixes сохраняются только в пределах capacity. Одни header/body limits не ограничивают
retries, chunk metadata, informational messages и все source bytes.

Каждый вызов parser и переданный/просмотренный slice консервативно учитываются, даже при нуле
принятых bytes или ошибке. Need-more без нового input/EOF не должен busy-loop по тому же prefix
или возвращать работу. EOF передаётся один раз в допустимой точке. Fixed URL storage и relative
resolution ограничиваются до роста; borrowed parsed components/redirect references не переживают
своё стабильное input storage или его изменение.

Request-close — невыделяющий идемпотентный admission cutoff. Отмена/terminal publication и
освобождение native/callback operations — разные события; descriptor/handler/TLS/input/output/
credential owners живут до соответствующего completion/drain. Cooperative progress оплачивает
явную cleanup work; request-close не выполняет inline join и не обещает немедленную quiescence
или жёсткий I/O deadline. Финальное уничтожение owner может выполнить выбранный fallback drain и
ждать host progress; наблюдаемая quiescence требует явного progress/drain до освобождения.
Поздние completions не возобновляют parsing, не вызывают stale provider и не публикуют успешный
полный snapshot.

## Обязательная квалификация до доступности реализации

Optional реализация должна сохранить принятые local APIs и квалифицировать:

- Exact dependency/source/license/CRT artifacts, выбранные ON и dependency-free OFF graphs;
  отдельные self-contained installed/physically relocated consumers на обеих обязательных платформах.
- Bounded protocol-neutral native/TLS seam и все затронутые QTP regressions; прежние controlled
  host/shared reservations остаются истинными до final retirement.
- Реальный controlled HTTP 200/HTTPS 200/file success, relative/bounded redirects, conditional 304,
  Last-Modified и различение framing/IPF completion; сжатие остаётся отдельным этапом pipeline.
- Приоритет explicit token над userinfo, percent/colon/UTF-8 production facts, Basic/Bearer/custom
  syntax, redaction, запрет redirect userinfo/cross-origin inheritance/downgrade и failed-provider
  no-fallback.
- Partial headers/body/EOF, conflicting framing, header/trailer/chunk/work/output limits,
  failed allocation, отмену на каждой фазе и реальные callback/native lifetime boundaries.
- MSVC 19.42 x64 Release/MD и GCC 13.3 Debug ASan/UBSan на final source, независимое ревью и actual
  generated documentation/link/source-hash binding.

Решение о dependency/ownership и optional реализация не означают release или приёмку полного
клиента. Proxy, сжатие и live collection остаются отдельными реализациями; неподдержанные случаи
отклоняются явно. Доступность требует перечисленных final-source gates.
