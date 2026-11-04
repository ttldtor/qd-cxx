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
