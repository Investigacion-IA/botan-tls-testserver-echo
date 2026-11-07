#include "server/TlsSessionHandler.hpp"

#include "common/NetErrors.hpp"
#include "tls/SessionCallbacks.hpp"

#include <boost/asio/redirect_error.hpp>

namespace {

std::string describe_peer(const AwaitableTcp& socket) {
   boost::system::error_code ec;
   const auto endpoint = socket.remote_endpoint(ec);
   return ec ? std::string("unknown") : endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

}  // namespace

TlsSessionHandler::TlsSessionHandler(std::shared_ptr<Botan::TLS::Context> tls_context,
                                     std::shared_ptr<IProtocolHandler> protocol,
                                     std::shared_ptr<ILogger> logger) :
      tls_context_(std::move(tls_context)), protocol_(std::move(protocol)), logger_(std::move(logger)) {}

boost::asio::awaitable<void> TlsSessionHandler::handle(AwaitableTcp socket) {
   namespace net = boost::asio;
   using tcp = net::ip::tcp;

   auto callbacks = std::make_shared<SessionCallbacks>(logger_, describe_peer(socket));
   TlsStream tls_stream(std::move(socket), tls_context_, callbacks);

   try {
      co_await tls_stream.async_handshake(Botan::TLS::Connection_Side::Server);
      co_await protocol_->serve(tls_stream);
   } catch(const boost::system::system_error& se) {
      if(!is_benign_disconnect(se.code())) {
         throw;
      }
   }

   boost::system::error_code ec;
   co_await tls_stream.async_shutdown(net::redirect_error(net::use_awaitable, ec));
   tls_stream.next_layer().shutdown(tcp::socket::shutdown_send, ec);
}
