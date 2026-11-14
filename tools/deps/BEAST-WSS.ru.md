# Подготовка внутренней зависимости Beast WSS

[English version](BEAST-WSS.md). Английская версия нормативная.

## Подготовить заголовки из локального архива

Нужен CMake 3.25 или новее. Поместите точный архив Boost 1.92.0 в локальный кэш и передайте путь к нему. Сценарий извлекает только файлы из закреплённого [манифеста](../../cmake/beast-wss-profile/boost-1.92.0-sources.sha256.tsv) и проверяет каждый хеш. Используйте доверенные локальные каталоги с согласованной записью. [Официальный архив Boost](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz)

```powershell
cmake '-DARCHIVE=.work/archives/boost_1_92_0.tar.gz' '-DDESTINATION=.work/wss-headers' `
  -P tools/deps/ProvisionBeastWss.cmake
```

| Параметр | Поведение |
| --- | --- |
| `HELP=ON` | Печатает справку до проверки входных данных или записи. `LANGUAGE=en` по умолчанию или `ru`. |
| `ENABLED=OFF` | Пропускает подготовку. По умолчанию ON. |
| `ARCHIVE` | Обязателен для нового каталога назначения. Абсолютный путь или путь от текущего каталога. |
| `DESTINATION` | Обязательный выходной каталог. Готовое дерево повторно проверяется без архива. |

Сценарий создаёт соседнюю блокировку и временный каталог, затем публикует проверенное дерево переименованием. Он отклоняет перенаправленные пути и сохраняет незавершённый каталог или повреждённый вход для проверки. Подготовка обходится без сети, компилятора и глобальных настроек. Код 0 означает успешную проверку, справку или OFF. Ошибка ввода, файла или проверки возвращает ненулевой код.

## Собрать и установить

На Windows используйте окружение MSVC 19.42 x64 Release с динамическим CRT, на Linux — GCC 13.3.0 x86-64 Debug с ASan/UBSan. Установка OpenSSL должна соответствовать профилю проекта. [Квалифицированный SDK OpenSSL](../../docs/CPP-BUILD.ru.md#tls-source-profile-cpp-build-ru)

```powershell
cmake -S cmake/beast-wss-profile -B .work/wss-build -G Ninja `
  -DORG_TTLDTOR_QDCXX_BEAST_WSS_DEPENDENCY=ON `
  '-DORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE=.work/wss-headers' `
  '-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/wss-build
ctest --test-dir .work/wss-build --output-on-failure
cmake --install .work/wss-build --prefix .work/wss-sdk
```

На Linux передайте путь к Linux SDK OpenSSL, `-DCMAKE_BUILD_TYPE=Debug`, `-DCMAKE_CXX_COMPILER=g++-13` и `'-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'` вместо флагов сборки для Windows. Используйте отдельные каталоги сборки и установки для двух платформ. Отдельный профиль по умолчанию выключен и завершает работу до включения C++ и проверки зависимостей. Обычный configure qd-cxx его не вызывает.

Установка содержит проверенные заголовки и лицензию в `source/` и относительную конфигурацию пакета в `lib/cmake/qdcxx_beast_wss_sdk/`. OpenSSL остаётся явным входом потребителя. Цель адаптера линкует `qdcxx_private::beast_wss` с областью `PRIVATE`.

## Проверить установленный пакет

Перенесите или скопируйте каталог установки. Настройте [пробу потребителя](../../tests/conformance/beast-wss-dependency/consumer/CMakeLists.txt) в новом каталоге сборки с абсолютным путём к копии в `-DSDK_ROOT`, тем же компилятором и флагами и явно указанным SDK OpenSSL. Соберите пробу для проверки импорта, компиляции и линковки.

```powershell
cmake -S tests/conformance/beast-wss-dependency/consumer -B .work/wss-consumer -G Ninja `
  '-DSDK_ROOT=C:/absolute/project/.work/wss-sdk-relocated' `
  '-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=C:/sdk/openssl-qualified' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/wss-consumer
```

Пробы сборки и потребления не подключаются к сети. Проверка относится к указанному [профилю зависимостей](../../cmake/beast-wss-profile/README.ru.md).
