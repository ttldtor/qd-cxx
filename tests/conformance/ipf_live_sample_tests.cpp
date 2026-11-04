// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: argv[1] is the sample; argv[2] is test-owned capture storage.
// All children have an eight-second deadline and exact-identity RAII cleanup.
#include "connect_process_support.hpp"
#include "ipf_http_peer.hpp"
#include <iostream>
#include <qd/transport/tcp_runtime.hpp>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace process = q::connect_tests::process;
using process::require;

/** Run a bounded independent public sample, then inspect only its retired output.
 * @param executable Exact sample path.
 * @param root Test-owned capture directory.
 * @param name Unique case name.
 * @param arguments Explicit finite sample policy.
 * @param expected Required exit.
 * @return Exact bounded stdout bytes.
 */
std::string run(const std::filesystem::path &executable, const std::filesystem::path &root, std::string_view name,
                const std::vector<std::string> &arguments, int expected) {
    const auto output = root / (std::string(name) + ".out");
    const auto error = root / (std::string(name) + ".err");
    process::Child child(executable, arguments, output, error);
    require(child.wait() == expected, "sample exit");
    require(process::read(error).empty(), "sample stderr");
    auto text = process::read(output);

    for (auto position = text.find("\r\n"); position != text.npos; position = text.find("\r\n", position)) {
        text.erase(position, 1);
    }

    return text;
}

/** Numeric loopback source, without DNS or public credentials.
 * @param port Owned peer.
 * @return Controlled URL.
 */
std::string source(std::uint16_t port) {
    return "http://127.0.0.1:" + std::to_string(port) + "/profiles.ipf";
}
} // namespace

/** Qualify help before SDK preparation and bounded real child shutdown.
 * @param argc Internal two-input argument count.
 * @param argv Exact sample and owned capture paths.
 * @return Zero for all observations, one for a concrete failure.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "internal runner requires sample and capture paths");
        const std::filesystem::path executable(argv[1]), root(argv[2]);
        std::filesystem::create_directories(root);
        const auto defaultHelp = run(executable, root, "noargs", {}, 0);
        require(defaultHelp == run(executable, root, "help", {"--help"}, 0), "noargs equals English help");
        require(defaultHelp.find("--duration-ms") != defaultHelp.npos, "complete English help");
        const auto russian = run(executable, root, "help-ru", {"--help-ru"}, 0);
        require(russian.find("--max-updates") != russian.npos && russian != defaultHelp, "translated input-free help");
        {
            const auto output = root / "invalid.out", error = root / "invalid.err";
            process::Child child(executable,
                                 {"--run", "--source", "http://secret:password@invalid/", "--duration-ms", "0"}, output,
                                 error);
            require(child.wait() == 2, "invalid policy exit before source I/O");
            require(process::read(output).empty(), "invalid policy stdout");
            const auto diagnostic = process::read(error);
            require(diagnostic.find("secret") == diagnostic.npos && diagnostic.find("password") == diagnostic.npos,
                    "argument failure redaction");
        }
        auto bootstrap = q::transport::RuntimeBootstrap::create();
        require(static_cast<bool>(bootstrap), "test loopback bootstrap");
        constexpr std::string_view headers = "HTTP/1.1 200 OK\r\nX-Live: provided\r\nContent-Length: 9999\r\n\r\n";
        {
            q::test::IpfHttpPeer peer({std::string(headers) + "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\n"}, 4096,
                                      true);
            const auto output = run(executable, root, "complete",
                                    {"--run", "--source", source(peer.port()), "--duration-ms", "1000"}, 0);
            require(output.find("\tSTOCK\tA\t0\n") != output.npos, "owning complete revision");
            require(output.find("initial_snapshot=complete retired=1") != output.npos, "snapshot and retirement");
            require(output.find("http_finished=0") != output.npos, "held HTTP is not fabricated complete");
            require(peer.accepted() == 1, "one controlled request");
        }
        {
            q::test::IpfHttpPeer peer({std::string(headers) + "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##\n"}, 4096, true);
            const auto output = run(executable, root, "partial",
                                    {"--run", "--source", source(peer.port()), "--duration-ms", "1000"}, 3);
            require(output.find("\tSTOCK\tA\t0\n") != output.npos, "partial owning prefix");
            require(output.find("initial_snapshot=incomplete retired=1") != output.npos, "no fabricated snapshot");
        }
        {
            q::test::IpfHttpPeer peer(
                {std::string(headers) + "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\nSTOCK,B\n##\n"}, 4096, true);
            const auto output =
                run(executable, root, "record-stop",
                    {"--run", "--source", source(peer.port()), "--duration-ms", "5000", "--max-updates", "2"}, 0);
            require(output.find("summary captured=2 initial_snapshot=complete retired=1") != output.npos,
                    "record limit closes and joins before output");
        }
        std::cout << "PASS help/invalid-policy/complete/partial/record-stop; all owned children retired\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
