// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/ipf/instrument_profile_connection.hpp>

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace i = org::ttldtor::qdcxx::ipf;
using Clock = std::chrono::steady_clock;
constexpr std::chrono::milliseconds DEFAULT_DURATION{10000}; ///< Application observation interval.
constexpr std::chrono::milliseconds MAXIMUM_DURATION{
    600000};                                       ///< Sample command-line maximum, independent of SDK limits.
constexpr std::size_t DEFAULT_UPDATES = 1000;      ///< Retained application output references.
constexpr std::size_t MAXIMUM_UPDATES = 100000;    ///< Finite sample capture ceiling.
constexpr std::size_t MAXIMUM_SOURCE_BYTES = 4096; ///< Source spelling accepted before owned API preparation.

/** Application policy, without any native/runtime ownership. */
struct Arguments {
    std::string_view source;                               ///< Borrowed argv bytes, used only on main before return.
    std::chrono::milliseconds duration = DEFAULT_DURATION; ///< Absolute application stop is derived once.
    std::size_t updates = DEFAULT_UPDATES;                 ///< Requested preallocated output-reference count.
};

/** Parse a nonempty unsigned decimal argument without overflow or allocation.
 * @param text Borrowed argument.
 * @param maximum Inclusive sample ceiling.
 * @return Parsed positive value, or no value for invalid/out-of-range input.
 */
std::optional<std::size_t> positive(std::string_view text, std::size_t maximum) {
    std::size_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value == 0 ||
        value > maximum) {
        return {};
    }

    return value;
}

/** Validate all arguments before any runtime, source or application capture is created.
 * @param argc Argument count.
 * @param argv Borrowed process strings.
 * @return Explicit bounded policy, or no value; errors never echo source/credential text.
 */
std::optional<Arguments> arguments(int argc, char **argv) {
    Arguments result;
    bool run = false, source = false, duration = false, updates = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view name(argv[index]);

        if (name == "--run" && !run) {
            run = true;
            continue;
        }

        if (index + 1 >= argc) {
            return {};
        }

        const std::string_view value(argv[++index]);

        if (name == "--source" && !source) {
            if (value.empty() || value.size() > MAXIMUM_SOURCE_BYTES) {
                return {};
            }

            result.source = value;
            source = true;
        } else if (name == "--duration-ms" && !duration) {
            auto parsed = positive(value, static_cast<std::size_t>(MAXIMUM_DURATION.count()));

            if (!parsed) {
                return {};
            }

            result.duration = std::chrono::milliseconds(*parsed);
            duration = true;
        } else if (name == "--max-updates" && !updates) {
            auto parsed = positive(value, MAXIMUM_UPDATES);

            if (!parsed) {
                return {};
            }

            result.updates = *parsed;
            updates = true;
        } else {
            return {};
        }
    }

    if (!run || !source) {
        return {};
    }

    return result;
}

/** Print input-free help; no runtime, file, signal policy or worker is constructed.
 * @param russian Select translated descriptions.
 * @return Zero for successful output, one if output fails.
 */
int help(bool russian) {
    if (russian) {
        std::cout << "Использование: qdcxx-ipf-live --run --source SOURCE [параметры]\n"
                     "Live IPF через публичные runtime/connection/collector API.\n"
                     "  --source SOURCE       Обязательный URL или regular file; до 4096 байт.\n"
                     "  --duration-ms N       Интервал приложения 1..600000 мс, по умолчанию 10000.\n"
                     "  --max-updates N       Owning revisions для вывода 1..100000, по умолчанию 1000.\n"
                     "  --help / --help-ru    Справка без SDK, source I/O, сети или потоков.\n"
                     "Остановка по времени, числу revisions или закрытию connection; transport polling нет.\n"
                     "Сначала creator close/join, затем TSV revision/type/symbol/removed и summary.\n"
                     "UTF-16 значения escaped; prefix/initial snapshot и HTTP/container/native outcomes раздельны.\n"
                     "Коды: 0 первый snapshot завершён и stop/drain успешен; 3 snapshot неполный;\n"
                     "1 ошибка source/callback/preparation/output; 2 неверные аргументы.\n"
                     "Public transfer/model defaults остаются конечными; custom trust/auth флагов здесь нет.\n"
                     "Создаёт один SDK worker и source handles; файлы/settings/signals не меняет.\n";
    } else {
        std::cout
            << "Usage: qdcxx-ipf-live --run --source SOURCE [options]\n"
               "Live IPF through public runtime/connection/collector APIs.\n"
               "  --source SOURCE       Required URL or regular file; at most 4096 bytes.\n"
               "  --duration-ms N       Application interval 1..600000 ms; default 10000.\n"
               "  --max-updates N       Owning revisions to print 1..100000; default 1000.\n"
               "  --help / --help-ru    Help without SDK, source I/O, network or threads.\n"
               "Stops at duration, revision count or connection close; no transport polling.\n"
               "Creator close/join precedes TSV revision/type/symbol/removed and summary output.\n"
               "UTF-16 values are escaped; prefix/initial snapshot and HTTP/container/native outcomes are separate.\n"
               "Exits: 0 first snapshot completed and stop/drain succeeded; 3 incomplete snapshot;\n"
               "1 source/callback/preparation/output failure; 2 invalid arguments.\n"
               "Finite public transfer/model defaults apply; no custom trust/auth flags here.\n"
               "Creates one SDK worker and source handles; changes no files/settings/signal policy.\n";
    }

    std::cout.flush();

    return std::cout ? 0 : 1;
}

