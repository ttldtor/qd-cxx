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

Компонент не читает файлы/URL и не начинает фоновую работу. IPF CSV reading/writing, HTTP/live
collection, построение option-chain и толкование расписаний относятся к отдельным возможностям.
[Oracle fixture](../tests/fixtures/ipf/README.ru.md) описывает фактические доказательства преобразований.
