# Явная подготовка зависимостей runtime

[English version](README.md). Английская версия нормативная.

`ProvisionRuntime.cmake` подготавливает существующие квалифицированные исходники
standalone Asio 1.38.2 и c-ares 1.34.9+qdcxx2 в выбранном вызывающим кодом каталоге.
Это отдельный шаг: обычный configure qd-cxx его не вызывает и SDK не загружает.
Глобальной установки нет; TLS, JSON, компиляторы и инструменты сборки не подготавливаются.

## Подготовить исходники

Нужны CMake 3.25+ и явно выбранный GNU patch. На Windows он доступен в Git for Windows,
на Linux укажите существующий `/usr/bin/patch`. Из корня репозитория, подставив путь:

```powershell
cmake '-DDESTINATION=.work/runtime-sdk' '-DARCHIVE_CACHE=.work/runtime-archives' `
  '-DPATCH_EXECUTABLE=C:/Program Files/Git/usr/bin/patch.exe' -P tools/deps/ProvisionRuntime.cmake
```

На Linux аргументы те же, с `-DPATCH_EXECUTABLE=/usr/bin/patch`. Загружаются только
точные [официальный архив Asio](https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz)
и [официальный архив c-ares](https://github.com/c-ares/c-ares/releases/download/v1.34.9/c-ares-1.34.9.tar.gz).
Существующие [квалификационные receipts](../../cmake/runtime-profile/README.ru.md)
задают SHA256 архивов/патча и каждого проверяемого файла. Из Asio извлекаются только
636 include-файлов и исходная лицензия BSL; две служебные символические ссылки
дистрибутива не извлекаются. Полное дерево c-ares из 532 файлов сохраняет лицензию MIT.
Cumulative-патч qdcxx2 применяется к свежему stock c-ares с
`--batch --forward --fuzz=0 --binary`; offset/fuzz и любые несовпадения конечных
исходников приводят к отказу. Скрипты, configure и программы из SDK не исполняются.

Результат внутри `DESTINATION`:

- `asio-asio-1-38-2/include` и соседний `LICENSE_1_0.txt`;
- `c-ares-1.34.9-qdcxx2`, включая `LICENSE.md`;
- `qdcxx-runtime-deps.json` — receipt завершения с относительными путями.

Передайте эти явные пути обычной сборке библиотеки:

```powershell
cmake -S . -B build/feed -G Ninja '-DCMAKE_BUILD_TYPE=Release' `
  '-DCMAKE_TRY_COMPILE_CONFIGURATION=Release' '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' `
  '-DORG_TTLDTOR_QDCXX_BUILD_FEED_API=ON' '-DORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON' `
  '-DORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON' '-DORG_TTLDTOR_QDCXX_BUILD_RECOVERY=ON' `
  '-DORG_TTLDTOR_QDCXX_ASIO_INCLUDE=.work/runtime-sdk/asio-asio-1-38-2/include' `
  '-DORG_TTLDTOR_QDCXX_CARES_SOURCE=.work/runtime-sdk/c-ares-1.34.9-qdcxx2'
cmake --build build/feed --target qdcxx_feed --config Release --parallel 2
```