/** Finite application output capture, independent of connection/runtime/collector facades. */
struct Capture {
    std::mutex mutex;                ///< Protect the application predicate and capture vector.
    std::condition_variable changed; ///< Application duration/record completion, never a transport pump.
    std::vector<std::shared_ptr<const i::ProfileRevision>> revisions; ///< Owning references, reserved before start.
    const std::size_t maximum; ///< Exact application capacity, including removed revisions.
    bool closed = false;       ///< Advisory CLOSED event; join remains creator-owned.
    std::stop_source limit;    ///< Cancels only the public first-snapshot wait when the application limit is reached.

    /** @param count Finite prevalidated reference count. */
    explicit Capture(std::size_t count) : maximum(count) {
        revisions.reserve(count);
    }

    /** Keep only the bounded admitted prefix; callback does no output or source work.
     * @param batch Owning collector delivery, borrowed through callback return.
     */
    void accept(const std::shared_ptr<const i::ProfileBatch> &batch) {
        bool full = false;
        {
            std::lock_guard lock(mutex);

            for (const auto &revision : batch->profiles()) {
                if (revisions.size() == maximum) {
                    break;
                }

                revisions.push_back(revision);
            }

            full = revisions.size() == maximum;
        }
        changed.notify_all();

        if (full) {
            limit.request_stop();
        }
    }

    /** Publish only the application's terminal predicate.
     * @param state Advisory connection state.
     */
    void state(i::IpfConnectionState state) {
        if (state == i::IpfConnectionState::CLOSED) {
            {
                std::lock_guard lock(mutex);
                closed = true;
            }
            changed.notify_all();
        }
    }

    /** Await one application interval after initial completion, without polling or SDK calls.
     * @param deadline Latched application stop.
     */
    void awaitStop(Clock::time_point deadline) {
        std::unique_lock lock(mutex);
        changed.wait_until(lock, deadline, [&] {
            return closed || revisions.size() == maximum;
        });
    }
};

/** Emit lossless ASCII TSV escapes from a public UTF-16 value.
 * @param value Borrowed immutable revision text, valid while its revision is retained.
 */
void escaped(std::u16string_view value) {
    constexpr char HEX[] = "0123456789abcdef";
    constexpr char16_t FIRST_PRINTABLE = u' ', LAST_PRINTABLE = u'~';
    constexpr int HEX_DIGIT_BITS = 4, FIRST_HEX_SHIFT = 12;
    constexpr unsigned HEX_DIGIT_MASK = 0xf;

    for (const auto unit : value) {
        if (unit >= FIRST_PRINTABLE && unit <= LAST_PRINTABLE && unit != u'\\') {
            std::cout.put(static_cast<char>(unit));
        } else {
            std::cout << "\\u";

            for (int shift = FIRST_HEX_SHIFT; shift >= 0; shift -= HEX_DIGIT_BITS) {
                std::cout.put(HEX[(unit >> shift) & HEX_DIGIT_MASK]);
            }
        }
    }
}

