# Граница запрошенной памяти и работы DNS

[English version](DNS-RESOURCES.md). Английская версия нормативная.

Это консервативные admission bounds по
[квалифицированным исходникам c-ares 1.34.9+qdcxx2](../../cmake/runtime-profile/README.ru.md).
Они не измеряют физическую heap, RSS, CPU instructions или задержку.
[TCP runtime](TCP-RUNTIME.ru.md) должен выполнять все предусловия ниже; одной
формулы недостаточно для произвольной сборки SDK или адаптера.

## Предусловия профиля

- Проверенный source set и квалифицированный x64 ABI; аллокаторы по умолчанию,
  без SDK event thread, reinit, ambient resolver configuration, search, aliases,
  cache entries, address sorting, EDNS, DNS0x20 и failed-server probes.
- Один wire query `ares_query_dnsrec`, A затем AAAA. Следующий запрос вне callbacks
  только после возврата предыдущего entry, завершения query и idle cleanup.
  Не более четырёх числовых IPv4 серверов, одной попытки и одного UDP-to-TCP fallback.
- Абсолютное ASCII/A-label имя до 254 байт с root dot. Без getaddrinfo, service
  lookup и follow-up query для CNAME.
- Не более девяти candidate/active/retiring connections и 16 readiness events.
  Весь SDK entry принимает до двух успешных native reads и 131 070 входных байт.
  Обычные native callback attempts делят предел 128, включая connect. Close
  использует отдельный фиксированный резерв до 16 собственных descriptors и не
  требует ordinary data credits. При исчерпании обычного предела
  возвращается non-EINTR до новой нативной попытки.
- Фиксированный descriptor registry без выделения памяти и socket-state callback
  queue, ограниченные snapshots готовности. Для UDP write после WOULD_BLOCK нужен
  отдельный флаг registry: один legacy snapshot SDK может потерять WRITE.
- До 64 endpoints и 16 CNAME links. Copier выполняет до 17 CNAME scans и одного
  address scan, с ограниченными проходами по именам и дубликатам. Прикладной код
  не выполняется из C callback.
- Ошибки прекращают admission и выводят канал из работы вне callbacks. Поправка
  qdcxx2 для response adoption обязательна: потерянный response нельзя восстановить
  таймаутом адаптера или уничтожением канала.

Linux требует скомпилированный native `arc4random_buf` path; getrandom/fread
fallback SDK этой границей не квалифицирован. Windows использует выбранный
native entropy path. Доступность OS/entropy services и накладные расходы
аллокатора остаются отдельными предусловиями платформы.

## Запрошенная память

Пусть `L = 65535`, `R = floor(L / 11) = 5957`. До выделения section arrays парсер
проверяет RR count по remaining bytes и принимает один question. Имена имеют
до 255 presentation bytes и 128 compression transitions; escaped owned buffers
укладываются в 1024 байта. На проверенных ABI RR занимает 80 байт. Суммарная ёмкость
трёх RR arrays с power-of-two/minimum-four ростом — до `2R + 12` slots.

Граница одного полного parsed record:

```text
P = 80*(2R+12) + 1024*(3R+4) + 128R
    + 24*(2L+4R) + 34L + R + 1048576
  = 27020851 bytes < 32 MiB
```

Учтены RR capacities; owner/SOA/NAPTR names и short strings; control objects;
TXT/OPT/SVCB/HTTPS item capacities; wire-consuming payloads; временные имена
и moving realloc. Все parsed types учитываются, даже если copier игнорирует записи.
SDK разбирает сообщение до проверки query ID.

Deferred response может существовать вместе со следующим parsed unsolicited reply.
Резервируются два округлённых envelope по 32 MiB, девять input/reallocation peaks
соединений по 786 432 байта, девять output allowances по 8192 байта и 1 MiB
fixed/setup/query state:

```text
2*32MiB + 9*786432 + 9*8192 + 1MiB = 75309056 bytes < 80MiB
```

Резерв SDK на канал — **80 MiB**. Консервативный предел одновременно живых
allocations — 220 000. Заголовки аллокатора, fragmentation, native socket buffers
и process-wide OS state этим не измерены и не ограничены. Asio arenas, буферы
адаптера и владельцы протокола имеют отдельные резервы.

## Рабочие бюджеты

Логическая единица — консервативный byte visit/copy/zero/moving-copy,
object/loop/option visit либо allocation/free request. Это не инструкция CPU
и не интервал времени. До SDK entry списывается полный бюджет; callbacks делят
оставшиеся native quotas, не сбрасывая их.

На верхних пределах профиля ранее удержанные и новые байты дают:

```text
C=9; J=16; D=128; U=1048576
B=C*(L+2)+131070+4=720907
F=floor(B/2)=360453; T=floor(B/11)=65537; N=F+3T=557064
E=8*(1+3R)+32R+2L+32=464702
X=F*E+1024*(C+J+4)=167503259702
```

`F` включает пустые TCP frames. Намеренно грубый `F*E` учитывает максимальный
parser allocation pattern для каждого возможного кадра. Каждый запрос — до 1 MiB,
с до `2U` единиц zero/moving-copy. Более точная, но недоказанная оценка aggregate
copy не подставляется вместо этой границы.

Полное списание:

```text
F + 8T + 4B + N*(128+255+1) + floor(B/4)*floor(L/4)
+ 2X + 2U*X + 8*1024*N + 128B + 128T + 4096F
+ 2*(J+2)*524288 + 1024*(D+J+C+4+1)
+ (18R+64R+17*17) + (18R*16*1024+R*64*16+17*17*4*256)
= 351280142190671838 logical units
```

Результат помещается в `uint64_t`; checked arithmetic сохраняет 64-битные
промежуточные значения. `dnsProgressWorkRequirement` возвращает консервативные
progress credits, чтобы вызывающая сторона не копировала число. Это достаточный
бюджет, не минимум и не измеренная работа. Увеличение предела требует нового вывода.

Подготовка канала учитывается отдельно. Для `S` серверов в пределах 1–4:

```text
Einit=1024*(S+1)
byteUnits=2U*Einit+16384; visits=3*Einit
```

При четырёх серверах нужны 10 737 434 624 byte-work credits и 15 360 visits.
`dnsPreparationWorkRequirement` сообщает этот DNS-only бюджет до create; проверка
wrapper, инициализация арен и фабрика чистой сессии имеют отдельные списания.
Эти credits **не** запрашивают 10 GiB памяти: реальная запрошенная память остаётся
в отдельном envelope выше. Deadline не прерывает синхронный parser и не задаёт
независимую от оборудования длительность.
