# Покрытие символов Java

[English](COVERAGE.md). Английская версия является нормативной.

[156 входов](inputs.tsv) дают [3465 наблюдений reader](expected/symbols.tsv): 693 основных чтения
(исходные векторы и каждый строгий префикс), после каждого — repeat, MRU-repeat, reset/repeat и
контрольный ABC в отдельных буферах. Это состояние примитива, не восстановление parser.
19 канонических входов writer также дают 114 последовательных чтений с флагами 0/1/5/5/0/-1.
Null на входе writer отвергается до записи; декодирование null cipher не означает пустой символ.

| Область | Сценарии/доказательства |
| --- | --- |
| Формы writer | empty, a, abc, abcd, abcde, abcdef, slash7, dollar7: ширины zero/15/20/30/35 бит; некодируемые seven/eight; ascii, quote, bmp, pair, high, low, nul, len63/len64 |
| Дополнительное принятие | zero15/20/30/35, penta-nul, dangling, noncipher35, UTF/CESU empty/overlong/pair и four-byte BMP, overlong-length |
| Ошибки/позиции | все reserved leads c0..df и fa/fb; отрицательные/выше-INT_MAX длины; неопределённый repeat; неверные UTF tails/leads/range; отказ FD для supplementary; выход FC за тело; каждый строгий префикс |
| Состояние | fresh и seed ABC/MRU=5; повторные/новые/нулевые/отрицательные/cross-width флаги; дубли префиксов; repeat/MRU после ошибок; явный reset; канонические повторы символов |
| Production | реальные байты composer и callbacks parser: ABC,ABC,XYZ,XYZ / XYZ,XYZ,ABC; второе сообщение начинается с 633a вместо ff |

Порядок колонок read: id, phase, seed bytes, input bytes, Java outcome, consumed position, cipher,
raw string UTF-16, decoded UTF-16, flags, Java flag-byte count. '-' означает пустое, 'null' — null.
Строки writer сохраняют исходные units/cipher/bytes. Нормализуются только окончания строк;
ошибки сохраняют Java classes/messages. Raw captures и полная provenance — в [валидации](VALIDATION.ru.md).

Связь с исходниками и проекция C++ — в [SYMBOLS](../../../../spec/encoding/SYMBOLS.ru.md).
Закреплённый Java игнорирует reader options; production parser внутри использует SUPPORTED_SET,
writer/primitive probes — EMPTY_SET. Ненулевые маски C++ явно не поддерживаются. Не покрыты negotiation,
resolver, огромные аллокации, другие input subclasses, полный перебор Unicode, семантика event/time,
PART, транспорт и запуск upstream test suite. Малые пограничные представители не означают полный
перебор penta и комбинаций flags. Исторические fixtures и lock сохранены.
