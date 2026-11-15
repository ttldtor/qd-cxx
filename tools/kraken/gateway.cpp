// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "subscription.hpp"

#include "qd/api/publisher.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr std::string_view LOOPBACK = "127.0.0.1";
constexpr std::u16string_view SYMBOL = u"BTC/USD";
constexpr std::size_t TRUST_LIMIT = 512 * 1024;
constexpr auto MAX_DURATION = 600000ms;
constexpr auto OWNER_TURN = 25ms;
static_assert(std::atomic_bool::is_always_lock_free);
std::atomic_bool interrupted = false;

constexpr std::string_view USAGE = R"(Usage: qdcxx-kraken-gateway --trust-pem PATH [options]
Publish Kraken Spot BTC/USD quotes to local plain QTP clients.

  --trust-pem PATH      Explicit CA certificate PEM file, up to 512 KiB (required).
  --source-host HOST    Route to the WSS server (default ws.kraken.com).
  --source-identity DNS  Verified TLS name and SNI (default ws.kraken.com).
  --source-port N       WSS TCP port (default 443).
  --listen-port N       Port on 127.0.0.1 (default 0, selected by the OS).
  --duration-ms N       Run for 1..600000 ms (default 30000) or until interrupted.
  --trace-quotes        Print exact QTP values on local publication admission.
  --help, --help-ru     Print help without opening files or sockets.

The first subscription starts after a local client requests BTC/USD.
Exit codes: 0 completed with a publication, 1 I/O/runtime failure or no publications,
2 invalid arguments, 130 interrupted. Running opens a local listener and a public WSS connection.
)";

constexpr std::string_view USAGE_RU = R"(Использование: qdcxx-kraken-gateway --trust-pem PATH [параметры]
Публикация котировок Kraken Spot BTC/USD для локальных клиентов QTP.

  --trust-pem PATH      Обязательный PEM-файл сертификатов УЦ, до 512 КиБ.
  --source-host HOST    Маршрут до WSS-сервера (по умолчанию ws.kraken.com).
  --source-identity DNS  Проверяемое имя TLS и SNI (по умолчанию ws.kraken.com).
  --source-port N       Порт WSS (по умолчанию 443).
  --listen-port N       Порт на 127.0.0.1 (по умолчанию 0, выбирает ОС).
  --duration-ms N       Работа 1..600000 мс (по умолчанию 30000) или до прерывания.
  --trace-quotes        Вывод точных значений QTP при допуске к публикации.
  --help, --help-ru     Справка без открытия файлов и сокетов.

Подписка на Kraken начинается после первого локального запроса BTC/USD.
Коды выхода: 0 завершение с публикацией, 1 ошибка или отсутствие публикаций,
2 ошибка аргументов, 130 прерывание. Запуск открывает локальный порт и соединение WSS.
)";

/** Own the process inputs until the listener and the source have each copied their configuration. */
struct Settings {
    q::kraken::SubscriptionSettings source;
    std::filesystem::path trustPath;
    std::uint16_t listenPort = 0;
    std::chrono::milliseconds duration{30000};
    bool trace = false;
};

/** Only record the stop request in the signal handler.
 * @param signalNumber Signal number provided by the runtime.
 */
void requestInterrupt([[maybe_unused]] int signalNumber) noexcept {
    interrupted.store(true, std::memory_order_relaxed);
}

/** Parse a complete decimal CLI argument inside its allowed range.
 * @tparam T Numeric type accepted by std::from_chars.
 * @param text Exact argument text, borrowed through the call.
 * @param[out] value Parsed number on success.
 * @param minimum Inclusive lower bound.
 * @param maximum Inclusive upper bound.
 * @return True for a complete in-range argument.
 */
template <class T> bool readNumber(std::string_view text, T &value, T minimum, T maximum) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);

    return error == std::errc{} && end == text.data() + text.size() && value >= minimum && value <= maximum;
}

/** Read arguments without starting the runtime or resolving trust.
 * @param arguments Borrowed arguments after the executable name.
 * @return Owned settings or no value if an option is invalid.
 */
