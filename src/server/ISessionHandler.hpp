#pragma once

#include <botan/asio_stream.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

using AwaitableTcp = boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::ip::tcp::socket>;
using TlsStream = Botan::TLS::Stream<AwaitableTcp>;

/// Owns one accepted TCP connection from start to finish.
class ISessionHandler {
   public:
      virtual ~ISessionHandler() = default;

      virtual boost::asio::awaitable<void> handle(AwaitableTcp socket) = 0;
};

/// Speaks the application protocol on an established TLS stream.
class IProtocolHandler {
   public:
      virtual ~IProtocolHandler() = default;

      virtual boost::asio::awaitable<void> serve(TlsStream& stream) = 0;
};
