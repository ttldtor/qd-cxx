# Значения instrument profile {#instrument-profiles-ru}

[English version](INSTRUMENT-PROFILES.md). Английская версия является нормативной.

`org::ttldtor::qdcxx::ipf::InstrumentProfile` хранит независимые метаданные инструмента.
Модель владеет стандартными/пользовательскими полями и поддерживает типизированные числа и
идентификаторы календарного дня. Это отдельная модель от подписываемого рыночного события Profile.

При конфигурации qd-cxx включите `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON`; по умолчанию OFF.
Линкуйте `qdcxx_ipf`, подключайте `qd/ipf/instrument_profile.hpp`. Runtime, Feed, JSON, сетевой SDK
и дополнительные библиотечные зависимости не нужны. Установленный пакет экспортирует тот же target:

```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(profile_example main.cpp)
target_link_libraries(profile_example PRIVATE qdcxx_ipf)
```

```cpp
#include <qd/ipf/instrument_profile.hpp>

int main() {
    using org::ttldtor::qdcxx::ipf::InstrumentProfile;
    InstrumentProfile profile;
    if (!profile.setType(u"STOCK") || !profile.setSymbol(u"IBM") ||
        !profile.setDescription(u"International Business Machines") ||
        !profile.setMultiplier(1) || !profile.setField(u"DESK", u"equities")) {
        return 1;
    }
    auto independent = profile;
    return independent.getSymbol() == u"IBM" &&
                   independent.getMultiplier() == 1 &&
                   independent.getField(u"DESK") == u"equities" ? 0 : 2;
}
```

Все 31 стандартное поле имеют именованные getters/setters. Строковые getters возвращают
заимствованные UTF-16 views; целочисленные и числовые — значения. Даты используют знаковые дни
от 1970-01-01 GMT: `setExpiration(1)` сохраняет день 1, а
`setField(u"EXPIRATION",u"1970-01-02")` разбирает то же значение. Общие
`getNumericField()/getDateField()` возвращают `std::expected`; обращение к стандартному
строковому полю как к числу возвращает `TEXTUAL_FIELD`. Неуспешный setter сохраняет профиль целиком.

Неизвестные поля и значения TYPE поддержаны, имена чувствительны к регистру, отсутствующие
пользовательские поля читаются пустыми. `setField(u"DESK",nullptr)` очищает пользовательское значение.
Пустота и null нормализуются вместе; исходная информация о присутствии поля в файле отделена.
Текст — точный UTF-16, без case folding и Unicode-нормализации. Копии остаются независимыми после
изменений. Getter views истекают при изменении, присваивании или уничтожении; перечисление имён
владеет результатом. Вызывающая сторона сериализует изменение/наблюдение.

Лимиты экземпляра по умолчанию: 256 пользовательских полей, 256 единиц имени, 65536 на значение,
1048576 совокупных UTF-16 единиц и 65536 входных единиц. Передавайте `ProfileLimits`, только если
приложению нужны другие границы. Канонические числа/даты также учитываются в лимитах хранимого
текста. Глобальные настройки не читаются. Преобразования, неограниченные числа, lenient-исторические
даты и различие точного символа/порядка представления определяет [контракт](../spec/ipf/PROFILE.ru.md).

Объект значения не начинает I/O или фоновую работу. Тот же optional target предоставляет отдельный
локальные reader и writer ниже. HTTP/live collection, построение option-chain и толкование расписаний
относятся к отдельным возможностям.
[Oracle fixture](../tests/fixtures/ipf/README.ru.md) описывает фактические доказательства преобразований.

## Читать локальный IPF stream

Подключите `qd/ipf/instrument_profile_reader.hpp` и свяжите тот же target `qdcxx_ipf`.
`readInstrumentProfilesFile(path)` открывает локальный файл в binary mode. Для уже принадлежащего
вам stream используйте синхронную перегрузку: она не закрывает stream и не делает seek:

```cpp
#include <qd/ipf/instrument_profile_reader.hpp>
#include <sstream>

int main() {
    using org::ttldtor::qdcxx::ipf::readInstrumentProfiles;
    std::istringstream input(
        "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n"
        "STOCK,IBM,International Business Machines\n"
        "##COMPLETE\n");
    auto result = readInstrumentProfiles(input);
    if (result.error || !result.complete || result.profiles.size() != 1) {
        return 1;
    }
    return result.profiles.front().getSymbol() == u"IBM" ? 0 : 2;
}
```

Результат владеет целыми profiles в порядке источника. При ошибке `profiles` сохраняет принятый
целый префикс, `error` описывает первую ошибку; частичная row не становится profile.
`completionSeen` и чистый `finished` — отдельные свидетельства; `complete` требует оба.
Marker завершения не останавливает stream. Default ALLOW принимает чистый EOF без marker как
неполный; явно выбирайте `IpfCompletionPolicy::REQUIRE`, если приложение требует marker.

