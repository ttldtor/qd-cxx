// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace ssl = asio::ssl;
namespace ws = beast::websocket;
using Tcp = asio::ip::tcp;
using Socket = ws::stream<ssl::stream<Tcp::socket>, false>;

constexpr std::size_t REQUEST_LIMIT = 4096;

/** Verify the fixed subscription and parse its positive, exact request correlation id.
 * @param request Owned request text.
 * @return Numeric identifier to echo in the acknowledgement.
 */
std::uint64_t requestId(std::string request) {
    // Read the fixed outgoing request fields without depending on the gateway's JSON field order.
    std::erase_if(request, [](char ch) {
        return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
    });

    if (!request.contains(R"("method":"subscribe")") || !request.contains(R"("channel":"ticker")") ||
        !request.contains(R"("symbol":["BTC/USD"])") || !request.contains(R"("event_trigger":"bbo")") ||
        !request.contains(R"("snapshot":true)")) {
        throw std::runtime_error("unexpected ticker subscribe request");
    }

    constexpr std::string_view ID_KEY = R"("req_id":)";
    const auto key = request.find(ID_KEY);

    if (key == std::string::npos) {
        throw std::runtime_error("subscription has no req_id");
    }

    std::uint64_t id = 0;
    const auto [end, error] =
        std::from_chars(request.data() + key + ID_KEY.size(), request.data() + request.size(), id);

    if (error != std::errc{} || id == 0 || end == request.data() + request.size() || (*end != ',' && *end != '}')) {
        throw std::runtime_error("invalid subscription req_id");
    }

    return id;
}

/** Emit one Kraken-shaped controlled response, preserving test field spelling.
 * @param socket Upgraded WSS connection.
 * @param payload Text borrowed until the synchronous write returns.
 */
void send(Socket &socket, const std::string &payload) {
    socket.write(asio::buffer(payload));
}

/** Serve one generation until a controlled disconnect or STOP.
 * @param listener Loopback listener owned by the caller.
 * @param tls TLS fixture context owned by the caller.
 * @param previousId Latest successfully read request id, initially zero.
 * @return True if a controlled disconnect requests a new connection.
 */
bool serve(Tcp::acceptor &listener, ssl::context &tls, std::uint64_t &previousId) {
    Socket socket(listener.get_executor(), tls);
    listener.accept(beast::get_lowest_layer(socket));
    socket.next_layer().handshake(ssl::stream_base::server);
    socket.accept();
    socket.read_message_max(REQUEST_LIMIT);
    socket.text(true);
    beast::flat_buffer buffer{REQUEST_LIMIT};
    socket.read(buffer);

    if (!socket.got_text()) {
        throw std::runtime_error("binary subscribe request");
    }

    const auto received = buffer.data();
    const auto id = requestId(std::string(static_cast<const char *>(received.data()), received.size()));

    if (id <= previousId) {
        throw std::runtime_error("request id did not advance after reconnect");
    }

    previousId = id;
    std::cout << "SUBSCRIBE " << id << std::endl;

    // The supervisor fences each write, waits for the downstream result, and owns the process deadline.
    for (std::string command; std::getline(std::cin, command);) {
        if (command == "DROP") {
            boost::system::error_code ignored;
            beast::get_lowest_layer(socket).close(ignored);
            std::cout << "DROPPED" << std::endl;

            return true;
        }

        if (command == "STOP") {
            std::cout << "PEER_STOP" << std::endl;

            return false;
        }

        if (command == "ACK") {
            send(socket,
                 R"({"method":"subscribe","success":true,"req_id":)" + std::to_string(id) +
                     R"(,"result":{"channel":"ticker","event_trigger":"bbo","snapshot":true,"symbol":"BTC/USD"}})");
        } else if (command.starts_with("SEND ")) {
            send(socket, command.substr(std::string_view("SEND ").size()));
        } else {
            throw std::runtime_error("unknown fixture control command");
        }

        std::cout << "SENT" << std::endl;
    }

    throw std::runtime_error("fixture input ended before STOP");
}

/** Run a loopback TLS/WSS peer with an externally controlled two-generation schedule.
 * @param fixtures Directory containing the existing synthetic certificate and key.
 * @return Zero on STOP after two subscriptions and one controlled disconnect.
 */
int run(const std::filesystem::path &fixtures) {
    asio::io_context context;
    ssl::context tls{ssl::context::tls_server};
    tls.use_certificate_chain_file((fixtures / "server.pem").string());
    tls.use_private_key_file((fixtures / "server.key").string(), ssl::context::pem);
    Tcp::acceptor listener{context, Tcp::endpoint{asio::ip::address_v4::loopback(), 0}};
    std::cout << "PORT " << listener.local_endpoint().port() << std::endl;
    std::uint64_t previousId = 0;

    if (!serve(listener, tls, previousId) || serve(listener, tls, previousId)) {
        throw std::runtime_error("fixture expected exactly one disconnect and two subscriptions");
    }

    return std::cout ? 0 : 1;
}

} // namespace

/** Run the finite externally supervised WSS test peer, using only a supplied synthetic TLS fixture. */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: qdcxx_kraken_fixture_peer TLS_FIXTURE_DIRECTORY\n";

        return 2;
    }

    try {
        return run(argv[1]);
    } catch (const std::exception &error) {
        std::cerr << "WSS fixture: " << error.what() << '\n';

        return 1;
    }
}
