# Локальное воспроизведение OnDemand

[English version](README.md).

Пример воспроизводит две синтетические котировки MOCK через установленный API qdcxx::feed. Он создаёт IPv4 loopback-источник с тем же конечным бинарным ответом и тестовым сервером, которые используются в проверках OnDemand. Endpoint начинает воспроизведение с паузой в 14:30:05 UTC 12 февраля 2024 года и переходит на скорость 2× после подтверждения покрытия. Слушатель запрашивает изменение скорости, а awaitState ожидает окончания без цикла опроса в приложении.

Соберите пример с совместимым установленным Feed SDK. Установленный каталог исходников содержит пример, фикстуру и тестовый источник.

~~~powershell
$prefix = 'C:/path/to/qdcxx-install'
$source = "$prefix/share/qdcxx/examples/on_demand"
cmake -S $source -B build/on-demand -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$prefix"
cmake --build build/on-demand
ctest --test-dir build/on-demand -R '^on_demand_example$' --output-on-failure
build/on-demand/qdcxx-on-demand --run
~~~

На Windows используйте подготовленный терминал компилятора и qdcxx-on-demand.exe. На Linux используйте тот же компилятор, runtime и режим сборки, что и для установленного SDK. Для SDK с TLS при конфигурации может понадобиться каталог проверенного OpenSSL.

Программа выводит MOCK bid=12.25 ask=12.5, затем MOCK bid=13.25 ask=13.5 с соответствующими временами событий. Без аргументов или с --help выводится английская справка без запуска SDK и сокетов. Код 0 означает получение двух котировок и опустошение очереди endpoint, 1 — ошибку источника или вывода, 2 — неверные аргументы. Ожидание ограничено 10 секундами, как и время жизни фикстуры. Рабочие потоки и loopback-сокеты завершаются до выхода процесса.

Это конечный синтетический профиль IPv4 loopback. Доступ к сервису поставщика, аутентификация, шифрование и производственные интервалы replay требуют отдельной проверки. [Руководство Feed API](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.ru.md) · [Синтетическая фикстура](https://github.com/ttldtor/qd-cxx/blob/master/tools/upstream/ondemand-oracle/README.ru.md)
