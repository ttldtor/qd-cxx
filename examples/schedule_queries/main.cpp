// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/schedule/schedule.hpp"
#include <iostream>
#include <string_view>

namespace {
/** @tparam Character Native platform argument character.
 * @param argument Native command-line argument.
 * @param ascii ASCII option spelling.
 * @return Exact option equality without locale-dependent conversion.
 */
template <class Character> bool option(const Character *argument, std::string_view ascii) {
    const std::basic_string_view<Character> text(argument);

    if (text.size() != ascii.size()) {
        return false;
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (text[index] != static_cast<Character>(ascii[index])) {
            return false;
        }
    }

    return true;
}

/** @tparam Character Native platform argument character.
 * @param argc Native argument count.
 * @param argv Native arguments; Windows passes wide paths without an ACP roundtrip.
 * @return 0 help/success, 1 data/query failure, 2 invalid command line.
 */
template <class Character> int run(int argc, Character **argv) {
    using namespace org::ttldtor::qdcxx::schedule;

    if (argc == 1 || (argc == 2 && option(argv[1], "--help"))) {
        std::cout
            << "Usage: qdcxx-schedule-queries --run --data ABSOLUTE_TZDB_DIRECTORY\n"
               "Print XNYS day/session values for 2024-11-04 using the pinned installed TZDB profile.\n"
               "Default: help, no file/native work. --help-ru: Russian help. No downloads or workers.\n"
               "Data must be the exact installed tzdb-2026f directory. Exit0 help/success,1 failure,2 arguments.\n";

        return 0;
    }

    if (argc == 2 && option(argv[1], "--help-ru")) {
        std::cout << "Использование: qdcxx-schedule-queries --run --data АБСОЛЮТНЫЙ_КАТАЛОГ_TZDB\n"
                     "Печатает Day/Session XNYS за 2024-11-04 из закреплённого установленного TZDB2026f.\n"
                     "По умолчанию: справка без файлов и нативной инициализации. Загрузок и фоновых потоков нет.\n"
                     "Коды:0 справка/успех,1 ошибка данных/запроса,2 неверные параметры.\n";

        return 0;
    }

    if (argc != 4 || !option(argv[1], "--run") || !option(argv[2], "--data")) {
        return 2;
    }

    const auto data = ScheduleTimeZoneData::initialize(std::filesystem::path(argv[3]));

    if (!data) {
        std::cerr << "Timezone initialization failed: " << static_cast<int>(data.error()) << '\n';

        return 1;
    }

    const auto context = ScheduleDataContext::create(*data, {}, {1793793600000LL, 128});

    if (!context) {
        return 1;
    }

    const auto schedule = Schedule::getInstance(*context, u"XNYS()");

    if (!schedule) {
        return 1;
    }

    const auto day = schedule->getDayByYearMonthDay(20241104);

    if (!day) {
        std::cerr << "Day query failed: " << static_cast<int>(day.error()) << '\n';

        return 1;
    }

    std::cout << "day=" << day->getYearMonthDay() << " start=" << day->getStartTime() << " end=" << day->getEndTime()
              << '\n';

    for (const auto &session : day->getSessions()) {
        std::cout << "type=" << static_cast<int>(session.getType()) << " start=" << session.getStartTime()
                  << " end=" << session.getEndTime() << " trading=" << session.isTrading()
                  << " empty=" << session.isEmpty() << '\n';
    }

    return 0;
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    return run(argc, argv);
}
#else
int main(int argc, char **argv) {
    return run(argc, argv);
}
#endif
