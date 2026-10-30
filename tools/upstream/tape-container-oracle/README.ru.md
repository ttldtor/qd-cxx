# Production oracle контейнеров tape

[English version](README.md). Английская версия является нормативной.

`ContainerOracle.java` использует фактические Java QD FileWriter, FileReader и StreamCompression
с одной обычной детерминированной записью. Сохраняются наблюдения NONE/GZIP/ZIP, усечённые headers/
trailers, CRC failure, хвостовые байты, concatenated gzip members и несколько ZIP файлов. Direct
decompressor также проверяется одно-байтовыми reads при available=0. Мутации — диагностические
входы, не вторая реализация parser. FileReader перехватывает I/O failures и пишет лог, поэтому
canonical record counts отделены от direct decompressor outcomes.

Launcher использует имеющийся JDK 8 и ровно четырнадцать production class directories. Компилирует
только helper в свежий output directory, не вызывает Maven и сравнивает полные inventories
production classes до/после. Переданный revision должен быть baseline
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` или отдельный current
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Identity revision предоставляется upstream provenance
вызывающего кода; launcher фиксирует фактические class hashes, но не аутентифицирует Git checkout.

## Справка и запуск

Требуются PowerShell 7.4, Windows JDK 8, существующий native `qdcxx-run-utc` и UTF8 JSON array
абсолютных production class directories в runtime order. Справка и отсутствие аргументов не
требуют рабочих входов, не выполняют capture и не получают зависимости:

```powershell
Get-Help ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 -Help
```

Пример с путями вызывающего кода и свежими частными каталогами:

```powershell
$parameters = @{
    JavaHome = 'C:/jdk8'
    ClasspathJson = 'C:/private/production-classpath.json'
    Revision = '0ddc0a09779fb61d74cc0b37c6449ff47dcb715f'
    Logger = 'C:/private/dev/qdcxx-run-utc.exe'
}
& ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 @parameters `
    -Mode Capture -OutputDirectory 'C:/private/container-capture'
& ./tools/upstream/tape-container-oracle/Invoke-Oracle.ps1 @parameters `
    -Mode Verify -ExpectedDirectory 'C:/private/container-capture' `
    -OutputDirectory 'C:/private/container-verify'
```

`Mode` по умолчанию Capture. Verify дополнительно сравнивает точные байты canonical `results.tsv`
с expected directory. Output directory всегда свежий; evidence не удаляется и не переписывается.
У каждого child timeout30 секунд; останавливается только собственное дерево процессов. Exit0 —
справка или успешный capture/verify, exit1 — аргументы, timeout, child failure, изменённый production
inventory или canonical mismatch. При ошибках raw streams/logs сохраняются.

## Provenance и ограничения

Перед capture запускается [Java formatter](../../format/README.ru.md), затем Capture и Verify
для каждого изменённого source/launcher. Metadata содержит фактические source/launcher/JDK/logger
hashes, revision, classpath inventories, argv, UTC intervals и exit status. Сохраняются raw UTC/
stdout/stderr и fixture hashes. Старые expectations/hashes не обновляются автоматически. Изменение
source требует новых metadata и фактического Verify.

Evidence содержит локальные source/output paths и хранится частно. Публикуются только проверенные
synthetic bytes, безопасные canonical rows и relative provenance summaries. ZIP writer timestamps
меняют physical hashes между запусками; Verify сравнивает decoded payload, failure class и
production callbacks, не identity нового архива. Исходные raw hashes сохраняют identity captures.

Инструмент доказывает выбранные production observations, но не C++ acceptance, полную ZIP
совместимость, поддержку private files, установленный runtime или layered COMPLETE.
[Контракт контейнеров](../../../spec/protocol/TAPE-CONTAINERS.ru.md) определяет более строгий
конечный C++ профиль. Linux native decoder tests — отдельная проверка; launcher только Windows.
