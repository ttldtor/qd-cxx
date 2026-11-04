# Явная подготовка зависимости IPF transfer

[English version](IPF-TRANSFER.md). Английский текст является нормативным.

Эти команды готовят только private [профиль зависимости Boost 1.92.0](../../cmake/ipf-transfer-profile/README.ru.md).
Они не добавляют production HTTP/file facade. Нет глобальной установки, package manager, fetch при
обычном configure, SDK bootstrap script или запуска compiler из provisioner `cmake -P`.
Нужны доверенные локальные каталоги без конкурентной подмены путей посторонними процессами.

## Получение исходников

Из корня репозитория с CMake 3.25+ сначала поместите точный официальный архив в выбранный cache, затем используйте явный путь с проверенным архивом и offline-подготовкой:

```powershell
cmake '-DDESTINATION=.work/ipf-boost-source' '-DARCHIVE_CACHE=.work/ipf-archives' `
  -DOFFLINE=ON -P tools/deps/ProvisionIpfTransfer.cmake
```

Принимается только точный [официальный архив](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz)
с [опубликованными metadata](https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz.json).
Его SHA256 и все выбранные source/license hashes закреплены в manifest профиля.
`-DOFFLINE=ON` требует готового архива в `ARCHIVE_CACHE` либо завершённого destination.
Для необязательного получения по HTTPS опустите `-DOFFLINE=ON`. Загрузка и простой имеют конечные timeouts; ошибка сети либо медленное соединение могут завершить попытку с сохранением partial download. Для медленной сети получите точный архив отдельно и используйте offline-путь.
Результат — выбранный source directory и соседний `DESTINATION.receipt.json`; завершённое дерево
всегда заново хешируется перед использованием. Этот helper не компилирует Boost.

| Параметр | Значение по умолчанию и контракт |
| --- | --- |
| `HELP=ON` | Справка до обязательных inputs, проверки путей, сети и записи. `LANGUAGE=en` (по умолчанию) или `ru`. |
| `ENABLED` | `ON`; `OFF` завершает работу без inputs, discovery, сети и записи. |
| `DESTINATION` | Обязательный свежий source directory либо завершённый каталог для проверки; относительно каталога вызова. |
| `ARCHIVE_CACHE` | Соседний `archives`; не должен пересекаться с destination/staging. |
| `OFFLINE` | `OFF`; `ON` запрещает загрузку. |
| `DOWNLOAD_TIMEOUT` | `120` секунд, целое 1..3600, общее время и простой; автоматических повторов нет. |

Exit 0: успех/help/OFF. Exit 1: ошибка CMake/input/path/cache/download/source. Диагностика использует
`QDCXX_IPF_DEPS_INPUT`, `PATH`, `BUSY`, `PARTIAL`, `CACHE_MISSING`, `ARCHIVE_CORRUPT`, `DOWNLOAD`,
`SOURCE_CORRUPT`, `PUBLISH` и `QDCXX_IPF_SOURCE`. Повреждённые archives/sources не исправляются и не
перезаписываются. Незавершённые `.part`, `.partial` и destinations сохраняются и отклоняются.
Проверьте неудачную попытку и выберите новые пути. Symlink/junction и redirected paths запрещены.
Helper создаёт parent/cache directories, locks, downloaded archive и fresh staging. Публикация —
обычный atomic rename под cooperative destination/cache locks в доверенных локальных Windows/Linux
filesystems, включая DrvFS. Защита от враждебной конкурентной подмены путей не обещана.
Сбой между rename исходников и публикацией соседнего receipt оставляет неполный destination,
который повторный запуск отклоняет, а не исправляет. Пустой owned staging после успешной публикации удаляется.

## Явная static сборка, проверка и установка

Используйте developer shell с уже выбранным компилятором. На Windows выберите
MSVC 19.42 x64 и release dynamic CRT; scripts не угадывают и не меняют toolchain оболочки:

```powershell
cmake -S cmake/ipf-transfer-profile -B .work/ipf-sdk-build -G Ninja `
  -DORG_TTLDTOR_QDCXX_IPF_TRANSFER_DEPENDENCY=ON `
  '-DORG_TTLDTOR_QDCXX_IPF_TRANSFER_SOURCE=C:/absolute/project/.work/ipf-boost-source' `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
  '-DCMAKE_INSTALL_PREFIX=C:/absolute/project/.work/ipf-sdk'
cmake --build .work/ipf-sdk-build --parallel 1
ctest --test-dir .work/ipf-sdk-build --output-on-failure
cmake --install .work/ipf-sdk-build
cmake '-DSDK_ROOT=.work/ipf-sdk' -P tools/deps/SealIpfTransferSdk.cmake
```

