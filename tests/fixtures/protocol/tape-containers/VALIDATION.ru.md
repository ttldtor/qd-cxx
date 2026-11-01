# Проверка компонента контейнеров

[English version](VALIDATION.md). Английская версия является нормативной.

Статус: ограниченный компонент, optional build integration и независимое scoped review проверены
в указанных ниже профилях. Operational CLI composition и итоговая catalogue acceptance остаются
отдельными; это не итоговый layered COMPLETE и не release acceptance. Область задают
[контракт](../../../../spec/protocol/TAPE-CONTAINERS.ru.md) и
[production captures](CAPTURES.ru.md).

Проверки 2026-10-30:

| Проверка | Фактический профиль/результат |
| --- | --- |
| Native focused CTest | Windows MSVC 19.42 Release, static zlib 1.3.2: 14 cases,130644 assertions, ошибок не обнаружено |
| Native focused CTest | Linux GCC 13.3 Debug ASan/UBSan, включая zlib: 14 cases,130639 assertions, ошибок не обнаружено |
| C++ formatting | Uncrustify 0.83.0 затем clang-format 20.1.8 Format и Check трёх новых sources |
| Документация | Strict Doxygen нового header/cpp, ноль warnings после описания private ownership/state |
| Java production | Оба pin: Capture, повторный Capture и Verify; 14-root inventories неизменны |
| Launcher | PowerShell syntax, отдельный manual CMake help gate, побайтовое равенство noargs/help, ненулевой exit неверных аргументов |
| Private dependency gate | Windows/Linux OFF и ON без consumer игнорируют poisoned dependency paths, не включают C/target; ON проверяет archive/version/license и собирается; bad hash/source override дают отказ |
| Полный configured project CTest | Windows MSVC Release63/63; Linux GCC Debug ASan/UBSan63/63, включая инструментированный C backend сжатия |
| Production-only builds | Три профиля каждой OS: minimal, tools/replay с JSON OFF, tools/replay с JSON ON; compression OFF |
| Actual root dependency profiles | Свежие OFF/consumer, ON/no-consumer и ON/production-consumer builds обеих OS; wrong archive/source override отвергнуты по ожидаемым причинам без invalid acquisition |
| Независимое component review | Итоговые native ownership/state/resource/frame contracts; копии Windows/Linux14-case binaries и отдельные EOCD classification/metadata-allocation probes |

Private gate проверял dependency block изолированно; последующие свежие root-build profiles проверили
интегрированный block. На Windows CXX configure также включает обычный RC; C отсутствует в OFF/
no-consumer states. Требование C++23 header является PUBLIC и передаётся native test target.
Manual PowerShell help gate отделён от обычного configure/build/CTest.

Native cases охватывают production NONE/GZIP/ZIP equality, existing standard-Quote preservation,
одно-байтовые physical/output chunks, empty/concatenated members, optional gzip headers/FHCRC,
каждый truncated prefix небольших gzip/zip inputs, CRC/ISIZE, поздние source failures, descriptor/
directory/EOCD closure, несколько файлов, каталоги с payload, ZIP64/encryption/method refusals и
trailing bytes. Нулевые, exact/excess и SIZE_MAX настройки проверяют physical/inflated/header/work/
call/entry/allocation/ratio bounds без allocation максимального count. Actual source-operation
counters подтверждают отсутствие read после terminal outcomes/destruction; одни byte counters
не заметили бы zero-byte probes. Отдельно проверены finish и empty-span contracts.

Physical read-ahead отделён от logical progress даже при запросе одного logical byte. Actual
source-call counters проверяют lifetime/stop независимо от числа выданных bytes. Sanitizer errors
в указанных проверках не обнаружены.

Developer catalog и отдельный manual oracle help gate зарегистрированы; native configure/build/
CTest сохраняет interpreter-free contract. Итоговая CLI/parser/companion/split/close/flush composition
и full catalogue acceptance принадлежат отдельной интеграции. Установленный runtime,
private-market tape и универсальная ZIP acceptance не заявляются.
