#include "timezone_provider.hpp"

#include "date/tz_private.h"
#include "snapshot.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace org::ttldtor::qdcxx::time::detail::provider {
namespace {
namespace native = org_ttldtor_qdcxx_date_305_v1;
constexpr std::size_t PROFILE_FILE_COUNT = 11;
constexpr std::size_t MAX_PATH_BYTES = 4096;
constexpr std::size_t MAX_ZONE_BYTES = 128;
constexpr std::int64_t MIN_TIME_MS = -62135596800000LL;
// Leave one year of headroom for the native transition search's following-year arithmetic.
constexpr auto MAX_NATIVE_QUERY_YEAR = 32766;
constexpr std::int64_t MAX_TIME_MS =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        native::sys_days{native::year{MAX_NATIVE_QUERY_YEAR} / native::December / 31}.time_since_epoch())
        .count() +
    86400000 - 1;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::int64_t SECONDS_PER_HOUR = 3600;
constexpr std::int64_t SECONDS_PER_MINUTE = 60;
// Qualified counts belong to this exact source/data/patch profile, not arbitrary TZDB inputs.
constexpr std::size_t PROFILE_ZONES = 344;
constexpr std::size_t PROFILE_LINKS = 253;
constexpr std::size_t PROFILE_RULES = 2596;

enum class State { EMPTY, INITIALIZING, READY, FAILED };

struct LimitFailure {};

struct DataFailure {};

std::array<std::string, PROFILE_FILE_COUNT> retainedBytes;
std::atomic<State> state{State::EMPTY};
std::size_t modelUnits = 0;
std::size_t modelLimit = 0;

/** Bound scans before constructing owning path/zone strings.
 * @param text Null-terminated input, or null.
 * @param limit Maximum accepted bytes excluding terminator.
 * @return Whether a terminator occurs within the accepted byte range.
 */
bool boundedText(const char *text, std::size_t limit) {
    if (text == nullptr) {
        return false;
    }

    for (std::size_t i = 0; i <= limit; ++i) {
        if (text[i] == '\0') {
            return true;
        }
    }

    return false;
}

/** Identify only the Java custom offset forms explicitly qualified by this probe.
 * @param id Requested identity.
 * @param seconds Receives a fixed offset when the identity is a qualified fixed zone.
 * @return True for an accepted fixed zone; other IDs use the named provider path.
 */
bool fixedOffset(const std::string &id, std::int64_t &seconds) {
    if (id == "EST") {
        seconds = -5 * SECONDS_PER_HOUR;

        return true;
    }

    if (id == "UTC" || id == "GMT" || id == "Etc/UTC") {
        seconds = 0;

        return true;
    }

    if (id.size() != 9 || id.substr(0, 3) != "GMT" || (id[3] != '+' && id[3] != '-') || id[6] != ':') {
        return false;
    }

    for (auto i : {4, 5, 7, 8}) {
        if (id[i] < '0' || id[i] > '9') {
            return false;
        }
    }

    const auto hours = (id[4] - '0') * 10 + id[5] - '0';
    const auto minutes = (id[7] - '0') * 10 + id[8] - '0';

    if (hours > 23 || minutes > 59) {
        return false;
    }

    seconds = (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE) * (id[3] == '-' ? -1 : 1);

    return true;
}

/** Map a qualified Java short identity without consulting the machine's zone.
 * @param id Requested identity.
 * @return Explicit provider identity, preserving the caller's separate input identity.
 */
const native::time_zone *namedZone(const std::string &id) {
    return native::get_tzdb().locate_zone(id == "PST" ? "America/Los_Angeles" : id);
}
} // namespace

namespace input {
const std::string *snapshotFile(const char *name) {
    for (std::size_t i = 0; i < APPROVED_FILE_COUNT; ++i) {
        if (std::strcmp(name, APPROVED_FILES[i].name) == 0) {
            return &retainedBytes[i];
        }
    }

    return nullptr;
}

void admitNativeModel() {
    if (modelUnits == modelLimit) {
        throw LimitFailure{};
    }

    ++modelUnits;
}
} // namespace input

