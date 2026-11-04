# Профиль исходников зависимости IPF transfer

[English version](README.md). Английский текст является нормативным.

Этот отдельный профиль собирает **Boost 1.92.0 URL как статическую библиотеку** и проверяет выбранный
Beast HTTP/1 `basic_parser` с одним непрерывным буфером вызывающей стороны. Он не включает production
transfer target, URL loader, socket, TLS stream, coroutine или thread pool. Root build и независимые
graph зависимостей IPF/feed не меняются. См. утверждённую [границу transfer](../../spec/ipf/TRANSFER.ru.md).

[Подготовка/сборка/seal](../../tools/deps/IPF-TRANSFER.ru.md) выполняются явно и локально для проекта.
Обычный configure эту зависимость не получает; отдельный профиль по умолчанию OFF и в этом режиме
даже не включает компилятор.

## Закреплённый вход и выбранное замыкание

[boost-1.92.0.sources.json](boost-1.92.0.sources.json) содержит официальный URL/SHA256 архива,
точные commits release/компонентов, лицензию BSL-1.0 и хеши всех **417** выбранных source/license файлов.
SHA256 архива: `c4a3b310ddd2472416e091067166b0713be97c63f38c212c484ada022fd296ce`.
Выборка объединяет реальные compiler dependency lists MSVC 19.42 x64 Release/MD и
GCC 13.3.0 x86-64 Debug ASan/UBSan, дополненные всеми файлами URL `src`, лицензией и version header.
Все source files компилируются отдельно; прекращённый `boost/url/src.hpp` не используется.
Это конечное замыкание для зафиксированных macros и probes, без заявления общей минимальности.
Если следующему адаптеру нужны другой header/macro, профиль надо явно расширить и переквалифицировать.
Provisioner извлекает только перечисленные пути архива, без Boost docs, examples и build scripts.

Static target экспортирует `BOOST_ALL_NO_LIB=1`, `BOOST_URL_NO_LIB=1`, `BOOST_URL_STATIC_LINK=1` и
`BOOST_BEAST_HEADER_ONLY=1`; `BOOST_URL_SOURCE=1` виден только при компиляции URL. Windows также
задаёт `_WIN32_WINNT=0x0A00`, `WINVER=0x0A00`, `NOMINMAX` и `WIN32_LEAN_AND_MEAN`.
Поддержанные profiles: single-configuration Windows MSVC 19.42 x64 Release/MD и
Linux GCC 13.3.0 x86-64 Debug с явными `-fsanitize=address,undefined -fno-omit-frame-pointer`.
Совместимость произвольных compiler/CRT/architecture из этого не следует.

## Владение и пределы квалификации

[Probe](../../tests/conformance/ipf-transfer-dependency/parser_probe.cpp) проверяет content-length,
непотреблённый suffix, status без body, завершение chunks/trailers, точный/превышенный body limit,
header limit, malformed status, конфликт content-length, truncated EOF и повторную передачу
непотреблённого partial header вызывающей стороной. URI probes проверяют borrowed input pointer,
сохранение encoded userinfo/query, port, relative reference, malformed host/percent, отсутствие scheme
и отказ для embedded NUL. Общий URI parser принимает другие schemes; transfer policy ещё обязана
отклонять их до I/O.

URL view заимствует живой вход вызывающей стороны. Probes не сохраняют view после окончания жизни
входа и не доказывают facade, владеющий входом. Parser field/body borrows используются внутри callbacks.
Выбранная перегрузка contiguous-buffer избегает convenience path со склейкой нескольких буферов.
Dynamic/multi-buffer wrappers могут выделять память. Здесь не заявлены универсальные zero-heap,
bounded-RSS, parser-work, HTTP acquisition, cancellation, credential, redirect или TLS-envelope
qualification. Эти production contracts остаются отдельной работой.

Установленный private SDK содержит выбранные sources/license, static archive, относительные CMake
package files и build metadata. Отдельный seal связывает хеши actual archive и metadata с закреплённым
source manifest. Это идентичность конкретной сборки: другие build paths/toolchain patch levels могут
менять байты архива. Воспроизводимая подготовка не обещает универсального переносимого binary hash.
Seal проверяет локальную согласованность в доверенных каталогах, а не подлинность недоверенной поставки,
независимое ревью или факт выполнения tests. До seal выполните описанные проверки.

[Отдельный consumer](../../tests/conformance/ipf-transfer-dependency/consumer/CMakeLists.txt)
использует только установленный package и exported private target. Relocation не требует прежнего
source/build directory. Public library interfaces не должны раскрывать Boost types; этот отдельный
SDK — внутренний пакет зависимости, а не новый публичный IPF API. Static linking сам по себе не
доказывает ODR или symbol isolation от другого Boost image в приложении.
