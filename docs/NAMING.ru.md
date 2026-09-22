# Идентичность проекта и миграция CMake

[English version](NAMING.md). Английский документ является основным.

Владелец выбрал `qdcxx` в области `ttldtor.org`. Эта миграция меняет только C++ namespaces,
CMake project/targets и собственные cache-переменные/опции. Публичные include-пути остаются
`qd/...`; executable — `qd-qtp-inspect`, с прежними идентификаторами и байтами TSV/human.
Переадресующие namespace aliases и дубли старых targets/options не добавляются.

| Область | Актуальное имя |
| --- | --- |
| Корень namespace | `org::ttldtor::qdcxx` |
| Компоненты | `org::ttldtor::qdcxx::core`, `::encoding`, `::protocol`, `::inspect` |
| CMake project | `qdcxx` |
| Библиотеки | `qdcxx_core`, `qdcxx_encoding`, `qdcxx_protocol` |
| Библиотека утилиты / executable target | `qdcxx_inspect_tool` / `qdcxx_qtp_inspect` |
| Имя executable | `qd-qtp-inspect` (сохранено через OUTPUT_NAME) |
| Таргеты тестовых executable | `qdcxx_compact_tests`, `qdcxx_frame_tests`, `qdcxx_description_tests`, `qdcxx_binding_tests`, `qdcxx_symbol_tests`, `qdcxx_stream_data_tests`, `qdcxx_offline_io_tests`, `qdcxx_human_io_tests` |
| Опция утилиты | `ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT` (по умолчанию OFF) |
| Fixture cache-переменные | `ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES`, `ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS`, `ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES`, `ORG_TTLDTOR_QDCXX_BINDING_FIXTURES`, `ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES`, `ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES` |

Имена CTest-случаев и исходных файлов неизменны. Стандартные имена и зависимости вроде
`BUILD_TESTING`, `CMAKE_*`, `Python3_*` и `doctest::doctest` не являются префиксами проекта.
Будущие собственные targets используют `qdcxx_`; публичные настройки/макросы — `ORG_TTLDTOR_QDCXX_`.
AGENTS.md и [контракт C++-стиля](CPP-STYLE.ru.md) закрепляют постоянное правило.

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
