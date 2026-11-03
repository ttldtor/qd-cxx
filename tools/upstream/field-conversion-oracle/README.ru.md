# Oracle преобразования полей

[English version](README.md). Английский текст является основным.

`Invoke-Oracle.ps1` захватывает или проверяет [контракт преобразований по схеме](../../../spec/events/FIELD-CONVERSIONS.ru.md)
через существующие production classes точной ревизии `upstream.lock`. Он вызывает generated
QuoteMapping/ProfileMapping и настоящие delegates MarketFactoryImpl для настроенных production
записей. Диагностическая копия числовых формул не реализуется.

```powershell
Get-Help ./tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/field-conversion-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/conversion-verify
```

Без аргументов и с `-Help` выводится английская Get-Help; `-HelpRu` показывает русскую справку.
Справка игнорирует рабочие пути и не запускает процессы, проверку входов, создание каталогов,
сеть или SDK. Рабочий режим по умолчанию Verify. Для Capture/Verify обязательны четыре пути:
Upstream — существующий закреплённый checkout с готовыми classes; JavaHome — JDK8;
Logger — существующий UTC logger с передачей argv; RunDirectory — новый каталог внутри `.work`
данного checkout. Перенаправляющие предки и существующий run-каталог отвергаются. Форматтер
использует отдельный JDK и не собирает reference.

Helper запускает по одному ограниченному subprocess, включая `java -version`: heap 128MiB и
timeout 60 секунд. Останавливается только собственное дерево процесса при timeout. Harness
компилируется в новый run-каталог, затем выполняются наблюдения UTC и Pacific/Honolulu.
Записываются полные команды, raw stdout/stderr, UTC start/end/exit, source hashes и production-class
inventories до/после. Изменённый относящийся к работе upstream source, неверная ревизия,
различие timezone результатов или изменение class inventory отвергаются. Загрузки зависимостей,
сетевые подключения, upstream build и изменение upstream файлов не выполняются.

Capture заменяет только `tests/fixtures/field-conversions/oracle.tsv` и его `provenance.json`
кандидатами наблюдений. Verify сравнивает эти файлы и provenance harness/launcher/source/classes,
не заменяя их. Оба режима сохраняют рабочие логи в RunDirectory. Public provenance имеет UTF-8
без BOM с LF; `.gitattributes` сохраняет raw TSV bytes.

Exit0 означает успех или справку. Exit1 означает неверные/отсутствующие входы, нарушение containment,
недоступный/неверный reference, timeout, subprocess error, различие вывода или provenance. Логи
ошибки сохраняются; повторный запуск требует нового run-каталога.

[Перечень capture](../../../tests/fixtures/field-conversions/CAPTURES.ru.md) описывает положительные,
граничные, missing и отрицательные случаи. Отказ pinned factory для BYTE/SHORT фиксируется прямо.
Каждый из четырёх Quote и десяти Profile публичных business-double getters фиксируется отдельно,
включая собственный missing default, для сравнения с typed native business getters.
Строковые случаи перед созданием delegate проходят реальные encoded bytes через production
StringField read/write. Production binary64 результаты delegate отделены от WideDecimal accessor
bits; они не означают, что композиция LONG с последующим binary64 представлением совпадает с
прямым LONG-to-binary64. Это offline доказательства production mapping/delegate, а не live service test.
