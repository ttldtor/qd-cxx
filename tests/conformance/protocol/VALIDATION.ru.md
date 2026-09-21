# Исполненная C++ валидация QDCXX-012

[English version](VALIDATION.md). Английская версия является нормативной.

Выполнено 2026-09-21 UTC после [этапа доказательств Java P02](../../fixtures/protocol/p02/VALIDATION.ru.md).
[Покрытие](COVERAGE.ru.md) фиксирует выборку и ограничения. Production-зависимости не добавлены;
qd_protocol зависит внутрь от qd_encoding, doctest остаётся только тестовой зависимостью.

## Результаты

| Конфигурация | Результат и сохранённые UTC-доказательства |
| --- | --- |
| MSVC 19.42.34444.0, x64, VS 2022 / SDK 10.0.26100.0, Debug | Оба CTest suites проходят; build, test. |
| Тот же MSVC, Release | Оба проходят; build, test. |
| ClangCL 19.1.5, те же MSVC STL/SDK, Debug | Оба проходят; build, test. |
| Тот же ClangCL, Release | Оба проходят; build, test. |
| WSL Ubuntu 24.04, GCC 13.3.0 / libstdc++, Debug | Оба проходят; build, test. |
| Тот же GCC, Release | Оба проходят; build, test. |
| GCC Debug, ASan+UBSan, frame pointers | Оба проходят без диагностик; build, test. |
| Новая BUILD_TESTING=OFF, GCC | Обе библиотеки собраны, нет _deps/fetch; configure, build. |

[Compiler/cache settings](configurations.json) сохраняют точные конфигурации. Существующие build trees
перегенерировали CMake после изменения CMakeLists; повторно использован кеш закреплённого doctest
2.4.12. C++23 наследуется от qd_encoding; extensions отключены для production/test targets. Все семь
обязательных compiler/configuration проверок доступны. Первоначальный доступ SDK/WSL из песочницы
завершился ошибкой; те же проверки прошли с разрешённым повышенным доступом. Правок исходников
для устранения этих ошибок доступа не требовалось.

Framing executable проходит три cases / 246 166 assertions (сводка);
primitive-регрессии — четыре cases / 43 375 assertions / 1146 наблюдений Java
(вывод). [Negative proof](negative-proof.json) фиксирует успешный
baseline и пять намеренных ошибок fixtures/projections с ожидаемыми exit codes и неизменными хешами;
log. Настоящие goldens не изменялись.

C++ Format и затем Check прошли закреплённый конвейер Uncrustify 0.83.0 → clang-format 20.1.8 для
восьми файлов. Java Format выполнен перед двумя финальными captures; Check проходит четыре исходника
на отдельном JDK 25 formatter. Финальные проверки фиксируют formatting,
evidence audit, проверку относительных ссылок и git diff --check. Doxygen, naming, арифметика,
span bounds, владение, EN/RU-утверждения и направление зависимостей проверены отдельно от formatter.

## Воспроизведение

Стандартные [инструкции сборки](../../../docs/CPP-BUILD.ru.md) запускают оба suites. CMake cache options
QD_PROTOCOL_FIXTURES и QD_FRAME_PROJECTIONS позволяют временные отрицательные входы без изменения
настоящих ожиданий. Прямой negative check:

```powershell
python tools/run_utc.py --log .work/framing-negative.log -- python tests/conformance/check_frame_negative.py .work/msvc/Release/frame_tests.exe --report .work/framing-negative-proof.json
```

WSL sanitizer build использует `-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`,
Debug и существующий кеш закреплённого doctest, как в CPP-BUILD. Все сохранённые command logs созданы
tools/run_utc.py без перемаркировки исторического времени. Java metadata/builds отделены от C++
проекций кадров и явных lifecycle/resource-политик.

## Ограничения

API предварителен. Transport, payload parser, frame writer и новые protocol/session-компоненты
не добавлены. Огромные выделения, инъекция allocation failure, исчерпание uint64, 32-bit/macOS и другие
комбинации Java buffer/error policy не проверялись. Размер выходного payload ограничен бюджетом body
минус type; служебные расходы allocator/vector не являются точной гарантией heap footprint. Reader
имеет постоянный буфер body+9 элементов byte; сохранённые вызывающим кадры не входят в его суммарный
лимит. Исторические expectations/captures oracle и upstream.lock побайтово неизменны; новый P02 отделён.
