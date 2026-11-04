# IPF reader для URL и источников

[English](URL-READER.md). Английский текст нормативный.

## Optional target и простой вызов

Необязательный qdcxx_ipf_transfer предоставляет InstrumentProfileReader из
qd/ipf/instrument_profile_url_reader.hpp. Он соединяет существующий [IPF parser](READER.ru.md),
ограниченный HTTP framing и private [byte owners](../transport/BYTE-OWNER.ru.md).
Локальные qdcxx_ipf функции и qdcxx_feed не зависят от Boost. BUILD_IPF_TRANSFER требует явных
IPF, CLIENT, RUNTIME и TLS и абсолютного ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK с квалифицированным
sealed SDK. Автоматической загрузки или поиска глобального SDK нет.

Standalone owner создаётся на main до потоков приложения:

    auto created = ipf::InstrumentProfileReader::create(options);
    if (!created)
        return report(created.error());
    auto reader = std::move(*created);
    std::stop_source cancellation;
    auto result = reader->loadURL(source, request, cancellation.get_token());

loadURL самостоятельно выполняет конечные native/HTTP/IPF turns и retirement. Приложению не нужен
work/poll loop. Другой поток может вызвать request_stop у stop source; он не обращается к facade
или SDK. Все методы facade и уничтожение остаются на creator-thread. Скрытых workers, пользовательских
callback из native handler, proxy discovery или cookie jar нет.

Перегрузка create(RuntimeBootstrap&, options) явно заимствует same-linked-instance bootstrap на его
creator-thread. Host создаётся на main до потоков и живёт до уничтожения facade и активного drain.
При совмещении feed runtime и reader приложение выбирает этот явный host profile при старте.
Скрытый owned bootstrap существующего ApiRuntime не экспортируется.

HTTPS требует явных trustPem, квалифицированных TLS limits/host maxima, SDK/resident allowances и
положительного reader-lifetime tlsWork. Для DNS names нужны явные числовые DNS servers/port.
System trust/default resolver и plaintext fallback отсутствуют. Обязательства embedding описаны в
[TLS resources](../transport/TLS-RESOURCES.ru.md).

## Источники и credentials

Принимаются локальные пути, квалифицированные file URI, HTTP и HTTPS. Сокращение host:port с портом
1..65535 разворачивается в http://host:port/ipf/all.ipf.gz с сохранением query и не включает
декомпрессию. File URI допускает пустой/localhost authority, без userinfo/port/query/fragment;
percent-decoded UTF-8 path ограничен и не содержит NUL. Windows drive/drive-relative формы применяют
Windows-only правило reference после классификации сокращения. Неизвестная URI scheme отвергается
до acquisition/I/O без fallback к локальному пути.

HTTP syntax проверяется Boost.URL, адрес хранится в фиксированных8192байтах. Начальный userinfo
становится отдельным owned Basic value с разделением по первому literal colon. Percent sequences
остаются literal credential bytes. Literal UTF-8 userinfo отделяется ограниченным borrow до строгой
проверки очищенного адреса; IRI-преобразований host/path нет. Явный owned Authorization или provider
имеет приоритет над userinfo. Ошибка выбранного provider не разрешает fallback. Basic кодирует точные
UTF-8 user/password через Base64; Basic/Bearer/custom проходят конечные syntax/size checks.

Plaintext HTTP с credentials отвергается. Provider вызывается на creator-thread только после
проверки намеченного HTTPS origin и вне native-owner locks. Destination и sanitized origin views
действуют лишь внутри callback. Captures, произвольная работа callback/destructor и внешний граф
provider остаются предусловиями приложения, вне library-owned memory/time bounds.

Callback может отменить операцию, удалить или заменить facade. Активный вызов держит отдельный stable
operation/host lease и после callback не обращается к facade. Перед публикацией токена повторно
проверяются liveness, attempt, cutoff, stop token и deadline. Старый результат не авторизует
заменённую операцию. Credentials/provider lease, buffers, parser, TLS/native owners и bootstrap
живут до соответствующего native retirement.

