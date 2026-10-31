# Захваты нативного writer подписок

[English version](README.md). Английская версия нормативная.

Индексированное expected-дерево содержит 317 байтов нативного потока, фактические наблюдения Java,
реальные чтения времени, source provenance и SHA-256 manifest. Его сформировал
[SubscriptionWriter](../../../../spec/subscriptions/WRITER.ru.md) на детерминированной нативной
фикстуре и разобрал [производственный verifier](../../../../tools/upstream/subscription-writer-oracle/README.ru.md).
Pin: исходники QD 3.355 на 0ddc0a09779fb61d74cc0b37c6449ff47dcb715f.
Handshake с версией протокола не выдаётся; source pin не подменяет runtime metadata.

| Запись | Native local ID | Outgoing sender ID | Java receiver ID | Поля фикстуры |
| --- | --- | --- | --- | --- |
| Alpha | 0 | 41 | 2 | x compact type 8 |
| Beta | 1 | 3 | 1 | x/y compact type 8, выходной порядок явный |
| Indexed | 2 | 7 | 0 | явный timed hi/lo/x compact type 8 |

Наблюдались 42 tuples: по 7 для 11/12/16/17/21/22. Символы включают empty, встроенный NUL,
непарные UTF-16, AB* и A?, с повторами внутри сообщения. HISTORY_ADD даёт 7 настоящих
readSubscriptionTime calls (MIN/MAX/-1/0/повторные границы). HISTORY_REMOVE чтений не даёт;
его callback raw time равен 0. Это знаковые raw record-time, не бизнес-миллисекунды.

Нативная фикстура пишет ожидаемые значения входов в expected.tsv и expected-time-reads.tsv своей
сборки; Java callbacks должны совпасть точно. Ожидания не извлекаются декодированием байтов
нативной реализации. Отдельные golden byte tests проверяют framing/description/symbol/bound syntax.
Subscription callback этим не квалифицирует все преобразования полей данных.

MSVC Windows и GCC Linux выдали одинаковые нативные байты; оба потока дали одинаковые пять файлов
на одном Windows JDK 8 provider profile. Выполнение Java на Linux не выводится.
Source/tool digests — часть точного профиля воспроизводимости, отдельно от приватных квитанций
compiled classes/JVM. Regeneration никогда автоматически не переписывает эталоны Git.
[Проверки](../../../conformance/subscriptions/WRITER-VALIDATION.ru.md) фиксируют выполненные ограничения.
