#include "server/ServerApp.hpp"

#include "common/NetErrors.hpp"
#include "pkcs11/Pkcs11Module.hpp"
#include "server/EchoProtocolHandler.hpp"
#include "server/TcpListener.hpp"
#include "server/TlsContextFactory.hpp"
#include "server/TlsSessionHandler.hpp"

#include <botan/asio_context.h>
#include <botan/version.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace net = boost::asio;
using tcp = net::ip::tcp;

ServerApp::ServerApp(ServerOptions options, std::shared_ptr<ILogger> logger) :
      options_(std::move(options)), logger_(std::move(logger)) {}

int ServerApp::run() {
   try {
      std::unique_ptr<Botan::PKCS11::Module> pkcs11;
      if(!options_.pkcs11_module.empty()) {
         pkcs11 = initialize_pkcs11_module(options_.pkcs11_module);
         logger_->info("PKCS#11 module " + options_.pkcs11_module + " initialized");
      }

      auto tls_context = create_tls_context(options_);

      auto echo = std::make_shared<EchoProtocolHandler>(std::chrono::seconds(options_.idle_timeout_seconds), logger_);
      auto sessions = std::make_shared<TlsSessionHandler>(std::move(tls_context), std::move(echo), logger_);

      const auto num_threads = std::max(1u, std::thread::hardware_concurrency());
      net::io_context io{static_cast<int>(num_threads)};

      net::co_spawn(io,
                    run_tcp_listener(tcp::endpoint{net::ip::make_address("0.0.0.0"), options_.port},
                                     std::move(sessions),
                                     logger_,
                                     "Session"),
                    make_completion_handler(logger_, "Acceptor"));

      logger_->info("TLS 1.3 echo server (Botan " + std::string(Botan::short_version_string()) + ") listening on port " +
                    std::to_string(options_.port) +
                    (options_.require_client_certificate() ? ", client certificates required" : ""));

      std::vector<std::jthread> threads;
      for(size_t i = 2; i <= num_threads; ++i) {
         threads.emplace_back([&io]() { io.run(); });
      }

      io.run();
   } catch(const std::exception& ex) {
      logger_->error("Server", ex.what());
      return 1;
   }

   return 0;
}
