# Подписка на Quote

[English version](README.md) · [Руководство Feed API](../../docs/FEED-API.ru.md)

[Полный пример](main.cpp) создаёт endpoint, подписывается на Quote и выводит цены и размеры bid и ask.

Соберите его с установленным пакетом qd-cxx и совместимым компилятором C++23. В Windows выполняйте команды из того же настроенного терминала MSVC, который использовался для пакета.

```powershell
cmake -S examples/feed_quote -B build/feed-quote -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/path/to/qdcxx
cmake --build build/feed-quote
```

### Presets установленного пакета {#sample-build-profiles-ru}

Для пакета Linux с ASan/UBSan используйте соответствующий Debug sanitizer preset с настройками пакета. В этом каталоге доступны configure и build presets qdcxx-sample-windows и qdcxx-sample-linux. Если пакет установлен в другом месте, задайте CMAKE_PREFIX_PATH. См. [профили сборки](../../docs/CPP-BUILD.ru.md#portable-feed-profiles-ru).

Запустите с параметрами по умолчанию или укажите адрес, символ и длительность.

```powershell
./build/feed-quote/qdcxx-feed-demo.exe --run
./build/feed-quote/qdcxx-feed-demo.exe --run --address demo.dxfeed.com:7300 --symbol AAPL --duration-ms 10000
```

Параметры и коды завершения перечислены в `--help`. Соединение использует обычный TCP. Доступность и права зависят от источника. Демоданные могут приходить с задержкой. После указанного интервала приложение закрывает endpoint. Сохранённая диагностика endpoint выводится и при успешном завершении локальной очереди. Ctrl+C использует стандартное для платформы поведение процесса.
