// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/version.hpp>

#if defined(ASIO_STANDALONE) || defined(ASIO_NO_DEPRECATED)
#    error The WSS dependency target must not inherit standalone Asio definitions
#endif

static_assert(BOOST_VERSION == 109200);

/** Link the three stream layers against the explicitly qualified OpenSSL SDK.
 * @return Zero when construction completes. No sockets are opened and no work is posted.
 */
int main() {
    boost::asio::io_context io;
    boost::asio::ssl::context tls{boost::asio::ssl::context::tls_client};
    using TlsStream = boost::asio::ssl::stream<boost::beast::tcp_stream>;
    boost::beast::websocket::stream<TlsStream> websocket{io, tls};

    return websocket.is_open() ? 1 : 0;
}