Redirect использует encoded RFC3986 merge/dot removal с повторной URL validation; %2e/%2f не
декодируются. Разрешены только HTTP(S) targets. Userinfo, file/прочие schemes и HTTPS downgrade
отвергаются до I/O. Origin — canonical scheme/host/effective port, независимо от DNS address.
Cross-origin redirect навсегда сбрасывает начальные token/provider и conditional metadata для этого
load; автоматического credential replay нет.

Диагностика не содержит полных URI/query, userinfo, токенов или provider exception text.
Удаление userinfo не делает query пригодным для логирования. Secure erasure обычной C++ storage и
памяти вызывающего не обещается.

## Независимые outcomes и completion

| Поле/результат | Значение |
| --- | --- |
| SUCCESS | Source/HTTP framing и parser EOF успешны по выбранной IPF policy; native retirement наблюдён |
| NOT_MODIFIED | Явный conditional request получил304; нового snapshot/complete/metadata commit нет |
| CANCELLED | Stop token, cancel или retirement facade закрыли допуск до success |
| FAILED | Первый typed source/native/TLS/HTTP/IPF/work/resource cause с owned whole-profile prefix |
| httpFinished | Завершён final HTTP framing; одного IPF marker недостаточно |
| ipf.finished / completionSeen / complete | Существующие независимые EOF/marker/policy признаки parser |
| nativeRetired | Все owned native handlers/borrows drained до возврата |
| lastModified / metadataCommitted | Parsed candidate timestamp отдельно от успешного200 content commit |

Snapshot GET посылает X-Live:no и Accept-Encoding:identity. X-Live:provided только наблюдается;
live collector не создаётся.304 без явного conditional request является ошибкой статуса.
Last-Modified принимает три HTTP-date формы; условная дата сериализуется как IMF-fixdate.
Для устаревшего двузначного года действует HTTP правило пятидесятилетнего окна.
Malformed date не становится committed timestamp.

Marker не прекращает IPF parsing. Отсутствующий COMPLETE следует ALLOW/REQUIRE. Усечённое
HTTP/TLS-завершение после marker остаётся transfer failure с owned prefix. Candidate metadata
не коммитит ошибочное частичное содержимое. Native EOF, HTTP framing, IPF EOF, marker admission и
TLS close_notify учитываются отдельно.

## Конечные лимиты и retirement

Все границы проверяются перед соответствующими growth/copy/publication. Значения независимы и
настраиваются; один лимит может отказать раньше другого:

| Область | Default |
| --- | --- |
| Source / Location / Authorization | 8192байта каждый |
| Header/fields; trailer/fields | 65536/128;16384/64 |
| Payload / plaintext HTTP wire / framing | 67108864/83886080/8388608байт |
| Chunk extension / cumulative extension | 4096/1048576байт |
| Redirect / informational / HTTP attempts | 5/8/6; connection retry отсутствует |
| HTTP calls / presented-copy work / turns | 1000000/268435456байт/1000000 |
| Contiguous input / whole request | 131072/32768байт |
| Transfer reservation / cleanup turns | 16777216байт/256зарезервированныхturns |
| Overall / connect / read / write | 300/30/60/60секунд |
| Handshake / TLS close / cadence | 30/1секунда/1миллисекунда |

Transfer reservation включает fixed host/operation/address structures, windows, выбранный secret
overlap, settings и ограниченную аллокацию shared host/control block. IPF logical parser/profile
output, native/TLS/process reservations и allocator bookkeeping учитываются отдельно. Hard RSS и
latency не обещаются.

Requested profile-vector growth ограничен прежним IPF profile bound; array/text relocation work
списывается перед growth. На платформе с throwing move preparation старый prefix копируется.
Old/candidate profile text ограничен удвоенным logical output-unit envelope; allocation failure
сохраняет прежний owned prefix. IpfReadLimits не увеличиваются.

Cumulative wire/framing/parser/copy/attempt counters не сбрасываются при redirect/informational.
Zero-progress attempts оплачиваются; need-more без новых bytes/EOF не запускает busy-loop.
Cleanup steps/handlers отделяются от исходного nativeWork, нового grant не создаётся.
SUCCESS ждёт наблюдённого native/TLS retirement. Исчерпанный cleanup reserve даёт WORK_LIMIT и
существующий safe fallback drain. Request-only cancel не означает мгновенную quiescence.

