#include "server/EchoProtocolHandler.hpp"

#include "echo/EchoSession.hpp"

EchoProtocolHandler::EchoProtocolHandler(std::chrono::milliseconds idle_timeout, std::shared_ptr<ILogger> logger) :
      idle_timeout_(idle_timeout), logger_(std::move(logger)) {}

boost::asio::awaitable<void> EchoProtocolHandler::serve(TlsStream& stream) {
   const std::uint64_t echoed = co_await serve_echo(stream, idle_timeout_);
   logger_->info("echo finished: " + std::to_string(echoed) + " bytes");
}
