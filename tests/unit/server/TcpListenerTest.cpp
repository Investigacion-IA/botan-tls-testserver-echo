#include "server/TcpListener.hpp"

#include "mocks/MockLogger.hpp"
#include "mocks/MockProtocolHandler.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

namespace {

std::uint16_t free_port() {
   net::io_context io;
   tcp::acceptor acceptor(io, tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
   return acceptor.local_endpoint().port();
}

}  // namespace

TEST(TcpListenerTest, EveryAcceptedConnectionIsHandedToTheHandler) {
   auto handler = std::make_shared<::testing::NiceMock<MockSessionHandler>>();
   auto logger = std::make_shared<::testing::NiceMock<MockLogger>>();
   std::atomic<int> handled{0};
   EXPECT_CALL(*handler, handle(::testing::_)).Times(2).WillRepeatedly(::testing::Invoke([&](AwaitableTcp) {
      return [&]() -> net::awaitable<void> {
         ++handled;
         co_return;
      }();
   }));

   const auto port = free_port();
   net::io_context io;
   net::co_spawn(io,
                 run_tcp_listener(tcp::endpoint(net::ip::make_address("127.0.0.1"), port), handler, logger, "Session"),
                 net::detached);
   std::jthread thread([&io] { io.run(); });

   net::io_context client_io;
   for(int i = 0; i < 2; ++i) {
      tcp::socket client(client_io);
      boost::system::error_code ec = net::error::connection_refused;
      for(int attempt = 0; attempt < 100 && ec; ++attempt) {
         client.connect(tcp::endpoint(net::ip::make_address("127.0.0.1"), port), ec);
         if(ec) {
            std::this_thread::sleep_for(20ms);
         }
      }
      ASSERT_FALSE(ec) << ec.message();
   }

   for(int attempt = 0; attempt < 200 && handled < 2; ++attempt) {
      std::this_thread::sleep_for(10ms);
   }
   io.stop();
   EXPECT_EQ(handled, 2);
}