HTTPS child использует прежние F+R admission, H history, E/close grants и same-image thread/failure
maxima. Reader-lifetime TLS credits расходуются через loads/redirects. Redirect, cancel, parser reset
и child retirement не возмещают и не переинициализируют shared SDK history/host storage. Host maxima
должны оставаться истинными для cooperating/foreign users до final retirement.

Compression и live collection — отдельные pipeline implementations; отсутствие не исключает их
из complete-client цели. Compressed signatures/unsupported Content-Encoding дают явный отказ.

## Первичные источники

Сравнения опираются на pinned Java production sources в [TRANSFER](TRANSFER.ru.md), включая
различие snapshot200/connection304 и явные C credential/redirect границы.
Wire rules: [RFC9110](https://www.rfc-editor.org/rfc/rfc9110.html),
[RFC9112](https://www.rfc-editor.org/rfc/rfc9112.html),
[RFC3986 vectors](https://www.rfc-editor.org/rfc/rfc3986.html#section-5.4).

Native resident allowance C равен96MiB и включает прежний конечный envelope DNS profile.
Byte-work по умолчанию равен64GiB консервативных логических единиц preparation/copy, а не
выделенной памяти. DNS units — конечный счётчик UINT64_MAX: каждый допущенный вход существующего
DNS списывает измеренные worst-entry units. Это не sentinel безлимитности. Независимые DNS
calls/packets/bytes, HTTP attempts, internal turns и deadlines также ограничивают progress.
Whole-load grants сохраняются при redirects без возврата или обновления. HTTPS resident funding
включает объявленный native allowance вместе с собственными TLS grants/history; host не покрывает
разницу неявно.

Borrowed creation удерживает stable identity bootstrap, а не адрес wrapper. Приложение может
переместить bootstrap facade, в том числе из authorization callback, и уничтожить moved-from
wrapper. Moved-to owner сохраняется до reader/operation retirement. Следующие load/redirect
получают child из той же stable lease и прежнего конечного borrow domain.
Local acquisition принимает только обычные файлы. POSIX выполняет nonblocking open, затем fstat
открытого descriptor; Windows проверяет открытый disk-file handle и отклоняет directories.
Symlink на обычный файл поддерживается. FIFO, pipe и device отклоняются до read; проверки одного
pathname недостаточно. Native filesystem acquisition и чтение обычного файла зависят от progress
filesystem/OS и не имеют жёсткой гарантии scheduling или latency.

Literal source/userinfo и decoded file-URI paths требуют полных UTF-8 scalars: malformed, overlong,
surrogate и out-of-range sequences отклоняются до I/O. ASCII percent triplets userinfo остаются
буквальными credential bytes. Authorization factories проверяют конечный размер до сканирования.

Transfer resident allowance учитывает одновременно живущие fixed source-return/redirect builders,
borrowed parsing views, удерживаемую Location copy и native open-path workspace. Return/move
elision не предполагается. Это консервативный requested backing, отдельно от allocator bookkeeping/RSS.

После native first cause обычный TLS poll прекращается: он мог бы принять или отправить pending
request output. Только уже verified cached plaintext копируется и consumed без SDK call;
непроверенный ciphertext отклоняется. Ранее detached profiles сохраняются. Plain completed input использует отдельный cache-only raw drain без ordinary owner poll после cause.
Plain prefix после native error использует отдельный cache-only RawByteOwner drain, без ordinary
poll. Completed Windows bytes/core input поступают в прежнее raw input window с прежними
native-step и copy-operation/byte credits. Нулевые credits дают yield до мутации; context poll,
новые socket operations, codec step и output acceptance/submission отсутствуют. Pending handlers
и output удерживаются до retirement, первый cause сохраняется.
HTTPS construction учитывает две одновременно живущие transfer-owned trust copies: host settings
и временный caller EndpointTls snapshot. Его полный inline object и конечный hostname/string
backing учтены до allocation обеих копий. Третий внутренний settings snapshot byte owner
по-прежнему финансируется отдельным прежним TLS allowance; domains не используют неучтённый
свободный объём друг друга.