# Валидация перехода на Google Java Style

[English](VALIDATION.md). Английская версия является основной.

Выполнено 2026-09-21 по запросу владельца о стиле после QDCXX-009. Проверены официальные
[руководство](https://google.github.io/styleguide/javaguide.html) и
[релиз форматтера](https://github.com/google/google-java-format/releases/tag/v1.36.1).
Jar скачан по официальному URL release asset; SHA-256 закреплён в
[lock](google-java-format.lock.json). Jar форматтера и кеш не добавлены в репозиторий.

## Проверки форматирования

- Все три Java harness репозитория проходят google-java-format 1.36.1 в стандартном режиме Google.
  Wrapper проверен с Microsoft JDK 21.0.4; проверки QD по-прежнему используют Zulu JDK 8.
- Вместе с автоматическим форматированием добавлены именованные packages, явные imports и
  управляющие блоки. SchemaOracle и EncodingOracle получили полные имена в `qdcxx.oracle`;
  обновлены launchers и вызовы инспектора окружения. ProtocolOracle сохраняет доступ к upstream
  package. Все три компилируются JDK 8 относительно classpath исходной сборки QD.
- Временный неотформатированный FormattingProbe.java обнаружен Check с exit 1. Его хеш и хеши
  трёх настоящих Java-файлов остались неизменными. Затем probe удалён, baseline Check прошёл.
  JDK 8 отвергнут с документированной диагностикой JDK форматтера.
  format-check.json сохраняет результаты и хеши Java-исходников.
- Format с последующим Check подтверждает устойчивый результат форматтера. Wrapper проверяет
  SHA-256 закреплённого jar при каждом запуске. Это проверка форматирования, а не полная проверка
  стиля или корректности; правила вне покрытия форматтера по-прежнему требуют review.

## Регрессии oracle и provenance

Все шесть штатных чистых сборок шли последовательно на общем закреплённом checkout. Фактические
аргументы процессов, рабочие каталоги, upstream, хеши инструментов/входов/байткода/classpath и
environment/build logs сохранены в метаданных. Семантика исходников upstream и эталоны не менялись.

| Процесс | Результат и доказательства |
| --- | --- |
| Schema Verify | 17 случаев проходят; метаданные, результат. |
| Protocol Verify | 9 случаев / 18 файлов проходят; метаданные, результат. |
| Encoding capture A/B | Независимые чистые сборки воспроизводят все три эталона; [A](../../tests/fixtures/encoding/captures/capture-a/metadata.json), [B](../../tests/fixtures/encoding/captures/capture-b/metadata.json). |
| Encoding отрицательный/baseline Verify | Временная мутация байта I-zero отвергнута; настоящий baseline проходит; оба набора эталонов неизменны; [доказательство](../../tests/fixtures/encoding/captures/verify-proof.json). |
| Аудит доказательств | `python tests/conformance/check_encoding_evidence.py` проходит проверку текущего provenance кода, всех сохранённых encoding observations и хешей прежних schema/protocol goldens. |

Schema/protocol используют `.work/google-style` в каталогах соответствующих harness. Прежние
capture задач остаются неизменными историческими доказательствами для прежних хешей исходников;
новые метаданные выше идентифицируют текущий код. Encoding использует `.work/google-a`,
`.work/google-b` и `.work/google-verify`; текущие evidence обновлены из этих фактических запусков.
Хеши исторических метаданных не подменялись вручную.
oracle-check.json сохраняет сравнения normalized outputs и текущие
хеши исходников.

README и validation обновлены парно EN/RU; проверены относительные файловые ссылки и
`git diff --check`. C++, importer, production dependency, глобальный Git hook и настройки IDE не добавлены.
