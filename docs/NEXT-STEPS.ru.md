# Ограниченное продолжение после QDCXX-007

[English version](NEXT-STEPS.md). Английский документ является основным.

Все строки — **предлагаемый backlog**, а не назначенные задачи или разрешение реализации. [ADR](decisions/ADR-0001-normalized-model-boundaries.ru.md) остаётся proposed. Границы доказательств описаны в [требованиях](../spec/schema/NORMALIZED-MODEL.ru.md).

| Работа | Зависимости / недостающие доказательства | Проверяемый результат |
| --- | --- | --- |
| B01 — Review compatibility profile и владельцев артефактов | Решения владельца по provenance, copied main alias, structured errors и JSON | Принятый или пересмотренный ADR; явные deviations и exclusions. Реализация не нужна. |
| B02 — Целевые schema probes | Существующий schema harness; перед добавлением alias overlays измерить collision/removal, defaulted attributes и copy docs/regionals/disabled; перед исправлением main alias измерить known mapping/property/wire effects. SC-01/02/03/04/07 сохраняют отдельные границы. | Точные stimuli, две репродукции, negative/positive verify, парные coverage/validation; сохранение failures и stages. Только пробы для следующего importer subset, а не все семейства. |
| B03 — Минимальный importer contract, затем отдельно разрешённый importer | B01; B02 для добавляемых конструкций. Начать с 17 существующих schema subcases, явно отклоняя/диагностируя неподдерживаемый scope. Custom types/generators/imports требуют дополнительных доказательств до поддержки. | Versioned interchange specification, original-input/profile provenance, предлагаемые normalized projections, проверенные относительно неизменных Java expectations; отдельная structured diagnostics. Код требует новой утверждённой задачи. |
| B04 — Primitive encoding evidence и contract | Существующий encoding source inventory; P01 недостаточно. Проверить compact width/sign boundaries, truncation/overlong forms, UTF null/empty/Unicode и ограниченные malformed lengths. | Byte vectors со ссылками на исходники и acceptance/error/partial-input rules; без выбора библиотеки. Основа для будущего явно разрешённого первого encoding component. |
| B05 — Расширение static protocol adaptation | B01/B04; измерить LONG/WIDE_DECIMAL tails и исключение missing time-index из wire; requested y,x order, P06 numeric/object mismatches и supported skip paths. | Incoming/outgoing mappings и точные bytes/values, без общего обещания defaults/coercion. Существующие P03/P04/P05 остаются regression subset. |
| B06 — Session/layout lifetime contract | Перед любым обещанием dynamic layout/session reuse: requests до/после первого использования writer, re-description и reset; P09 negotiation и соответствующие P12 probes. | Измеренное cache/lifetime/error behavior и отдельное решение совместимости. Без автоматического сохранения source-observed cache retention. |

Полный dxFeed provider/property parity, generated/regional schemas, mapping getters, PART/auth/sockets/timing и широкие rebuild sequences могут остаться вне initial profile. Им нужны отдельные ограниченные задачи, а не незаметное расширение B03 или B05. qd-core остаётся независимым от transport; backlog не добавляет C++, importer, build system или dependencies.
