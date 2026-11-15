# Котировки страйков опционов

[English](README.md)

[main.cpp](main.cpp) читает полный локальный IPF, выбирает до четырёх страйков вокруг заданной цены в первой серии опционов и подписывается на доступные call и put. Выводятся символ, bid и ask. Серии упорядочены по атрибутам, начиная с даты экспирации; динамический выбор экспирации пример не выполняет.

Соберите и установите пакет qd-cxx с `ORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON` и `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`. Для примера нужен совместимый компилятор C++23:

```sh
cmake -S examples/feed_option_quotes -B build/option-quotes -DCMAKE_PREFIX_PATH=/path/to/qdcxx
cmake --build build/option-quotes
ctest --test-dir build/option-quotes --output-on-failure
```

Запуск: `qdcxx-option-quotes FILE UNDERLYING CENTER [ADDRESS]`. Например, укажите свой полный несжатый IPF и адрес источника с данными для соответствующих опционов:

```sh
build/option-quotes/qdcxx-option-quotes options.ipf SPY 500 demo.dxfeed.com:7300
```

Лимиты чтения IPF — 64 МиБ и 100 000 профилей. По умолчанию используется `demo.dxfeed.com:7300`; подписка по TCP работает десять секунд. Доступность данных и права зависят от источника. В [example.ipf](example.ipf) находятся вымышленные символы для проверки выбора, не рыночные данные:

```sh
build/option-quotes/qdcxx-option-quotes examples/feed_option_quotes/example.ipf ABC 11 --list
```

`--list` выводит выбранные символы без подключения. Ограничения ввода и коды завершения приведены в `--help`. В Windows используйте `.exe`; многоконфигурационные сборки помещают его в каталог выбранной конфигурации.

При сборке из установленного каталога примера используйте `-S .`.
