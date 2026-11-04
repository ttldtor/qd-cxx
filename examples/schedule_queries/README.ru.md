# Пример запросов расписания

[English version](README.md) является нормативной.

Задайте `ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON`, `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON` и соберите
`qdcxx_schedule_queries_example`. Runtime SDK и загрузки не нужны. Пример явно инициализирует
контролируемые данные, запрашивает XNYS за2024-11-04, печатает границы Day и упорядоченные Session,
затем завершается без workers и ручного закрытия окон.

```powershell
.\build\examples\schedule_queries\qdcxx-schedule-queries.exe --help-ru
.\build\examples\schedule_queries\qdcxx-schedule-queries.exe --run --data C:\sdk\share\qdcxx\time-zone-data\tzdb-2026f
```

Linux: `./build/examples/schedule_queries/qdcxx-schedule-queries --run --data /opt/sdk/share/qdcxx/time-zone-data/tzdb-2026f`.
Без аргументов, с `--help` или `--help-ru` файловой/native работы нет. Рабочий синтаксис только
`--run --data АБСОЛЮТНЫЙ_КАТАЛОГ`. Дата, площадка и raw-reference instant — детерминированные входы
примера. Версионное установленное поддерево данных можно скопировать с приложением либо перенести
SDK prefix целиком; передайте новый абсолютный каталог явно. Windows принимает native wide paths.
Backend проверяет точные байты, а не произвольные локальные timezone-файлы.
Коды:0 справка/успех,1 ошибка данных/запроса,2 неверные параметры.

Отдельный installed consumer использует `find_package(qdcxx REQUIRED COMPONENTS schedule time_zone_data_backend)`
и `target_link_libraries(app PRIVATE qdcxx_schedule)`. Тот же `main.cpp` компилируется с C++23;
MSVC также нужен `/utf-8`. Приватные headers, Date-типы и source-tree paths не нужны.
[Владение/диапазоны](../../docs/SCHEDULE-QUERIES.ru.md), [зависимость и перенос данных](../../docs/TIME-ZONE-DATA.ru.md).
