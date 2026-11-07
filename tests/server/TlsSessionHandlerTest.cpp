#include "server/TlsSessionHandler.hpp"

#include "mocks/MockLogger.hpp"
#include "mocks/MockProtocolHandler.hpp"
#include "server/ServerOptions.hpp"
#include "server/TlsContextFactory.hpp"
#include "support/TlsLoopback.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>

using namespace std::chrono_literals;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace net = boost::asio;

class TlsSessionHandlerTest : public ::testing::Test {
   protected:
      void SetUp() override {
         ServerOptions options;
         options.cert = test_support::fixture("server.pem");
         options.key = test_support::fixture("server.key");
         options.policy = test_support::fixture("../../policies/tls13_pqc.txt");
         handler = std::make_shared<TlsSessionHandler>(create_tls_context(options), protocol, logger);
         server = std::make_unique<test_support::LoopbackServer>(handler, logger);
      }

      std::shared_ptr<NiceMock<MockProtocolHandler>> protocol = std::make_shared<NiceMock<MockProtocolHandler>>();
      std::shared_ptr<NiceMock<MockLogger>> logger = std::make_shared<NiceMock<MockLogger>>();
      std::shared_ptr<TlsSessionHandler> handler;
      std::unique_ptr<test_support::LoopbackServer> server;
};

TEST_F(TlsSessionHandlerTest, HandsTheStreamToTheProtocolHandlerAfterTheHandshake) {
   std::promise<void> served;
   EXPECT_CALL(*protocol, serve(_)).WillOnce(Invoke([&](TlsStream&) -> net::awaitable<void> {
      served.set_value();
      co_return;
   }));

   test_support::TlsTestClient client(server->port());
   client.handshake();

   EXPECT_EQ(served.get_future().wait_for(5s), std::future_status::ready);
   EXPECT_EQ(client.negotiated().version, "TLS v1.3");
}

TEST_F(TlsSessionHandlerTest, ClosesTheTlsSessionWhenTheProtocolHandlerReturns) {
   EXPECT_CALL(*protocol, serve(_)).WillOnce(Invoke([](TlsStream&) -> net::awaitable<void> { co_return; }));

   test_support::TlsTestClient client(server->port());
   client.handshake();

   EXPECT_TRUE(client.reaches_end_of_stream());
}

TEST_F(TlsSessionHandlerTest, ReportsWhatWasNegotiated) {
   std::promise<void> served;
   EXPECT_CALL(*logger, info(HasSubstr("TLS established"))).Times(1);
   EXPECT_CALL(*protocol, serve(_)).WillOnce(Invoke([&](TlsStream&) -> net::awaitable<void> {
      served.set_value();
      co_return;
   }));

   test_support::TlsTestClient client(server->port());
   client.handshake();

   EXPECT_EQ(served.get_future().wait_for(5s), std::future_status::ready);
}

TEST_F(TlsSessionHandlerTest, ClientThatOnlySpeaksTls12IsNeverServed) {
   EXPECT_CALL(*protocol, serve(_)).Times(0);
   EXPECT_CALL(*logger, error(_, _)).Times(::testing::AtMost(1));

   test_support::TlsTestClient client(server->port(), test_support::kTls12OnlyClient);

   EXPECT_THROW(client.handshake(), boost::system::system_error);
}
