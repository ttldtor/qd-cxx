# Oracle локального IPF writer

[English version](README.md). Английская версия является основной.

`pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Help` и `-HelpRu` показывают
встроенную EN/RU справку без inputs, каталогов, Java, сети и SDK work.

```powershell
pwsh -File tools/upstream/ipf-writer-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe `
  -RunDirectory .work/ipf-writer-verification
```

Launcher использует готовые production classes pinned QD3.355 и JDK8. Verify — default;
Capture записывает только writer fixture/provenance candidates. Для работы нужны Upstream,
JavaHome, Logger и RunDirectory. RunDirectory должен быть внутри .work этого checkout; для
capture нужен новый собственный каталог: launcher заменяет names, но не удаляет историю/orphans.
Каждый собственный процесс ограничен128MiB heap/60секундами; останавливается только его дерево
при timeout. Процессы идут по одному, сохраняются полные logs/source/class/tool hashes.
Exit0 означает успех/справку; exit1 — неверные/отсутствующие inputs, несовпадение provenance/
observations, недоступный reference, timeout либо сбой процесса. Download/build/network нет.

Необязательный `-NativeDirectory <directory>` дополнительно читает точные25 accepted native
.ipf outputs actual pinned Java reader, проверяет byte identity с Java writer и canonical
reader outcomes, пишет native-read.tsv внутри RunDirectory. Native tests создают эти файлы,
только если test-only environment variable `ORG_TTLDTOR_QDCXX_IPF_WRITER_ORACLE_OUTPUT`
указывает собственный output directory; это не свойство библиотеки. Extra/orphan files запрещены.

[Fixtures](../../../tests/fixtures/ipf-writer/README.ru.md) и
[writer contract](../../../spec/ipf/WRITER.ru.md) определяют фиксированные32 cases и явные
native-ограничения точности. Actual lossy/rejected Java output сохранён как доказательство.

