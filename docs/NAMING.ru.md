# Идентичность проекта и миграция CMake

[English version](NAMING.md). Английский документ является основным.

Владелец выбрал `qdcxx` в области `ttldtor.org`. Политика именования охватывает C++ namespaces,
CMake project/targets и собственные cache-переменные/опции. Публичные include-пути остаются
`qd/...`; executable теперь — `qdcxx-qtp-inspect`, с прежними идентификаторами и байтами TSV/human.
Переадресующие namespace aliases и дубли старых targets/options не добавляются.

| Область | Актуальное имя |
| --- | --- |
| Корень namespace | `org::ttldtor::qdcxx` |
| Компоненты | `org::ttldtor::qdcxx::core`, `::encoding`, `::protocol`, `::inspect` |
| CMake project | `qdcxx` |
| Библиотеки | `qdcxx_core`, `qdcxx_encoding`, `qdcxx_protocol` |
| Библиотека утилиты / executable target | `qdcxx_inspect_tool` / `qdcxx_qtp_inspect` |
| Имя executable | `qdcxx-qtp-inspect` (выбрано через OUTPUT_NAME) |
| Таргеты тестовых executable | `qdcxx_compact_tests`, `qdcxx_frame_tests`, `qdcxx_description_tests`, `qdcxx_binding_tests`, `qdcxx_symbol_tests`, `qdcxx_stream_data_tests`, `qdcxx_offline_io_tests`, `qdcxx_human_io_tests` |
| Опция утилиты | `ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT` (по умолчанию OFF) |
| Fixture cache-переменные | `ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES`, `ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS`, `ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES`, `ORG_TTLDTOR_QDCXX_BINDING_FIXTURES`, `ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES` |

Имена CTest-случаев и исходных файлов неизменны. Стандартные имена и зависимости вроде
`BUILD_TESTING`, `CMAKE_*`, `Python3_*` и `doctest::doctest` не являются префиксами проекта.
Будущие собственные targets используют `qdcxx_`; публичные настройки/макросы — `ORG_TTLDTOR_QDCXX_`.
[Контракт C++-стиля](CPP-STYLE.ru.md) также закрепляет это правило.

Это несовместимое изменение source/build-интерфейса: обновите namespace, target/cache-имена
у потребителей и пересоберите. Старые QD_* cache-записи не управляют новыми опциями; используйте
новый каталог сборки либо удалите устаревшие записи и явно передайте новую опцию. Совместимость
source/binary ABI переименованных C++-символов не заявляется. Include-пути менять не нужно.

```cpp
#include "qd/core/local_schema.hpp"
// Путь заголовка намеренно оставлен прежним.
using org::ttldtor::qdcxx::core::LocalSchema;
```

```powershell
cmake -S . -B .work/qdcxx -DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=ON
cmake --build .work/qdcxx --config Release
ctest --test-dir .work/qdcxx -C Release --output-on-failure
```

Исторический текст задач, captures, логи валидации и снимки source hashes сохраняют исходные
имена и команды: они описывают прежние ревизии. Актуальные примеры сборки используют новые имена.
Java QD/package names, семантика протокола/схемы и fixtures неизменны. Каталог репозитория
не переименовывается; поддержка tape/transport не добавляется. [Новая валидация](../tests/conformance/naming/VALIDATION.ru.md)
фиксирует проверки переименованных исходников отдельно от исторических доказательств.

## Имена исполняемых файлов утилит

Владелец расширил миграцию на имена утилит: `qdcxx-qtp-inspect` заменяет `qd-qtp-inspect`.
Для будущих пользовательских утилит используйте `qdcxx-` и слова через дефис; CMake-таргеты
сохраняют вид `qdcxx_*`, имя файла задаётся OUTPUT_NAME. Внутренние тестовые binaries сохраняют имена qdcxx_ targets.
Обновите команды/ярлыки и пересоберите. Старое имя как alias binary не создаётся. В повторно используемых
каталогах могут остаться старые binaries; их наличие не означает пересборку. Заголовки TSV/human
версии 1 по-прежнему содержат `qd-qtp-inspect` как закреплённый идентификатор формата для сохранения точных байтов.
Help показывает новое имя executable. [Валидация](../tests/conformance/executable-naming/VALIDATION.ru.md).
