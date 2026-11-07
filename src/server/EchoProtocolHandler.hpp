#pragma once

#include "common/ILogger.hpp"
#include "server/ISessionHandler.hpp"

#include <chrono>
#include <memory>

/// The raw echo application protocol: bytes in, the same bytes out.
class EchoProtocolHandler final : public IProtocolHandler {
   public:
      EchoProtocolHandler(std::chrono::milliseconds idle_timeout, std::shared_ptr<ILogger> logger);

      boost::asio::awaitable<void> serve(TlsStream& stream) override;

   private:
      std::chrono::milliseconds idle_timeout_;
      std::shared_ptr<ILogger> logger_;
};
