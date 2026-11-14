# Построить цепочку опционов из файла IPF

[English version](README.md).

[main.cpp](main.cpp) читает полный локальный файл IPF в UTF-8 через `readInstrumentProfilesFile`, строит цепочки через `OptionChainsBuilder::build` и выводит серии для одного базового актива. Для приложения достаточно пакета IPF.

Соберите пакет с `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`. Пример можно собрать из текущего каталога или из `share/qdcxx/examples/ipf_option_chain` установленного SDK. Укажите путь к этому SDK:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/qd-cxx
cmake --build build
ctest --test-dir build --output-on-failure
./build/qdcxx-ipf-option-chain example.ipf ABC
```

На Windows добавьте расширение `.exe`. Многоконфигурационные генераторы помещают бинарник в каталог выбранной конфигурации.

В [example.ipf](example.ipf) находятся call и put со страйком 10 из [входных данных Java-эталона цепочек](https://github.com/ttldtor/qd-cxx/blob/master/tests/fixtures/option-chains/README.ru.md). Это воспроизводимый пример.

```text
ABC series=1
calls=1 puts=1 strikes: 10
```

Для других инструментов используйте свой несжатый файл IPF с маркером завершения. Получение обновлений по сети показано в отдельном [примере live IPF](https://github.com/ttldtor/qd-cxx/blob/master/examples/ipf_live/README.ru.md).
