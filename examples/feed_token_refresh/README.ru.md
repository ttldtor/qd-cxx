# Обновление учётных данных QTP Basic

[English version](README.md) · [Аутентификация в Feed](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.ru.md#авторизация-qtp-basic)

[Пример](main.cpp) подключается к QTP-серверу по TLS с проверкой сертификата.
После подтверждения первых учётных данных он обновляет
`CredentialProvider`, вызывает `reconnect()` и ждёт подтверждения новых
учётных данных на другом подключении. Пример выводит номера двух эпох
источника. Оба значения Basic передаёт приложение. Получение и автоматическое
обновление токенов OAuth находятся за пределами этого API.

Для сборки нужны квалифицированный компилятор и [пакет qd-cxx с TLS](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.ru.md#подключение-с-проверкой-tls).
Укажите ту же квалифицированную сборку OpenSSL, которую использует пакет.

```sh
cmake -S examples/feed_token_refresh -B build/feed-token-refresh -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx -DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=/path/to/qualified-openssl
cmake --build build/feed-token-refresh
```

Укажите адрес QTP TLS, ожидаемое имя в сертификате и файл доверенных
сертификатов в формате PEM. Подайте на стандартный ввод две полные строки
`Basic <base64>` из источника учётных данных, сначала действующее значение,
затем новое. Пример для POSIX-оболочки с уже защищённым входным файлом.

```sh
build/feed-token-refresh/qdcxx-feed-token-refresh qtp.example.net:7443 qtp.example.net trust.pem < credentials.txt
```

Храните учётные данные вне аргументов командной строки и журналов. Пример
не гарантирует их безопасное стирание из памяти. Нужен сервер QTP, который
запрашивает Basic и принимает оба значения. После окончательного отказа
создайте новый endpoint. Пределы ресурсов TLS рассчитаны на этот
самостоятельный процесс. Для встраивания см. [профиль ресурсов TLS](https://github.com/ttldtor/qd-cxx/blob/master/spec/transport/TLS-RESOURCES.ru.md).
