# Контрольные точки NetTest для записей и событий

## Назначение

Эти инструменты сравнивают публикацию на разных уровнях. Результат имеет смысл, когда совпадают тип записи или события, контракт коллектора, символы, размер пакета, предложенная скорость, число соединений, длительность и нагрузка потребителя. По умолчанию инструменты измеряют разную работу.

## QD NetTest

QD NetTest публикует физические записи QD. Производитель заполняет `RecordBuffer` сгенерированными записями и передаёт буфер коллекторам через `QDDistributor.processData`. Тип записи задаётся свойством `com.devexperts.qd.tools.NetTest.record`, по умолчанию это `TimeAndSale`. Это эталон совместимости для пути записей. Один запуск проверяет один тип записи, а не весь каталог схемы. [Производитель QD 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java), [производитель QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java), [выбор записи в QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestSide.java)

Общий CLI поддерживает режим производителя или потребителя (`p` либо `c`), адрес, число экземпляров (`-C`), настройку символов (`-S`), выбор коллектора (`-c`), wildcard-подписку (`-w`), ограничение скорости (`-r`), период статистики (`-s`) и имя вывода (`-n`). Размер пакета по умолчанию равен 100 записям и задаётся свойством `com.devexperts.qd.tools.NetTest.batchSize`. Коллектор по умолчанию — ticker, период статистики — 10 секунд. [CLI QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTest.java)

В проверенных основных файлах QD 3.357 сохраняет производителя, CLI, выбор записи и поиск временных полей из QD 3.355. В потребитель добавлена включённая по умолчанию распаковка объектных полей через `com.devexperts.io.Marshalled.unwrap`. Ею управляет `com.devexperts.qd.tools.NetTest.unwrapObjects`, значение по умолчанию — `true`. Для записей с объектными полями это добавляет работу по декодированию на стороне потребителя. Путь кодирования записей у производителя не изменился. [Потребитель QD 3.355](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java), [потребитель QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java)

Потребитель вычисляет задержку по временным полям и `Sequence`, если распознаёт их раскладку. Для `TimeAndSale` он объединяет секунды и старшие биты `Sequence`, поэтому точность составляет одну миллисекунду. Если подходящих полей времени нет, измеренная задержка равна нулю. При сравнении задержек сохраняйте одинаковые тип записи и коллектор. [Вычисление задержки в QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestConsumerAgentThread.java)

На стороне производителя таймер lag включает ожидание ограничителя скорости и генерацию записей. Замер длительности вызова публикации учитывайте отдельно. [Производитель QD 3.357](https://github.com/devexperts/QD/blob/052e916c842026cf23a8dcf2067b83cbcc929b88/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java)

## Публикация событий dxFeed

dxFeed Graal Native SDK открывает dxFeed API через native-интерфейс. Его следует измерять как путь API событий. В публичном описании SDK нет CLI для бенчмарка, поэтому параметры командной строки нужно документировать в репозитории реализации, когда он публичен. [Graal Native SDK](https://github.com/dxFeed/dxfeed-graal-native-sdk)

Java API dxFeed публикует объекты событий через `DXPublisher.publishEvents`. Для издателя доступны роли `PUBLISHER` и `STREAM_PUBLISHER`. Это путь API событий на Java, отдельный от прямого вызова QD-дистрибьютора записей. [API DXPublisher](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html), [API DXEndpoint](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXEndpoint.html)

Эти измерения событийных API не доказывают побайтовую совместимость с QD NetTest. Java API и C API имеют собственные границы вызова и runtime. Для сравнения пропускной способности сначала выровняйте нагрузку и контракт потребителя, а затем отдельно фиксируйте вызовы публикации и результаты на стороне потребителя для каждого API. [Graal Native SDK](https://github.com/dxFeed/dxfeed-graal-native-sdk), [API DXPublisher](https://docs.dxfeed.com/dxfeed/api/com/dxfeed/api/DXPublisher.html)

## Совместимость и измерения

- Оставьте QD 3.355 закреплённым baseline совместимости. QD 3.357 проверен по исходникам как дополнительный эталон на коммите `052e916c842026cf23a8dcf2067b83cbcc929b88`. Это сравнение не требует менять pin 3.355. [Тег QD 3.355](https://github.com/devexperts/QD/tree/qd-qds-3.355), [тег QD 3.357](https://github.com/devexperts/QD/tree/qd-qds-3.357)
- Для взаимодействия по записям используйте QD NetTest. Явно задайте один тип записи на производителе и потребителе и выберите контракт коллектора, который передаёт эту запись.
- Свойство `batchSize` в QD задаёт число записей. Для каждого API событий проверьте, что именно считает размер его пакета, затем выровняйте фактическое число записей на пакет.
- Раздельно учитывайте вызовы производителя, записи, принятые API, байты транспорта, записи, доставленные потребителю, и его задержку. Локальная скорость издателя сама по себе не подтверждает доставку.
- Ticker может объединять повторные обновления. Для проверки доставки каждого опубликованного обновления используйте потоковый контракт.
- Задержка QD NetTest для стандартных раскладок времени и последовательности имеет точность до миллисекунды. Не трактуйте её как субмиллисекундную сквозную задержку.
- Для тестирования полного каталога нужен отдельный сценарий, который перебирает каждую запись и проверяет оба направления. Сам QD NetTest выбирает одну запись на запуск.
