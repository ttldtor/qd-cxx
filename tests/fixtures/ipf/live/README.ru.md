# Java-наблюдения connection

[English version](README.md) является нормативной.

ConnectionOracle.java фиксирует55 конечных наблюдений закреплённого production QD/JDK8.
connection-provenance.json содержит helper/capture/source hashes и runtime. Private
Handler.checkAndDownload reflection instrumentation проверяет pending carryover,
поколения, capability/date/conditional headers и период; отдельный actual public
start→state-callback close выполняет join production Handler. Instrumentation не
выдаётся за public lifecycle proof.

Команды компиляции/запуска приведены в английской версии. outputClasses — новый
каталог caller, qdClasspath — явный существующий classpath module target/classes и
jsr305/slf4j/log4j. Production/dependency не собираются/не загружаются. Oracle требует
JDK8; formatter использует отдельный JDK21.

Без аргументов, --help и --help-ru не нужны рабочие входы и не создают sockets/threads.
--run использует только собственные loopback sockets, максимум32768bytes header,
конечные socket deadlines и join своих server/Handler threads. Ошибочные параметры/
exceptions возвращают nonzero; exit0 означает завершение наблюдений. В stdout идут
TSV и обычные QD logs: полный лог сохраняют отдельно, для capture выбирают строки с
табуляцией. Helper сам не пишет source/capture files. Captured values нельзя заменять
предполагаемыми C++ outcomes.

Проверяются Unicode integer digits, signed64/inf/saturation/ошибки периода, unflushed
carryover после ошибки, flush-before-error,304, равная дата, exact live capability,
live EOF, одно поколение после COMPLETE и однократный public lifecycle. См.
[контракт](../../../../spec/ipf/CONNECTION.ru.md), [адаптацию владения/metadata](../../../../docs/decisions/ADR-0009-live-ipf-connection-ownership.ru.md).

## Oracle ожидания completion

CompletionWaitOracle.java наблюдает23 outcomes production waitUntilCompleted закреплённого QD/JDK8:
terminal-before-wait, zero/negative timeout, wait до start, pending close/interrupt, восстановление
interrupt flag, sticky completion после close/interrupt и worker callback waits. Используется
один собственный regular temporary IPF source, без сети. Private reflection только наблюдает
регистрацию public waiter и сохраняет actual handler threads для cleanup; production wait не
переписывается. Все собственные threads получают interrupt/join с конечными cleanup bounds.

Используйте прежние явные existing classpath/outputClasses правила, заменив имя source/class
на CompletionWaitOracle.java/CompletionWaitOracle. Без аргументов, --help и --help-ru source/threads
не создаются. --run создаёт/удаляет temp file, выводит TSV и обычные QD logs; exit0 — полные
наблюдения, exit1 — exception, exit2 — неверные аргументы. Сохраняйте full logs отдельно и выбирайте
только TSV observations в completion-wait-outcomes.tsv. completion-wait-provenance.json связывает
helper/capture/pinned source/observed classes. Адаптация C++ self-wait/stop-token описана ADR-0009;
Java rows не заменяются C++ expectations. Прежние ConnectionOracle captures/provenance не меняются.

## Shutdown oracle

ShutdownOracle.java фиксирует12 actual pinned QD/JDK8 observations в shutdown-outcomes.tsv:
remove возвращается при активном selected callback; удалённый listener не получает следующие
states; исключения state/collector callbacks изолируются; другие listeners получают completion/
profiles; реальные connection handlers завершаются после close. Collector использует явный direct
executor; это не обещание parity с произвольным asynchronous executor.

Компиляция/запуск как выше с ShutdownOracle.java и ShutdownOracle. Без аргументов, --help и
--help-ru source/threads не создаются. --run использует один собственный временный regular IPF,
без сети, callback/wait до трёх секунд и handler joins до четырёх секунд. Файл удаляется, собственные
потоки завершаются при обычном/error path. Exit0: observations complete; exit1: exception;
exit2: неверные аргументы. Reflection лишь удерживает actual public-start handlers для cleanup.
Обычные QD exception logs ожидаемы; для capture отбираются только anchored TSV rows, полные logs
сохраняются отдельно. shutdown-provenance.json связывает helper/capture/pinned sources/observed classes.

Строки описывают обычное Java behavior, не его monitor ownership или C++ API spelling.
См. ADR-0009 и публичный shutdown contract. C++ sample/loopback regressions отдельно проверяют
interrupted HTTP, request-only stop, owning prefixes и joined-worker notification barriers.
