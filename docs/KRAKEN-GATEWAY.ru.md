# Запуск шлюза котировок Kraken

[English version](KRAKEN-GATEWAY.md).

Шлюз читает публичные лучшие цены покупки и продажи BTC/USD через Kraken Spot WebSocket v2. Он передаёт точные цены и объёмы в записях Quote по QTP TICKER на IPv4 loopback. Для публичного источника не требуются учётная запись или API-ключ. [Kraken ticker API](https://docs.kraken.com/exchange/api-reference/spot-websocket-v2/ticker)

## Сборка и установка

Используйте настройки платформы из [профиля Beast](../tools/deps/BEAST-WSS.ru.md) и [руководства по сборке Feed](CPP-BUILD.ru.md#public-feed-api-build-ru). Подготовьте закреплённые заголовки Boost и проверенный статический OpenSSL SDK. Укажите абсолютные пути. Включите среду выполнения Feed с standalone Asio и c-ares. Для этого процесса оставьте QTP TLS выключенным.

Добавьте к конфигурации Feed следующие параметры.

```text
-DORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON
-DORG_TTLDTOR_QDCXX_BUILD_KRAKEN_SOURCE=ON
-DORG_TTLDTOR_QDCXX_BUILD_KRAKEN_GATEWAY=ON
-DORG_TTLDTOR_QDCXX_BUILD_TLS=OFF
-DORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE=<закреплённые заголовки Boost>
-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=<проверенный OpenSSL SDK>
```

Соберите и установите настроенный проект.

```sh
cmake --build build/kraken
cmake --install build/kraken --prefix stage/kraken
```

Исполняемый файл устанавливается в каталог бинарников, обычно `stage/kraken/bin`. Независимые клиентские приложения подключают Feed через `find_package(qdcxx CONFIG REQUIRED)` и цель `qdcxx::feed`. См. [отдельный пример Quote](../examples/feed_quote/README.ru.md).

## Получение котировок

Соберите клиент Quote из дерева исходников с установленным пакетом. Используйте те же инструменты и профиль сборки, что и при сборке шлюза.

~~~sh
cmake -S examples/feed_quote -B build/kraken-client -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/absolute/path/to/stage/kraken
cmake --build build/kraken-client
~~~

В Linux используйте соответствующий профиль GCC 13 с санитайзерами.

~~~sh
cmake -S examples/feed_quote -B build/kraken-client -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-13 \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined -no-pie' \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/stage/kraken
cmake --build build/kraken-client
~~~

Подготовьте PEM с явно выбранным доверенным корнем для текущей цепочки сертификатов Kraken. Используйте корни, разрешённые в вашей среде. Запустите установленный шлюз из дерева исходников.

```sh
stage/kraken/bin/qdcxx-kraken-gateway --trust-pem /path/to/kraken-ca.pem --listen-port 7400 --duration-ms 30000
```

После строки `LISTEN 127.0.0.1 7400` запустите один или несколько клиентов Feed в отдельных терминалах.

```sh
build/kraken-client/qdcxx-feed-demo --run --address 127.0.0.1:7400 --symbol BTC/USD --duration-ms 10000
```

Для автоматического выбора порта используйте `--listen-port 0`. Фактический порт из строки `LISTEN` передайте клиентам. Адреса и доверенные сертификаты заданы явно. `--help` выводит параметры без открытия сокетов. Шлюз подписывается на источник после появления спроса на BTC/USD. Для каждой новой сессии публикация начинается после подтверждения подписки и свежего снимка.

Quote содержит точные цены и объёмы в представлении wide decimal. Метки времени, номер последовательности и коды биржи в Quote равны нулю. Опция `--trace-quotes` выводит время от поставщика и принятые сырые значения Quote. TICKER может объединять промежуточные обновления. Строка статистики отражает локальную публикацию и транспорт. Для проверки доставки смотрите котировки, полученные каждым клиентом.

Для совместимого локального WSS-стенда укажите `--source-host`, `--source-identity`, `--source-port` и его доверенный PEM. Путь HTTP — `/v2`. Шлюз поддерживает один публичный профиль источника и QTP без TLS на IPv4 loopback.