Error initialize(const std::filesystem::path &root, Limits limits, Stats &stats) noexcept {
    if (root.native().size() > MAX_PATH_BYTES || limits.contexts != 1 || limits.generations != 1) {
        return Error::INVALID_ARGUMENT;
    }

    auto expected = State::EMPTY;

    if (!state.compare_exchange_strong(expected, State::INITIALIZING)) {
        return Error::LIMIT;
    }

    Error result = Error::PROVIDER;

    try {
        const auto &path = root;

        if (!path.is_absolute() || !std::filesystem::is_directory(std::filesystem::symlink_status(path))) {
            result = Error::MISSING_DATA;
            throw DataFailure{};
        }

        if (input::APPROVED_FILE_COUNT > limits.files) {
            throw LimitFailure{};
        }

        if (limits.zones < PROFILE_ZONES || limits.links < PROFILE_LINKS || limits.rules < PROFILE_RULES) {
            throw LimitFailure{};
        }

        std::array<std::string, PROFILE_FILE_COUNT> staging;
        Stats staged{};

        for (std::size_t i = 0; i < input::APPROVED_FILE_COUNT; ++i) {
            const auto &file = input::APPROVED_FILES[i];
            const auto filePath = path / file.name;

            if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(filePath))) {
                result = Error::MISSING_DATA;
                throw DataFailure{};
            }

            if (file.size > limits.totalBytes - staged.bytes) {
                throw LimitFailure{};
            }

            std::ifstream input(filePath, std::ios::binary);

            if (!input) {
                result = Error::MISSING_DATA;
                throw DataFailure{};
            }

            staging[i].resize(file.size);
            input.read(staging[i].data(), static_cast<std::streamsize>(file.size));

            if (!input || input.peek() != std::char_traits<char>::eof() ||
                staging[i].compare(0, file.size, reinterpret_cast<const char *>(file.bytes), file.size) != 0) {
                result = Error::BAD_DATA;
                throw DataFailure{};
            }

            std::size_t lineLength = 0;
            bool atLineStart = true;

            for (const auto byte : staging[i]) {
                if (byte == '\n') {
                    if (++staged.lines > limits.lines) {
                        throw LimitFailure{};
                    }

                    lineLength = 0;
                    atLineStart = true;
                } else {
                    if (++lineLength > limits.lineBytes) {
                        throw LimitFailure{};
                    }

                    if (atLineStart && byte != '#' && byte != ' ' && byte != '\t' && byte != '\r') {
                        if (++staged.records > limits.records) {
                            throw LimitFailure{};
                        }
                    }

                    atLineStart = false;
                }
            }

            staged.bytes += file.size;
        }

        retainedBytes = std::move(staging);
        modelLimit = limits.modelUnits;
        const auto &db = native::get_tzdb();
        staged.zones = db.zones.size();
        staged.links = db.links.size();
        staged.rules = db.rules.size();

        if (staged.zones != PROFILE_ZONES || staged.links != PROFILE_LINKS || staged.rules != PROFILE_RULES ||
            db.version != "2026f") {
            throw DataFailure{};
        }

        for (const auto &zone : db.zones) {
            // Native rule/zonelet adjustment is lazy; finish it before publication.
            (void)zone.get_info(native::sys_seconds{std::chrono::seconds{0}});
        }

        for (const auto &link : db.links) {
            (void)db.locate_zone(link.name());
        }

        staged.modelUnits = modelUnits;
        stats = staged;
        state.store(State::READY, std::memory_order_release);

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        result = Error::ALLOCATION;
    } catch (const LimitFailure &) {
        result = Error::LIMIT;
    } catch (const DataFailure &) {
        // The precise admission error was set before throwing.
    } catch (...) {
        result = Error::PROVIDER;
    }

    state.store(State::FAILED, std::memory_order_release);

    return result;
}

Error localToUtc(const char *zone, std::int64_t localMs, std::int64_t &utcMs) noexcept {
    if (state.load(std::memory_order_acquire) != State::READY) {
        return Error::NOT_READY;
    }

    if (!boundedText(zone, MAX_ZONE_BYTES) || localMs < MIN_TIME_MS || localMs > MAX_TIME_MS) {
        return Error::INVALID_ARGUMENT;
    }

    try {
        const std::string id(zone);
        std::int64_t seconds = 0;

        if (!fixedOffset(id, seconds)) {
            const auto info = namedZone(id)->get_info(
                native::local_time<std::chrono::milliseconds>{std::chrono::milliseconds{localMs}});
            seconds = (info.result == native::local_info::ambiguous ? info.second.offset : info.first.offset).count();
        }

        utcMs = localMs - seconds * MILLIS_PER_SECOND;

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        return Error::ALLOCATION;
    } catch (...) {
        return Error::BAD_DATA;
    }
}

