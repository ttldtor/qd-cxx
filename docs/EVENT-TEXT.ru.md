# Вывод текста событий и значений

[English version](EVENT-TEXT.md). Английская версия является основной.

Подключите qd/events/event_text.hpp и заголовок события; используйте target qdcxx_event_text. [Консольный пример](../examples/event_text/README.ru.md) выводит собственную UTF-8 строку напрямую через std::cout.

~~~cpp
using namespace org::ttldtor::qdcxx::events;
Quote quote{};
quote.symbol = u"DEMO";
const auto context = EventTextContext::utc();
const EventTextLimits limits{1024, 4096, 16384, 65536};
const auto text = toStringUtf8(quote, context, limits, MalformedUtf16::REJECT);
if (text) {
    std::cout << *text << '\n';
}
~~~

toString возвращает независимую собственную std::u16string; toStringUtf8 форматирует и преобразует её с общим бюджетом работы. Оба возвращают std::expected без частичного результата. События/контексты заимствуются только на время синхронного вызова и должны оставаться неизменными. Результат переживает уничтожение или замену входных объектов.

Явно выберите UTC или EventTextContext::fixedOffset(offsetMillis). Фиксированный offset принимает любой signed32 offset в миллисекундах, включая доли секунды, без инициализации timezone. Именованный контекст привязывается к общей явной capability данных timezone с переданным опорным моментом raw offset. Форматирование не обращается к часам, домашней/системной timezone, файлам, сети и не выполняет инициализацию. Нулевой provider для допустимого именованного ID даёт TIME_ZONE_NOT_READY.

Полные overloads поддерживают Quote, Profile, Trade, TradeETH и Summary. Overloads enum возвращают имена Java, включая DELETE для OrderAction::DELETE_ORDER; неизвестные сырые C++ enum codes явно отклоняются. Overloads sources сохраняют точные имена; IndexedEventSource различает null, пустое имя и буквальное null. formatEventFlags выводит каждый исходный signed32 бит в беззнаковом шестнадцатеричном виде со строчными буквами.

Символы и строки Profile сохраняют буквальный UTF-16, включая NUL и отдельные суррогаты. Exchange codes используют encodeEventChar. Преобразование в UTF-8 явно отклоняет непарные суррогаты либо заменяет каждый на U+FFFD; кодировку процесса и настройки консоли оно не меняет.

См. [формат и бюджеты](../spec/events/EVENT-TEXT.ru.md), [решение об эталоне](decisions/ADR-0011-java-event-text-reference.ru.md) и [границы проверок](../tests/conformance/EVENT-TEXT-VALIDATION.ru.md). Канонические helpers Candle/period/attribute/source и установленные round trips сохраняются.

Для именованных зон подключите qd/time/time_zone_data.hpp. При сборке производителя явно включите ORG_TTLDTOR_QDCXX_BUILD_TIME_ZONE_DATA, а у установленного пакета запросите компонент time_zone_data_backend. Само связывание EventText backend не включает. Один раз инициализируйте [контролируемые данные](TIME-ZONE-DATA.ru.md), затем создайте привязку:

~~~cpp
auto data = org::ttldtor::qdcxx::time::TimeZoneData::initialize(dataRoot);
if (data) {
    auto context = EventTextContext::timeZone(&*data, u"America/New_York", 0);
    // A successful context owns its binding; data and the input ID may now be destroyed.
}
~~~

Явный опорный момент выбирает метаданные raw offset конструктора; форматирование запрашивает фактическое смещение для каждого момента события. Историческое фактическое смещение может отличаться от опорного. maxIdUnits ограничивает копируемые единицы ID (по умолчанию 128), отдельно от лимитов форматирования. Ошибки диапазона/данных провайдера возвращаются как TIME_ZONE_RANGE/TIME_ZONE_DATA; неизвестный ID и отсутствие готового провайдера различаются. UTC и нормализованные GMT-привязки доступны через фабрику без готового провайдера; fixedOffset также принимает субсекундные и все signed32 смещения. Семантика именованных профилей и отдельная классификация различий версий данных наследуются от общего провайдера.