# Glossary fixtures

[English](README.md). Английский текст является нормативным.

`oracle.tsv` — точный вывод production Java. UTF-16 units записаны четырьмя lowercase hex
цифрами, `-` означает пустой текст, `~` — null. D rows — metadata каждого достижимого
атрибута и полный порядок значений; C/I — CFI text/packed factories и decipher metadata;
A/Q — AU text parser и unchecked static lookup; M — map format и результат разбора SPC.
Binary64 bits записаны lowercase hex без padding. Ошибки — production message с префиксом `!`.

[Oracle](../../../tools/upstream/glossary-oracle/README.ru.md) воспроизводит/проверяет файл.
[Provenance](provenance.json) содержит pin, hashes исходников/classes, JDK8 и harness/launcher.
Native test отдельно проверяет C++ пределы, lifetime и allocation failures; это не Java evidence.