std::optional<Settings> readSettings(std::span<char *> arguments) {
    constexpr std::array<std::string_view, 7> names{"--trust-pem",   "--source-host", "--source-identity",
                                                    "--source-port", "--listen-port", "--duration-ms",
                                                    "--trace-quotes"};
    std::array<bool, names.size()> seen{};
    Settings settings;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view option = arguments[i];
        const auto found = std::ranges::find(names, option);

        if (found == names.end())
            return std::nullopt;

        const auto index = static_cast<std::size_t>(found - names.begin());

        if (std::exchange(seen[index], true))
            return std::nullopt;

        if (option == "--trace-quotes") {
            settings.trace = true;
            continue;
        }

        if (++i == arguments.size())
            return std::nullopt;

        const std::string_view value = arguments[i];

        if (value.empty() || value.starts_with("--"))
            return std::nullopt;

        if (option == "--trust-pem") {
            settings.trustPath = value;
        } else if (option == "--source-host") {
            settings.source.routeHost = value;
        } else if (option == "--source-identity") {
            settings.source.identity = value;
        } else if (option == "--source-port") {
            if (!readNumber<std::uint16_t>(value, settings.source.port, 1, UINT16_MAX))
                return std::nullopt;
        } else if (option == "--listen-port") {
            if (!readNumber<std::uint16_t>(value, settings.listenPort, 0, UINT16_MAX))
                return std::nullopt;
        } else {
            int millis = 0;

            if (!readNumber(value, millis, 1, static_cast<int>(MAX_DURATION.count())))
                return std::nullopt;

            settings.duration = std::chrono::milliseconds(millis);
        }
    }

    if (settings.trustPath.empty())
        return std::nullopt;

    return settings;
}

/** Read bounded bytes from a configured local regular file for SDK trust admission.
 * @param path Trusted local configuration path, borrowed for this read.
 * @return Owned PEM input, or no value on an I/O or size refusal.
 */
std::optional<std::string> readTrust(const std::filesystem::path &path) {
    std::error_code error;

    if (!std::filesystem::is_regular_file(path, error))
        return std::nullopt;

    std::ifstream input(path, std::ios::binary);

    if (!input)
        return std::nullopt;

    std::string data(TRUST_LIMIT + 1, '\0');
    input.read(data.data(), static_cast<std::streamsize>(data.size()));
    const auto count = input.gcount();

    if (!input.eof() || input.bad() || count <= 0 || count > static_cast<std::streamsize>(TRUST_LIMIT))
        return std::nullopt;

    data.resize(static_cast<std::size_t>(count));

    return data;
}

/** Report a static library diagnostic without copying peer messages.
 * @param error Detached Publisher diagnostic.
 */
void report(q::api::Diagnostic error) {
    std::cerr << error.stage << ": code=" << static_cast<int>(error.code) << " native=" << error.nativeCode
              << " detail=" << error.detailCode << '\n';
}

/** Report the latest source failure as bounded static and numeric fields.
 * @param diagnostic Borrowed diagnostic from the source owner.
 */
void reportSource(const q::kraken::SubscriptionDiagnostic &diagnostic) {
    std::cerr << "Kraken source: generation=" << diagnostic.generation
              << " reason=" << static_cast<int>(diagnostic.reason);

    if (diagnostic.transport) {
        const auto &transport = *diagnostic.transport;
        std::cerr << " stage=" << static_cast<int>(transport.stage)
                  << " transport=" << static_cast<int>(transport.reason) << " native=" << transport.native.value()
                  << " verify=" << transport.verifyError << " http=" << transport.httpStatus
                  << " close=" << transport.closeCode;
    }

    if (diagnostic.decode)
        std::cerr << " decode=" << static_cast<int>(*diagnostic.decode);

    std::cerr << '\n';
}

/** Emit only an unambiguous single-line timestamp token in publication traces.
 * @param text Provider timestamp, borrowed from the current observation.
 * @return View of the admitted token, or a static placeholder.
 */
std::string_view timestampToken(std::string_view text) noexcept {
    constexpr std::string_view SYMBOLS = "0123456789TtZz:.+-";

    return !text.empty() && text.find_first_not_of(SYMBOLS) == std::string_view::npos ? text : "-";
}

/** Drive the existing source and the local Publisher from the application thread.
 * @param settings Owning run configuration, including the local trust path.
 * @return Gateway process exit code.
 */