Linux использует GCC 13.3.0 x86-64 с теми же source/install arguments, `-DCMAKE_BUILD_TYPE=Debug` и
`'-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer'`; MSVC options опустите.
Для каждой платформы нужны отдельные свежие build/install directories. Отдельная option по умолчанию OFF:
OFF не включает compiler, не ищет, не загружает и не линкует SDK.

Установленный package содержит `source/` со всеми 417 файлами/license, `lib/` со static URL archive
и относительными package files, `share/qdcxx-ipf-transfer/` с pinned manifest/build metadata.
`SealIpfTransferSdk.cmake` требует явный `SDK_ROOT`, рабочего пути по умолчанию нет. Он проверяет
pinned sources/metadata и записывает только `share/qdcxx-ipf-transfer/seal.json` и cooperative lock.
Существующие seals только проверяются, никогда не перезаписываются. EN/RU `HELP=ON` не требует inputs
и не создаёт files. Exit 0: успех/help; exit 1: неверные source/profile/artifact/path/lock.
Seal хеширует каждый source/archive/package file, используемый consumers; receipt и lock исключены
из собственного inventory. Он связывает actual local static archive и flags с идентичностью sources.
Он не выполняет tests и не удостоверяет недоверенный package; CTest надо выполнить до seal.
Разные build paths могут дать разные байты архива. Совпадающий source profile — воспроизводимая
подготовка, а не единый универсальный artifact hash и не гарантия binary/ODR isolation.

## Установленный и перемещённый consumer

Переместите либо скопируйте полный sealed SDK, затем настройте отдельный consumer с новым расположением:

```powershell
cmake -S tests/conformance/ipf-transfer-dependency/consumer -B .work/ipf-consumer -G Ninja `
  '-DCMAKE_PREFIX_PATH=C:/relocated/ipf-sdk' -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
cmake --build .work/ipf-consumer --parallel 1
ctest --test-dir .work/ipf-consumer --output-on-failure
```

На Linux используйте те же GCC Debug/sanitizer flags. `find_package(qdcxx_ipf_transfer_sdk CONFIG REQUIRED)`
импортирует `qdcxx_private::url` только после проверки host/source/artifact/inventory. Package не зависит
от прежнего source/build path. Отсутствующий SDK, неверный host/CRT, изменённые sources/archive/package
files либо лишние файлы SDK приводят к ошибке configure. Это consumer внутренней зависимости; public IPF
interfaces не меняются и не должны раскрывать Boost types.

## Проверки helpers

```powershell
cmake -DHELP=ON -P tools/deps/ProvisionIpfTransfer.cmake
cmake -DHELP=ON -DLANGUAGE=ru -P tools/deps/SealIpfTransferSdk.cmake
cmake '-DTEST_ROOT=.work/ipf-deps-tests' '-DARCHIVE_CACHE=.work/ipf-archives' `
  -P tools/deps/TestsIpfTransfer.cmake
```

`TestsIpfTransfer.cmake` требует свежий `TEST_ROOT`, имеет EN/RU `HELP=ON`, не запускает compiler или сеть
и сохраняет per-case logs/fixtures. Без необязательного cache выполняются help/OFF и отрицательные случаи;
с cache дополнительно проверяются offline provision/readback и повреждение source. Exit 0 означает, что
выполненные cases успешны; exit 1 — invalid inputs или unexpected behavior. Запрет создания symlink явно
помечается `NOT_CHECKED`, а не успешной проверкой. Полная transfer/authentication/TLS/cancellation
qualification относится к production facade и этими dependency checks не доказывается.