Error offsetAt(const char *zone, std::int64_t utcMs, std::int64_t &seconds) noexcept {
    if (state.load(std::memory_order_acquire) != State::READY) {
        return Error::NOT_READY;
    }

    if (!boundedText(zone, MAX_ZONE_BYTES) || utcMs < MIN_TIME_MS || utcMs > MAX_TIME_MS) {
        return Error::INVALID_ARGUMENT;
    }

    try {
        const std::string id(zone);

        if (!fixedOffset(id, seconds)) {
            seconds = namedZone(id)
                          ->get_info(native::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{utcMs}})
                          .offset.count();
        }

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        return Error::ALLOCATION;
    } catch (...) {
        return Error::BAD_DATA;
    }
}

Error resolve(const char *canonical, Zone &zone) noexcept {
    if (state.load(std::memory_order_acquire) != State::READY) {
        return Error::NOT_READY;
    }

    if (!boundedText(canonical, MAX_ZONE_BYTES)) {
        return Error::INVALID_ARGUMENT;
    }

    try {
        const auto *nativeZone = native::get_tzdb().locate_zone(canonical);
        zone.native = nativeZone;

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        return Error::ALLOCATION;
    } catch (...) {
        return Error::BAD_DATA;
    }
}

const char *canonicalName(Zone zone) noexcept {
    return static_cast<const native::time_zone *>(zone.native)->name().c_str();
}

Error offsets(Zone zone, std::int64_t utcMs, std::int64_t &offsetSeconds, std::int64_t &saveSeconds) noexcept {
    if (state.load(std::memory_order_acquire) != State::READY) {
        return Error::NOT_READY;
    }

    if (zone.native == nullptr || utcMs < MIN_TIME_MS || utcMs > MAX_TIME_MS) {
        return Error::INVALID_ARGUMENT;
    }

    try {
        const auto info = static_cast<const native::time_zone *>(zone.native)
                              ->get_info(native::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{utcMs}});
        offsetSeconds = info.offset.count();
        saveSeconds = std::chrono::duration_cast<std::chrono::seconds>(info.save).count();

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        return Error::ALLOCATION;
    } catch (...) {
        return Error::BAD_DATA;
    }
}

Error localOffsets(Zone zone, std::int64_t localMs, std::int64_t &firstSeconds, std::int64_t &secondSeconds,
                   int &result) noexcept {
    if (state.load(std::memory_order_acquire) != State::READY) {
        return Error::NOT_READY;
    }

    if (zone.native == nullptr || localMs < MIN_TIME_MS || localMs > MAX_TIME_MS) {
        return Error::INVALID_ARGUMENT;
    }

    try {
        const auto info =
            static_cast<const native::time_zone *>(zone.native)
                ->get_info(native::local_time<std::chrono::milliseconds>{std::chrono::milliseconds{localMs}});
        firstSeconds = info.first.offset.count();
        secondSeconds = info.result == native::local_info::unique ? firstSeconds : info.second.offset.count();
        result = info.result;

        return Error::NONE;
    } catch (const std::bad_alloc &) {
        return Error::ALLOCATION;
    } catch (...) {
        return Error::BAD_DATA;
    }
}

std::int64_t maximumUtcMillis() noexcept {
    return MAX_TIME_MS;
}

bool legacyBoundaries(std::int64_t utcMillis, std::int64_t rawSeconds, std::int64_t &springUtc,
                      std::int64_t &autumnUtc) noexcept {
    if (utcMillis < 0 || utcMillis > MAX_TIME_MS) {
        return false;
    }

    const auto instant = native::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{utcMillis}};
    const auto year = native::year_month_day{native::floor<native::days>(instant)}.year();
    const auto spring = native::sys_days{year / native::April / native::Sunday[native::last]};
    const auto autumn = native::sys_days{year / native::October / native::Sunday[native::last]};
    springUtc = std::chrono::duration_cast<std::chrono::milliseconds>(spring.time_since_epoch()).count() +
                (2 * SECONDS_PER_HOUR - rawSeconds) * MILLIS_PER_SECOND;
    autumnUtc = std::chrono::duration_cast<std::chrono::milliseconds>(autumn.time_since_epoch()).count() +
                (SECONDS_PER_HOUR - rawSeconds) * MILLIS_PER_SECOND;

    return true;
}
} // namespace org::ttldtor::qdcxx::time::detail::provider
