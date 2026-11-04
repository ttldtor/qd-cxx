# Пример live IPF

[English](README.md) — нормативная версия.

Конечное приложение использует только публичные connection/runtime/collector API установленного
пакета. Оно сохраняет owning-префикс неизменяемых revisions, закрывает connection, выполняет join
runtime на creator thread и лишь затем печатает TSV. Transport polling в приложении нет.
Без аргументов, --help и --help-ru справка доступна до SDK, source I/O и создания потоков.

Пакет собирается с ORG_TTLDTOR_QDCXX_BUILD_IPF_LIVE=ON и явными transfer prerequisites.
ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON включает qdcxx-ipf-live. Исходники примера устанавливаются
в share/qdcxx/examples/ipf_live и отдельно собираются через find_package; component ipf_live
не требуется. Отсутствующий target отклоняется явно.

~~~powershell
cmake -S "$prefix/share/qdcxx/examples/ipf_live" -B "$sampleBuild" -G Ninja "-DCMAKE_PREFIX_PATH=$prefix" "-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=$opensslPrefix" "-DORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK=$transferSdk"
cmake --build "$sampleBuild" --parallel 1
& "$sampleBuild/qdcxx-ipf-live.exe" --help-ru
& "$sampleBuild/qdcxx-ipf-live.exe" --run --source "http://127.0.0.1:8080/profiles.ipf" --duration-ms 10000 --max-updates 1000
~~~

prefix — установленный пакет, sampleBuild — собственный build directory приложения, два SDK
prefixes — существующие совместимые qualified dependencies. На POSIX имя executable без .exe.
Адрес требует собственного сервера; доступность публичного сервиса не заявляется.

--source обязателен с --run, до4096байт в синтаксисе source reader.
--duration-ms:1..600000мс, default10000; абсолютный deadline начинается до start.
--max-updates:1..100000, default1000. До start резервируется столько owning-reference slots;
сохраняются только первые revisions, оставшийся вывод намеренно опускается. Конечные публичные
model/transfer limits продолжают действовать, включая retained value units; allocator metadata/RSS
не являются точной байтовой гарантией. Дополнительных trust/auth flags нет.

First-snapshot wait и application condition-variable wait используют один абсолютный deadline.
Заполнение capture отменяет первый wait, затем application close останавливает connection.
Нет скрытого polling loop, signal bridge или записи файлов. Runtime имеет один worker/connection;
bootstrap создаётся до worker, creator join выполняется до SDK cleanup. Вывод может блокироваться
после join, уже не удерживая активный SDK worker.

TSV содержит revision, type, symbol, removed. UTF-16 вне печатного ASCII и backslash кодируются
без потерь с помощью escape-последовательностей Unicode. Summary разделяет initial snapshot, HTTP/parser/container completion
и native retirement. Flushed prefix без COMPLETE остаётся неполным; stop не создаёт EOF
и успешный framing. Счётчики state/collector callback failures независимы.
Диагностика не выводит source/credentials.

Exit0: first snapshot завершён, stop/drain/output успешны. Exit3: штатная остановка приложения
до первого полного snapshot. Exit1: preparation/source/callback/drain/output error.
Exit2: неверные аргументы до source I/O. Создаются один worker и source handles, без изменения
файлов/settings или signal handlers. Retained revisions живут после collector/runtime.

Удаление listener advisory: уже выбранный callback может завершиться. Runtime join покрывает
свой worker и уведомления; внешние collector producers требуют отдельных joins участников.
У этого примера внешних collector producers нет.
