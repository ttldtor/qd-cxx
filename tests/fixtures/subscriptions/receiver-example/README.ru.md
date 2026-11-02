# Записи запусков установленного получателя Quote

[English version](README.md). Нормативный текст — EN.

Синтетические loopback-запуски от 2026-11-02 не используют внешний feed или системный
resolver. Источник объявляет полную встроенную схему Quote, затем передаёт OTHER и IBM
после фактического ACTIVE приложения. Decimal raw zero — NaN. DNS-запуски получают два
реальных запроса, числовые — ни одного. Каждое приложение печатает одну строку IBM
и сообщает DRAINED без истечения срока. Нативная диагностика после локального cutoff
сохранена без изменений.

| Профиль | Вывод приложения | Диагностика | Счётчики источника |
| --- | --- | --- | --- |
| MSVC 19.42 Release/MD numeric | [stdout](windows-numeric/live.stdout) | [stderr](windows-numeric/live.stderr) | [peer](windows-numeric/peer.stdout) |
| MSVC 19.42 Release/MD DNS | [stdout](windows-dns/live.stdout) | [stderr](windows-dns/live.stderr) | [peer](windows-dns/peer.stdout) |
| GCC 13.3 Debug ASan/UBSan numeric | [stdout](linux-numeric/live.stdout) | [stderr](linux-numeric/live.stderr) | [peer](linux-numeric/peer.stdout) |
| GCC 13.3 Debug ASan/UBSan DNS | [stdout](linux-dns/live.stdout) | [stderr](linux-dns/live.stderr) | [peer](linux-dns/peer.stdout) |

Linux leak detection отключена. [Список SHA-256](hashes.json) охватывает двенадцать
raw-файлов с исходными окончаниями строк платформ. [Отчёт проверки](../../../conformance/subscriptions/RECEIVER-VALIDATION.ru.md)
описывает тесты исходников, установленные сборки и ограничения.
[Контролируемый источник](../../../conformance/subscription_receiver_example_peer.cpp)
наблюдает один клиентский байт; записи не подтверждают содержание ADD, подтверждение
доставки, wire flush, чистое завершение транспорта или общую совместимость с поставщиками.
