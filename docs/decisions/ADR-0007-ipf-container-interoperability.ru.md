# ADR-0007: Совместимость контейнеров IPF

[English](ADR-0007-ipf-container-interoperability.md). Английская версия нормативная.

Статус: выбрано техническое решение для ограниченной реализации IPF sources. Квалификация
реализации, приёмка проекта и выпуск учитываются отдельно.

## Контекст и решение

Pinned QD выбирает gzip/ZIP по содержимому, пропускает ZIP directories и читает первый файл.
Фактический JDK8 имеет дефекты descriptors, отклоняющие корректные ZIP. Согласно
[PKWARE APPNOTE6.3.10](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT), разделы4.3.9.1–4.3.9.3
и4.5.3, descriptors могут не иметь signature, ZIP64 допустим для малых файлов, наличие ZIP64 extra
выбирает восьмибайтовые размеры descriptor, а local extra включает оба размера.

Сохраняем выбор первого файла QD и обычное поведение gzip, но поддерживаем корректные незашифрованные
stored/deflate ZIP forms в конечных лимитах. Дефект pushback/lookahead JDK8 не воспроизводится;
размер малого payload не подменяет явный выбор ZIP64. Исходные Java captures остаются неизменными.

| Fixture | Фактический JDK8/QD | Контракт C++ |
| --- | --- | --- |
| zip-descriptor-nosignature | EOF при точном конце descriptor | Принимает unsigned ZIP32 без лишнего чтения хвоста |
| zip-directory-nosignature-first | Не находит файл из-за pushback | Пропускает каталог и читает следующий первый файл |
| zip64-descriptor64-small | Ошибка размера у корректного ZIP64 | Принимает64-битные размеры, выбранные extra |
| zip64-descriptor64-nosignature-small | Ошибка размера | Принимает unsigned64-битный descriptor |
| zip64-directory-nosignature-first | Ошибка размера | Пропускает каталог и читает следующий файл |
| zip64-descriptor-small | Принимает32-битный descriptor при ZIP64 extra | Отклоняет несогласованную ширину |

Эти различия не объявляются Java parity. Ошибки metadata/CRC/фактических размеров дают явный
container failure. После выбранного файла последующие entries/central directory не проверяются;
ограничения источника и HTTP/native framing сохраняются. Извлечения файлов и интерпретации путей нет.

Content-Encoding:gzip выбирает тот же единственный gzip слой. Без него/при identity используется
magic. Результат идёт прямо в IPF parser; рекурсивного декодирования и новых codings negotiation нет.

## Альтернативы

Точное повторение дефектов JDK8 отклоняло бы корректные архивы. Строгий single-file ZIP32 профиль
tape tool исключает нужные ZIP64/descriptors. Проверка всего архива меняет QD first-file семантику.
Эти альтернативы не выбраны.

## Последствия и миграция

Корректные unsigned/малые ZIP64 producers работают без смены формата. Producer с ZIP64 extra
и32-битным descriptor должен выпустить согласованный64-битный descriptor либо обычный ZIP32.
Ранее принимаемые Java некорректные данные здесь дают явную ошибку.

Reader резервирует фиксированный decoder и конечный inflater allowance, в том числе для plain
чтения через compression-capable facade. Exact/one-below проверки измеряют фактический layout,
старые ресурсные goldens не ослабляются. После добавления public limits/progress/error требуется
пересборка статических consumers. Независимый сырой UTF-8/IPF parser не меняется.

[Контракт](../../spec/ipf/COMPRESSION.ru.md) и [oracle fixtures](../../tests/fixtures/ipf/compression/README.md)
фиксируют происхождение и границы. Проверки охватывают signed/unsigned ZIP32/ZIP64, directories,
обычные полные ZIP, optional headers/members gzip, corruption/truncation, expansion и точные лимиты.
Controlled local/HTTP/HTTPS tests проверяют bad trailer после COMPLETE, first cause, cache-only
prefix drain и request-only отмену из другого потока. Нативная квалификация обязательна.
