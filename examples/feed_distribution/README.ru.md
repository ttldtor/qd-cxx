# Запуск примера раздачи данных

[English version](README.md).

Пример запускается в одной из трёх ролей: источник публикации, сервер или приложение-подписчик. Все роли используют обычный QTP через IPv4 loopback. Источник публикует синтетические события IBM. Сервер передаёт объединённый спрос источнику и раздаёт Quote, TimeAndSale и Order клиентам. Клиент создаёт одну смешанную подписку через `qdcxx::feed`.

Пример собирается отдельно от исходников SDK, через установленный пакет Feed:

```sh
cmake -S /path/to/qd-cxx-install/share/qdcxx/examples/feed_distribution -B build/feed-distribution -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qd-cxx-install
cmake --build build/feed-distribution
ctest --test-dir build/feed-distribution --output-on-failure
```

Имя программы — `qdcxx-distribution` в Linux и `qdcxx-distribution.exe` в Windows. Корневая сборка создаёт цель `qdcxx_distribution`, когда включены Feed и примеры.

1. В первом терминале выполните `qdcxx-distribution source`. Возьмите номер порта из строки `PORT`.
2. Во втором терминале выполните `qdcxx-distribution server 0 SOURCE_PORT`. Возьмите его `PORT`. Строка `SOURCE` показывает согласованное поколение соединения с источником.
3. В третьем терминале выполните `qdcxx-distribution client SERVER_PORT`. Второго клиента можно запустить в отдельном терминале.
4. Дождитесь `ADD Quote`, `ADD TimeAndSale` и `ADD Order` у источника. Введите `QUOTE`, затем `EVENTS` и `REMOVE`. Клиенты печатают цену покупки, каждое событие сделки, индекс и флаги заявки.
5. Введите `CLEAR` у клиента, чтобы отписаться. Источник выводит REMOVE после ухода последнего клиента, подписанного на символ. Введите `STOP` или завершите stdin в каждом терминале, чтобы закрыть соответствующую роль.

`SENT` означает локальное принятие публикации. Строки событий у клиентов подтверждают вызов обработчиков. Пример выводит Order источника DEFAULT. Feed также может формировать производные Order из Quote. Команда `STATS` показывает локальное состояние соединений, спроса и очередей. `sentFrames` считает полные кадры DATA, которые принял локальный сокет. Сервер также показывает состояние соединения с источником и последний этап, на котором возникла ошибка. Полный список команд и кодов завершения доступен через `--help` и `--help-ru`. Поддерживаемые форматы и пределы описаны в [профиле публикации](https://github.com/ttldtor/qd-cxx/blob/master/spec/subscriptions/PUBLISHER.ru.md).