Reader принимает объявленные/переопределённые форматы, точные стандартные/custom names, quoted
запятые/newlines, парные кавычки и JDK-совместимые UTF-8 replacements. По умолчанию input ограничен
64 MiB, output — 100000 profiles/16 Mi UTF-16 units, отдельно ограничены row/field/header и каждый
profile. Это инженерные значения по умолчанию: передавайте `IpfReadOptions` для иных границ.
Чистый `InstrumentProfileParser` поддерживает incremental spans, один владеющий profile/control
item и явно пополняемый work. См. [контракт reader](../spec/ipf/READER.ru.md) и
[фактические наблюдения грамматики](../tests/fixtures/ipf-reader/README.ru.md).

Input/output/work bounds не задают I/O deadline. Streambuf вызывающей стороны может читать
заранее; `acceptedBytes` — логическое потребление parser, а не `tellg()` или обещание rollback.
Маска исключений stream вызывающей стороны сохраняется. Вышедший наружу `std::bad_alloc` даёт
`ALLOCATION_FAILURE`; если stream подавляет его и устанавливает `badbit`, наблюдаемый результат —
`IO_ERROR`. Ошибки выделения памяти parser/output дают `ALLOCATION_FAILURE`. Типизированные ошибки
сохраняют полные предшествующие profiles.
Поддержан только локальный несжатый input; HTTP, decompression и автоопределение формата не запускаются.

## Записывать и читать локальные profiles

Подключите `qd/ipf/instrument_profile_writer.hpp` и свяжите `qdcxx_ipf`. Обычный stream round trip:

```cpp
#include <qd/ipf/instrument_profile_writer.hpp>
#include <qd/ipf/instrument_profile_reader.hpp>
#include <array>
#include <sstream>

int main() {
    using namespace org::ttldtor::qdcxx::ipf;
    InstrumentProfile profile;
    if (!profile.setType(u"STOCK") || !profile.setSymbol(u"IBM")) {
        return 1;
    }
    const std::array profiles{profile};
    std::ostringstream output;
    auto written = writeInstrumentProfiles(output, profiles);
    if (written.error || !written.complete) {
        return 2;
    }
    std::istringstream input(output.str());
    auto read = readInstrumentProfiles(input);
    return !read.error && read.complete && read.profiles.size() == 1 &&
           read.profiles.front().getSymbol() == u"IBM" ? 0 : 3;
}
```

`composeInstrumentProfiles(profiles)` возвращает владеющую полную UTF-8 строку без I/O.
`writeInstrumentProfilesFile(path, profiles)` готовит документ до открытия собственного binary/
truncating файла. Stream API заимствует ваш stream на вызов и сохраняет exception mask.

Formats/CSV сохраняют canonical field text, точные type/custom names и порядок profiles.
Исходное написание чисел, raw presence и биты signed zero не сериализуются. Чтобы не терять text
тихо, непарный UTF-16, запрещённые reader types и непустые extra REMOVED fields отклоняются до
публикации; обычные REMOVED tombstones, unknown/empty types и valid surrogate pairs работают.
См. [writer contract](../spec/ipf/WRITER.ru.md) и
[actual writer observations](../tests/fixtures/ipf-writer/README.ru.md).

Defaults ограничивают profiles/types/fields, retained input/format units, output bytes и logical
work; передавайте `IpfWriteOptions` для иных инженерных границ. Value metadata `retainedUnits()`
и `customFieldCount()` ничего не выделяют и описывают logical UTF-16 storage/entries, а не RSS.

Любая I/O ошибка даёт complete=false. Body write/flush предшествует публикации marker; поздняя
ошибка marker/final flush уже могла оставить снаружи полный marker.
`confirmedBytes` учитывает лишь успешные write calls, а не точные side effects неуспешного вызова.
Streams/files не транзакционны; open файла может уничтожить прежнее содержимое. Для успешного
file result нужен успешный owned close. Resource limits не задают deadline блокирующей I/O.


## Решение о загрузке URL sources

Локальные value/read/write APIs остаются независимыми от сетевых зависимостей. Выбранный
[контракт dependency/ownership загрузки](../spec/ipf/TRANSFER.ru.md) определяет Beast HTTP parser,
скомпилированный статический Boost.URL и повторное использование controlled TLS profile. Он задаёт
владение source/credentials и квалификацию до доступности URL loading, но не добавляет URL API.

## Цепочки опционов

Тот же необязательный IPF-компонент включает [владеющие снимки option chains](OPTION-CHAINS.ru.md) с точным Java-порядком серий/strike и конечными лимитами хранения, временных копий и работы.