/** Output only after this application's sole producer and its notifications have joined.
 * @param capture Owning references, no longer mutated.
 * @param observed Detached connection evidence after creator join.
 * @param collectorFailures Independent contained application callback failures.
 * @return True when all bounded output succeeds.
 */
bool print(const Capture &capture, const i::IpfConnectionObservation &observed, std::uint64_t collectorFailures) {
    std::cout << "revision\ttype\tsymbol\tremoved\n";

    for (const auto &revision : capture.revisions) {
        std::cout << revision->revision << '\t';
        escaped(revision->profile.getType());
        std::cout << '\t';
        escaped(revision->profile.getSymbol());
        std::cout << '\t' << (revision->profile.getType() == u"REMOVED") << '\n';
    }

    std::cout << "summary captured=" << capture.revisions.size()
              << " initial_snapshot=" << (observed.initialSnapshotComplete ? "complete" : "incomplete")
              << " retired=" << observed.retired << " state_callback_failures=" << observed.callbackFailures
              << " collector_callback_failures=" << collectorFailures;

    if (observed.lastAttempt) {
        const auto &attempt = *observed.lastAttempt;
        std::cout << " attempt_status=" << static_cast<int>(attempt.status) << " http_finished=" << attempt.httpFinished
                  << " ipf_finished=" << attempt.ipfFinished << " envelope_finished=" << attempt.envelope.finished
                  << " native_retired=" << attempt.nativeRetired;
    }

    if (observed.diagnostic) {
        std::cout << " error=" << static_cast<int>(observed.diagnostic->transfer.error)
                  << " stage=" << observed.diagnostic->transfer.stage;
    }

    std::cout << '\n';
    std::cout.flush();

    return static_cast<bool>(std::cout);
}
} // namespace

/** Run a finite public-API application; only creator owns join and SDK cleanup.
 * @param argc Process argument count.
 * @param argv Borrowed process arguments, never echoed on failure.
 * @return Documented usage/preparation/source/snapshot result.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        return help(false);
    }

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru") {
        return help(true);
    }

    const auto configured = arguments(argc, argv);

    if (!configured) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    try {
        auto prepared = i::InstrumentProfileConnectionRuntime::create();

        if (!prepared) {
            std::cerr << "Runtime preparation failed.\n";

            return 1;
        }

        auto runtime = std::move(*prepared);
        auto capture = std::make_shared<Capture>(configured->updates);
        std::shared_ptr<i::InstrumentProfileCollector> model = i::InstrumentProfileCollector::create();
        auto created = runtime->createConnection(configured->source, model);

        if (!created) {
            std::cerr << "Connection preparation failed.\n";

            return 1;
        }

        auto connection = std::move(*created);
        auto updates = model->addUpdateListener([capture](auto batch) {
            capture->accept(batch);
        });
        auto states = connection->addStateChangeListener([capture](auto, auto state) {
            capture->state(state);
        });

        if (!updates || !states) {
            std::cerr << "Listener admission failed.\n";

            return 1;
        }

        const auto deadline = Clock::now() + configured->duration;

        if (!connection->start()) {
            std::cerr << "Connection start failed.\n";

            return 1;
        }

        const auto initial = connection->awaitCompleted(deadline, capture->limit.get_token());

        if (initial == i::IpfCompletionWaitStatus::COMPLETED) {
            capture->awaitStop(deadline);
        }

        const bool applicationClose = connection->getState() != i::IpfConnectionState::CLOSED;
        connection->close();
        const auto joined = runtime->closeAndAwaitTermination();
        const auto observed = connection->observation();

        if (!joined || !observed.retired) {
            std::cerr << "Runtime retirement failed.\n";

            return 1;
        }

        const auto collectorFailures = model->callbackFailures();
        (void)model->removeUpdateListener(*updates);
        (void)connection->removeStateChangeListener(*states);
        const bool output = print(*capture, observed, collectorFailures);
        const bool failure = observed.diagnostic && !(applicationClose && observed.diagnostic->transfer.error ==
                                                                              i::IpfTransferError::CANCELLED);

        if (!initial || failure || observed.callbackFailures != 0 || collectorFailures != 0 || !output) {
            return 1;
        }

        return observed.initialSnapshotComplete ? 0 : 3;
    } catch (...) {
        std::cerr << "Application preparation/execution failed.\n";

        return 1;
    }
}