При configure из другого каталога задавайте абсолютные пути SDK. На Linux применяйте
квалифицированный профиль GCC без параметров MSVC; см. [инструкцию сборки](../../docs/CPP-BUILD.ru.md#public-feed-api-build-ru).
Подготовка исходников не расширяет поддержанные архитектуры и контракт компилятора/ABI.

## Offline и поведение кэша

`-DOFFLINE=ON` запрещает загрузку. Заранее поместите оригинальные
`asio-1.38.2.tar.gz` и `c-ares-1.34.9.tar.gz` в `ARCHIVE_CACHE`; до извлечения helper
проверит SHA256. Готовый существующий destination повторно проверяется по всем 637/532
записям без архивов и GNU patch. Повторный вызов не патчит квалифицированное дерево
и не исправляет/перезаписывает повреждённые входные данные.

| Параметр | Значение по умолчанию и контракт |
| --- | --- |
| `HELP=ON` | Справка до проверки обязательных параметров, файлов и выполнения работы. Язык: `LANGUAGE=en` или `ru`. |
| `ENABLED` | `ON` для явно вызванного helper; `OFF` завершает работу без проверки входных данных и зависимостей. Независим от опций корневой сборки. |
| `DESTINATION` | Обязателен: новый bundle исходников или готовый bundle для проверки. Относительные пути считаются от каталога запуска. |
| `ARCHIVE_CACHE` | Соседний каталог `archives`; не должен пересекаться с destination/staging. Готовые корректные архивы используются повторно. |
| `OFFLINE` | `OFF`; при `ON` нужны кэшированные архивы или готовый destination. |
| `PATCH_EXECUTABLE` | Явный абсолютный путь GNU patch для новой подготовки. Для перечитывания готового кэша не нужен. |
| `DOWNLOAD_TIMEOUT` | `120` секунд, целое 1..3600; предел всей загрузки и простоя. Автоматического повтора нет. |

Код 0: успех, справка или OFF. Код 1: ошибка CMake/параметров, кэша, загрузки, патча,
пути или проверки; ошибки запуска CMake сохраняют свой код. Диагностика различает
`QDCXX_DEPS_CACHE_MISSING` (нет offline-архива), `QDCXX_DEPS_ARCHIVE_CORRUPT`
(неверные байты, без перезаписи), `QDCXX_DEPS_PARTIAL` (незавершённая загрузка,
staging или destination), `QDCXX_DEPS_PATCH_*`, `QDCXX_DEPS_PATH`, `QDCXX_DEPS_BUSY`
и ошибки отсутствия/хеша/набора файлов source-manifest. Одному receipt завершения
helper никогда не доверяет.

Создаются родительские/cache-каталоги, lock-файлы, загрузки и свежий соседний
`DESTINATION.partial`. Неблокирующий lock destination/cache исключает другие
сотрудничающие процессы helper. Символические ссылки/junction и перенаправленные
пути отклоняются. Пути архива проверяются до извлечения; принимаются только точные
закреплённые байты. Оба SDK проверяются до публикации bundle одним переименованием.
Для каталогов используется обычное атомарное переименование после повторной проверки отсутствия destination и путей под lock helper, в том числе на DrvFS. В модели сотрудничающих процессов с lock и доверенными каталогами существующий destination не удаляется и не заменяется; конкурентная подмена пути посторонним процессом вне этой модели. Для загруженных обычных файлов архивов сохраняется атомарная публикация `NO_REPLACE`. При ошибке staging/download сохраняются для диагностики; следующий вызов их отклоняет.
Проверьте и сохраните нужные артефакты, затем выберите свежие пути или вручную удалите
только известные неудачные артефакты. Helper не удаляет каталоги. Используйте доверенные
локальные каталоги без конкурентной замены путей: advisory-lock не защищает от
постороннего процесса, подменяющего пути файловой системы.

## Проверить helper

```text
cmake -DHELP=ON -P tools/deps/ProvisionRuntime.cmake
cmake -DHELP=ON -DLANGUAGE=ru -P tools/deps/ProvisionRuntime.cmake
cmake -DTEST_ROOT=.work/deps-cli-tests -P tools/deps/Tests.cmake
```

Самостоятельные проверки требуют свежий выходной каталог. Они проверяют help/OFF
без обязательных входных данных и артефактов, блокировки конкурирующего writer, пустые/непустые destination/staging, отсутствующий/повреждённый/частичный
кэш, сохранение staging, пересечение путей, повреждение исходников и запрет ссылок,
если система разрешает их создавать (иначе явно `NOT_CHECKED`). Добавьте
`-DARCHIVE_CACHE=/готовые/архивы -DPATCH_EXECUTABLE=/gnu/patch` для реального
извлечения архивов, cumulative-патча и полного повторного чтения source-кэша.
Артефакты и per-case команды/вывод сохраняются, удаления и загрузок нет.
[Квалификация runtime](../../cmake/runtime-profile/README.ru.md) остаётся основанием
контракта SDK; helper предоставляет воспроизводимые входные данные.
