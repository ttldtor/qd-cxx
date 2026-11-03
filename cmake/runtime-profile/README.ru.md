# Квалифицированные исходники runtime

[English version](README.md). Английская версия нормативная.

Для воспроизводимого получения и патча с нулевым fuzz используйте отдельный [helper зависимостей runtime](../../tools/deps/README.ru.md). Он загружает только закреплённые архивы при явном вызове, поддерживает offline-кэш архивов/исходников и использует эти полные source-receipts. Обычный configure по-прежнему ничего не загружает.

Сборка runtime получает каталоги исходников явно. Конфигурация ничего не скачивает
и не устанавливает. При runtime OFF эти каталоги не проверяются и инструменты
сборки зависимостей не требуются.

| SDK | Идентичность исходников | Проверяемые файлы |
| --- | --- | --- |
| Standalone Asio 1.38.2 | Commit `8806a6803cde7054c3049d3666d3ec36786568c5`; [receipt](asio-1.38.2.sources.json) | 636 файлов include и исходная лицензия BSL-1.0 |
| c-ares 1.34.9+qdcxx2 | Upstream commit `bc91829cce983a35ec01f5dbc05465858a3df469`; [receipt](c-ares-1.34.9-qdcxx2.sources.json) | 532 файла исходников/лицензии |

Receipts содержат хеши исходных архивов и всех квалифицированных файлов. Одного
заголовка версии недостаточно, чтобы связать проверку ресурсов с компилируемыми
исходниками. Включённая конфигурация проверяет полный соответствующий набор файлов
и SHA-256 перед сборкой SDK.

## Подготовка c-ares

Явно получите [исходный архив релиза](https://github.com/c-ares/c-ares/releases/download/v1.34.9/c-ares-1.34.9.tar.gz)
и сохраните оригинальные MIT notices. Примените
[кумулятивную поправку](c-ares-1.34.9-qdcxx2.patch) к новой распаковке этого архива.
Она уже включает прежние ограничения конфигурации; поверх qdcxx1 её применять
не нужно. Stock c-ares и qdcxx1 — другие, неподдерживаемые входы для runtime.

Пример PowerShell из корня репозитория с GNU patch из Git for Windows
(явные пути инструмента/архива замените на соответствующие своей установке):

```powershell
$runtimeArchive = (Resolve-Path -LiteralPath '.work/deps/c-ares-1.34.9.tar.gz').Path
$runtimeSource = Join-Path $PWD '.work/deps/c-ares-1.34.9-qdcxx2'
$runtimePatch = (Resolve-Path -LiteralPath 'cmake/runtime-profile/c-ares-1.34.9-qdcxx2.patch').Path
$runtimePatchTool = 'C:/Program Files/Git/usr/bin/patch.exe'
if ((Get-FileHash -LiteralPath $runtimeArchive).Hash -ne '9364b70a12eb59a06830fb488d9357f90100dff0d98b23ea3593a183d43e6f2f') {
    throw 'Unexpected upstream archive'
}
if (Test-Path -LiteralPath $runtimeSource) { throw 'Choose a fresh source directory' }
New-Item -ItemType Directory -Path $runtimeSource | Out-Null
tar -xzf $runtimeArchive -C $runtimeSource --strip-components 1
if ($LASTEXITCODE -ne 0) { throw 'Extraction failed' }
& $runtimePatchTool --batch --forward --fuzz=0 -p1 --directory $runtimeSource --input $runtimePatch
if ($LASTEXITCODE -ne 0) { throw 'Patch application failed' }
```

Нативный GNU patch на Linux использует те же параметры. Хеш поправки:
`65a92251cf4dd90933ec4df743e87d83ff01ac62d4d01d7ed6a9c9b79a020b7e`.
Процедура от архива до квалифицированных исходников проверена на Windows и Linux
с нулевым fuzz и совпадением хешей всех 532 файлов.

Поправка qdcxx2 делает передачу response в отложенную очередь явной: при отказе
освобождает невставленный response, снимает connection/timeout linkage и передаёт
ошибку, сохраняя query для штатного уничтожения канала. Прикладной callback
на ошибочной ветви не добавляется. Аллокаторы по умолчанию, ограничения потоков
и конфигурация числовых DNS-серверов сохранены.

Четыре проверки владения используют корректные объекты и переданный статус ошибки
admission. Они проверяют cleanup, однократное уничтожение и время жизни borrowed
callback. Проверки прошли на Windows Release и Linux Debug ASan/UBSan; отдельный
запуск этих случаев с включённым LeakSanitizer тоже прошёл. Это не воспроизведение
реального отказа malloc и не доказательство полного бюджета памяти/задержек runtime.

## Подготовка Asio

Используйте [закреплённый архив](https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-38-2.tar.gz)
с SHA-256 `9f2648fa483e58a6bf848d970ee0ea650ca19ed7769dfa520ed4f7b8d27af1db`.
Распакуйте, не меняя заголовки, и сохраните `LICENSE_1_0.txt` рядом с `include`.
При включении runtime передайте этот `include` в `ORG_TTLDTOR_QDCXX_ASIO_INCLUDE`,
а квалифицированный корень c-ares — в `ORG_TTLDTOR_QDCXX_CARES_SOURCE`.

Прежний [профиль проверки реализуемости](../../tests/conformance/transport/runtime-feasibility/README.ru.md)
сохраняет исходные входы qdcxx1 и доказательства. Его результаты не переобозначены
как тесты qdcxx2 или полная квалификация ресурсов production.
