#pragma once

#include "common/ILogger.hpp"
#include "server/ISessionHandler.hpp"

#include <botan/asio_context.h>

#include <memory>

/// Runs the TLS 1.3 handshake on an accepted socket, hands the stream to the protocol handler,
/// then closes the TLS session in an orderly way.
class TlsSessionHandler final : public ISessionHandler {
   public:
      TlsSessionHandler(std::shared_ptr<Botan::TLS::Context> tls_context,
                        std::shared_ptr<IProtocolHandler> protocol,
                        std::shared_ptr<ILogger> logger);

      boost::asio::awaitable<void> handle(AwaitableTcp socket) override;

   private:
      std::shared_ptr<Botan::TLS::Context> tls_context_;
      std::shared_ptr<IProtocolHandler> protocol_;
      std::shared_ptr<ILogger> logger_;
};
