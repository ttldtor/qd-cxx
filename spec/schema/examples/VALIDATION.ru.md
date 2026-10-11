# Выполненная валидация контракта QDCXX-008

[English version](VALIDATION.md). Английский документ является основным.

Выполнено 2026-09-21 в Windows/PowerShell с установленным Python 3.11.1 (только стандартная библиотека). [Контракт](../JSON-CONTRACT.ru.md), [матрица случаев](COVERAGE.ru.md), [проверка](../../../tests/conformance/check_json_contract.py).

## Выполненные проверки

Из корня репозитория:

```powershell
python tests/conformance/check_json_contract.py
git diff --exit-code -- tests/fixtures upstream.lock
git diff --check
```

Проверка прошла для всех 17 примеров: 13 успешных схем и четыре отказа (пять записей диагностики). Скрипт строго разбирает JSON с отклонением повторяющихся ключей/чисел вне JSON, проверяет format/profile, обязательные/неизвестные поля, целочисленные диапазоны, ID, имена, ссылки, один head на логическое поле, порядок Java category/ordinal, принадлежность LONG/WIDE_DECIMAL tails, index positions/placeholders и lookup scopes. С неизменными TSV сверяются порядок входов, SHA-256 исходных входов, runtime outcomes, порядок выдачи generation, порядок/имена/полные type ID активных полей модели, все имена/типы/количества Java slots и все зафиксированные положительные/отрицательные probes поиска записей/полей. Диагностические попытки, граница recognition, спроектированные codes/types/subjects и ссылки на доказательства соответствуют четырём зафиксированным отказам. Проверены порядок ссылок metadata/environment и наличие файлов.

Пройдены 13 отрицательных проверок в памяти: неизвестный format, неизвестный profile, неизвестное вложенное поле, boolean ID, null вместо коллекции fields, несуществующий владелец поля, неправильный владелец tail, обратный порядок слотов, повторный lookup, неправильная цель индекса, records при отказе, неправильная стадия recognition и повторный ключ объекта JSON. Они не меняют примеры или goldens. Скрипт проверяет корпус примеров и выбранные мутации; это не полный валидатор произвольных документов. В частности, extension payloads, все граничные случаи Unicode, произвольные некорректные структуры и resource limits не проверены исчерпывающе. Формальная JSON Schema не предоставлена.

Команда diff для fixtures/upstream завершилась без различий; schema/protocol inputs, expectations и captures неизменны. `git diff --check` прошла. Локальный аудит Markdown-ссылок во всех изменённых/новых Markdown-файлах проверил относительные пути файлов/каталогов и взаимные ссылки EN/RU; все разрешились. Внешние URL не загружались, фрагменты заголовков не проверялись.

## Ручная сверка проекций и языковых версий

Неизменный [корпус TSV](../../../tests/fixtures/schema/expected/) сверен с [матрицей из 17 строк](COVERAGE.ru.md) и подготовленным JSON. S04 сохраняет порядок активных объявлений отдельно от порядка int/obj и исключает HiddenRecord/Hidden из локального артефакта. S05 различает реальные index heads, absent/disabled placeholders и both-disabled failure; имена, не объявленные индексами, могут оставаться обычными полями. S15-slots содержит шесть логических значений, шесть int slots и два obj slots, tails принадлежат Long и Wide. S07 раздельно сохраняет semantic/effective/full/local/property names и collapsed lookup keys. S08-before/after сохраняют порядок входов и различия снимков; S08-main-alias сохраняет Copy.First.Name, тогда как исходник/доказательство сохраняет explicit Chosen.Name main metadata. Effective name не вычисляется заново из этого объявления.

Пример no-URLs сохраняет две разные попытки загрузчиков, в отличие от успешного пустого XML. BYTE/SHORT сохраняют успешное распознавание модели и отказ построения с утверждённой структурированной диагностикой; all-disabled indexes сохраняет отдельный код и доказательство SchemeException. Точные exception messages/classes/origins остаются в TSV по ссылкам, а не в вымышленных JSON-наблюдениях. Метаданные профиля и [inventory окружения](../../../tests/fixtures/schema/captures/final-a/environment.tsv) сохраняют закреплённый контекст providers.

Проверено соответствие EN/RU контракта, coverage, validation и обновлений task/state/backlog по полям, ограничениям, выводам, доказательствам, исключениям и различию выполненного и отложенного. Обе версии ссылаются на одни и те же JSON, XML, TSV и capture metadata. Прежние утверждения истории задач сохраняют историческую область действия; текущие заметки об утверждении/обновлении заменяют pending-метки решений.

## Ограничения

Это спроектированные проекции, подготовленные по существующим наблюдениям, а не новый Java output. Ручная сверка и сравнения скриптом — проверки документов/доказательств, а не importer или C++ conformance. Не выполнялись запуск Java, importer, C++, build configuration, выбор зависимостей, canonical identity/hash, сериализация протокола или работа QDCXX-009. SHA-256 входов идентифицирует только исходные байты. Архитектура всей сессии или трёх форматов не требуется.

Core использует только разрешённое локальное содержимое; аудит provenance отделён. Семантика программного построения, расширенные XML/provider profiles, alias collisions/removal, generators/imports, conversions и dynamic protocol/reset behavior отложены. [P05](../../protocol/CAPTURES.ru.md) обосновывает исключение requests/selection из local identity; он не измеряет LONG/time-index wire projection или wire-эффекты XML copy. Эти probes и расширенная валидация некорректных входов остаются дальнейшей работой.