int run(Settings settings) {
    auto trust = readTrust(settings.trustPath);

    if (!trust) {
        std::cerr << "Cannot read the configured CA certificate PEM file.\n";

        return 1;
    }

    settings.source.trustPem = std::move(*trust);

    if (std::signal(SIGINT, requestInterrupt) == SIG_ERR || std::signal(SIGTERM, requestInterrupt) == SIG_ERR) {
        std::cerr << "Cannot install interrupt handlers.\n";

        return 1;
    }

    auto bootstrap = q::transport::RuntimeBootstrap::create();

    if (!bootstrap) {
        std::cerr << "Cannot initialize the QTP runtime.\n";

        return 1;
    }

    q::api::EventRegistryWork registryWork;
    auto registry = q::api::EventRegistry::builtIn({}, registryWork);

    if (!registry) {
        std::cerr << "Cannot build the Quote registry.\n";

        return 1;
    }

    q::api::PublisherOptions options;
    options.listener.address = LOOPBACK;
    options.listener.port = settings.listenPort;
    options.demand.ticker = q::api::FeedFilter::Builder(*registry).withCategory(u"Kraken", u":Quote&BTC[/]USD").build();
    auto endpoint = q::api::PublisherEndpoint::create(*bootstrap, std::move(options));

    if (!endpoint) {
        report(endpoint.error());

        return 1;
    }

    const auto deadline = Clock::now() + settings.duration;
    std::cout << "LISTEN " << LOOPBACK << ' ' << (*endpoint)->port() << '\n' << std::flush;

    if (!std::cout)
        return 1;

    const auto publisher = (*endpoint)->getPublisher();

    q::kraken::KrakenSubscription source(std::move(settings.source));
    using enum q::kraken::SubscriptionPhase;
    bool started = false;
    bool hadDemand = false;
    bool failed = false;
    std::uint64_t observed = 0;
    std::uint64_t published = 0;
    std::optional<std::uint64_t> reportedFailure;
    std::optional<q::kraken::SourceObservation> current;

    while (!interrupted.load(std::memory_order_relaxed) && Clock::now() < deadline) {
        if (const auto diagnostic = (*endpoint)->getDiagnostic(); diagnostic.code != q::api::ErrorCode::NONE) {
            report(diagnostic);
            failed = true;

            break;
        }

        const auto turnEnd = std::min(Clock::now() + OWNER_TURN, deadline);

        if (!started) {
            started = (*endpoint)->awaitDemand(SYMBOL, true, turnEnd);

            if (!started)
                continue;
        }

        source.runOneFor(std::max(0ms, std::chrono::duration_cast<std::chrono::milliseconds>(turnEnd - Clock::now())));

        if (const auto &diagnostic = source.diagnostic(); diagnostic && reportedFailure != diagnostic->generation) {
            reportSource(*diagnostic);
            reportedFailure = diagnostic->generation;
        }

        if (source.phase() == FAILED) {
            failed = true;

            break;
        }

        const bool demand = (*endpoint)->awaitDemand(SYMBOL, true, Clock::now());
        bool changed = false;

        while (auto next = source.takeObservation()) {
            ++observed;
            current = std::move(next);
            changed = true;
        }

        if (source.phase() != ACTIVE)
            current.reset();

        if (!interrupted.load(std::memory_order_relaxed) && Clock::now() < deadline && demand && current &&
            (changed || !hadDemand)) {
            const auto &value = current->observation;
            const auto &quote = value.quote;

            if (auto result = publisher.publish(quote); !result) {
                report(result.error());
                failed = true;

                break;
            }

            ++published;

            if (settings.trace) {
                std::cout << "PUBLISH " << current->generation << ' ' << timestampToken(value.timestamp) << ' '
                          << quote.bidPrice.raw << ' ' << quote.askPrice.raw << ' ' << quote.bidSize.raw << ' '
                          << quote.askSize.raw << '\n'
                          << std::flush;

                if (!std::cout) {
                    failed = true;

                    break;
                }
            }
        }

        hadDemand = demand;
    }

    source.requestStop();

    while (source.phase() != STOPPED && source.phase() != FAILED)
        source.runOneFor(OWNER_TURN);

    (*endpoint)->closeAndAwaitTermination();
    const auto statistics = (*endpoint)->getStatistics();
    const auto &ticker = statistics.families.front();
    std::cout << "STATS observed=" << observed << " published=" << published
              << " sent_frames=" << ticker.sentFrames.value << " retired_peers=" << statistics.retiredPeers.value
              << " pending_publications=" << statistics.pendingPublications << '\n'
              << std::flush;

    if (interrupted.load(std::memory_order_relaxed))
        return 130;

    return failed || !published || !std::cout ? 1 : 0;
}
} // namespace

int main(int argc, char **argv) {
    const std::span arguments(argv + 1, static_cast<std::size_t>(argc - 1));

    if (arguments.size() == 1) {
        const std::string_view option = arguments.front();

        if (option == "--help" || option == "--help-ru") {
            std::cout << (option == "--help" ? USAGE : USAGE_RU) << std::flush;

            return std::cout ? 0 : 1;
        }
    }

    try {
        auto settings = readSettings(arguments);

        if (settings)
            return run(std::move(*settings));

        std::cerr << "Invalid arguments. Try --help.\n";

        return 2;
    } catch (const q::api::Error &error) {
        report(error.diagnostic());
    } catch (const std::exception &) {
        std::cerr << "Cannot complete the gateway run.\n";
    }

    return 1;
}
