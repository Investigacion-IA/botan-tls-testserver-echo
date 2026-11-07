#pragma once

#include "common/ILogger.hpp"
#include "server/ISessionHandler.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <memory>
#include <string>

/// Accepts connections forever and runs each one through the handler on its own coroutine.
boost::asio::awaitable<void> run_tcp_listener(boost::asio::ip::tcp::endpoint endpoint,
                                              std::shared_ptr<ISessionHandler> handler,
                                              std::shared_ptr<ILogger> logger,
                                              std::string session_context);
