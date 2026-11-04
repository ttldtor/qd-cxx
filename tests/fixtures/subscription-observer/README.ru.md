# Закреплённые доказательства наблюдения подписки

[English version](README.md). Английская версия основная.

[oracle.tsv](oracle.tsv) сохраняет 49 наблюдений реальных production subscription classes Java QD 3.355; [provenance.json](provenance.json) связывает reference commit, JDK/tool/harness/source hashes и неизменяемый production-class inventory. Воспроизведение и ограниченная capture/verify справка описаны в [oracle runner](../../../tools/upstream/subscription-observer-oracle/README.ru.md).

Сценарии покрывают ordinary initial/add/replace/remove/clear/close, неизменяемые type/symbol views, равенство indexed-source identity, явный ALL, self-removal с recursive CLOSED и exception fanout/propagation. Сортировка представляет только содержимое наборов; строки сохраняют порядок operations/callbacks. UTC и Honolulu совпадают. Сетевой источник не используется.

Native tests напрямую сравнивают ordinary membership phases. Raw Java monitor ownership, reentry depth2 и propagated exceptions сохранены как доказательства; они не переписываются под [native callback profile](../../../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.ru.md). Fixture не заявляет typed history или будущие event families и не заменяет отдельные decorated-identity ADR и selector fixture.
