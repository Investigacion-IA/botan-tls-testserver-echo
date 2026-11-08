// The whole stack: Boost.Asio TCP, Botan TLS 1.3, the echo protocol; talked to by a real TLS client.

#include "mocks/MockLogger.hpp"
#include "server/EchoProtocolHandler.hpp"
#include "server/ServerOptions.hpp"
#include "server/TlsContextFactory.hpp"
#include "server/TlsSessionHandler.hpp"
#include "support/TlsLoopback.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <string>

using test_support::fixture;
using test_support::kTls12OnlyClient;
using test_support::kTls13Client;
using test_support::TlsTestClient;

namespace {

ServerOptions server_options(bool mutual_tls) {
   ServerOptions options;
   options.cert = fixture("server.pem");
   options.key = fixture("server.key");
   options.policy = fixture("../../policies/tls13_pqc.txt");
   if(mutual_tls) {
      options.client_ca = fixture("ca.pem");
   }
   return options;
}

}  // namespace

class TlsEchoIntegrationTest : public ::testing::Test {
   protected:
      void start(const ServerOptions& options) {
         auto echo = std::make_shared<EchoProtocolHandler>(std::chrono::seconds(10), logger);
         auto sessions = std::make_shared<TlsSessionHandler>(create_tls_context(options), echo, logger);
         server = std::make_unique<test_support::LoopbackServer>(sessions, logger);
      }

      /// Handshake, send, read back; false when the server refused at any point.
      bool echo_works(TlsTestClient& client, const std::string& message) {
         try {
            client.handshake();
            client.write(message);
            return client.read_exactly(message.size()) == message;
         } catch(const std::exception&) {
            return false;
         }
      }

      std::shared_ptr<::testing::NiceMock<MockLogger>> logger = std::make_shared<::testing::NiceMock<MockLogger>>();
      std::unique_ptr<test_support::LoopbackServer> server;
};

TEST_F(TlsEchoIntegrationTest, EchoesOverTls13) {
   start(server_options(false));
   TlsTestClient client(server->port());

   EXPECT_TRUE(echo_works(client, "ping through TLS 1.3"));
   EXPECT_EQ(client.negotiated().version, "TLS v1.3");
}

TEST_F(TlsEchoIntegrationTest, ChoosesTheHybridPostQuantumGroupWhenTheClientOffersIt) {
   start(server_options(false));
   const std::string policy = std::string(kTls13Client) + "key_exchange_groups = x25519/ML-KEM-768 x25519\n";
   TlsTestClient client(server->port(), policy);

   EXPECT_TRUE(echo_works(client, "pq"));
   EXPECT_EQ(client.negotiated().kex, "x25519/ML-KEM-768");
}

TEST_F(TlsEchoIntegrationTest, FallsBackToAClassicalGroupForAClientWithoutMlKem) {
   start(server_options(false));
   const std::string policy = std::string(kTls13Client) + "key_exchange_groups = x25519\n";
   TlsTestClient client(server->port(), policy);

   EXPECT_TRUE(echo_works(client, "classical"));
   EXPECT_EQ(client.negotiated().kex, "x25519");
}

TEST_F(TlsEchoIntegrationTest, EchoesBinaryDataAndALargeStream) {
   start(server_options(false));
   TlsTestClient client(server->port());
   client.handshake();

   const std::string binary("\x00\x01\x02\xff\xfe\x00", 6);
   client.write(binary);
   EXPECT_EQ(client.read_exactly(binary.size()), binary);

   std::string chunk(64 * 1024, '\0');
   for(int i = 0; i < 16; ++i) {  // 1 MiB
      for(std::size_t j = 0; j < chunk.size(); ++j) {
         chunk[j] = static_cast<char>((i * 7 + j) & 0xff);
      }
      client.write(chunk);
      ASSERT_EQ(client.read_exactly(chunk.size()), chunk) << "chunk " << i;
   }
}

TEST_F(TlsEchoIntegrationTest, ServesSeveralClientsAtTheSameTime) {
   start(server_options(false));
   TlsTestClient first(server->port());
   TlsTestClient second(server->port());
   first.handshake();
   second.handshake();

   first.write("one");
   second.write("two");

   EXPECT_EQ(second.read_exactly(3), "two");
   EXPECT_EQ(first.read_exactly(3), "one");
}

TEST_F(TlsEchoIntegrationTest, RejectsAClientThatOnlySpeaksTls12) {
   start(server_options(false));
   TlsTestClient client(server->port(), kTls12OnlyClient);

   EXPECT_FALSE(echo_works(client, "no way"));
}

TEST_F(TlsEchoIntegrationTest, WithoutClientCaNoClientCertificateIsNeeded) {
   start(server_options(false));
   TlsTestClient anonymous(server->port(), kTls13Client, false);
   TlsTestClient with_certificate(server->port(), kTls13Client, true);

   EXPECT_TRUE(echo_works(anonymous, "anonymous"));
   EXPECT_TRUE(echo_works(with_certificate, "certified"));
}

TEST_F(TlsEchoIntegrationTest, MutualTlsRefusesAClientWithoutCertificate) {
   start(server_options(true));
   TlsTestClient client(server->port(), kTls13Client, false);

   EXPECT_FALSE(echo_works(client, "who am I"));
}

TEST_F(TlsEchoIntegrationTest, MutualTlsAcceptsAClientWithACertificateFromTheClientCa) {
   start(server_options(true));
   TlsTestClient client(server->port(), kTls13Client, true);

   EXPECT_TRUE(echo_works(client, "known client"));
}

TEST_F(TlsEchoIntegrationTest, ServerReportsTheClientCertificateItAuthenticated) {
   EXPECT_CALL(*logger, info(::testing::_)).Times(::testing::AnyNumber());
   EXPECT_CALL(*logger, info(::testing::HasSubstr("client_cert=CN=\"client\"")))
      .Times(::testing::AtLeast(1));
   start(server_options(true));
   TlsTestClient client(server->port(), kTls13Client, true);

   ASSERT_TRUE(echo_works(client, "log me"));
}

TEST_F(TlsEchoIntegrationTest, ClientThatClosesTheTlsSessionIsNotAnError) {
   std::promise<void> finished;
   EXPECT_CALL(*logger, error(::testing::_, ::testing::_)).Times(0);
   EXPECT_CALL(*logger, info(::testing::_)).Times(::testing::AnyNumber());
   EXPECT_CALL(*logger, info(::testing::HasSubstr("echo finished: 4 bytes")))
      .WillOnce(::testing::Invoke([&](const std::string&) { finished.set_value(); }));
   start(server_options(false));
   TlsTestClient client(server->port());

   ASSERT_TRUE(echo_works(client, "ping"));
   client.close_cleanly();

   EXPECT_EQ(finished.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);
}

TEST_F(TlsEchoIntegrationTest, ClientThatJustDropsTheConnectionIsNotAnError) {
   std::promise<void> finished;
   EXPECT_CALL(*logger, error(::testing::_, ::testing::_)).Times(0);
   EXPECT_CALL(*logger, info(::testing::_)).Times(::testing::AnyNumber());
   EXPECT_CALL(*logger, info(::testing::HasSubstr("echo finished: 4 bytes")))
      .WillOnce(::testing::Invoke([&](const std::string&) { finished.set_value(); }));
   start(server_options(false));
   TlsTestClient client(server->port());

   ASSERT_TRUE(echo_works(client, "ping"));
   client.drop();

   EXPECT_EQ(finished.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);
}

TEST_F(TlsEchoIntegrationTest, MutualTlsWorksWithTheDefaultPolicy) {
   ServerOptions options = server_options(true);
   options.policy = "default";
   start(options);
   TlsTestClient client(server->port(), kTls13Client, true);

   EXPECT_TRUE(echo_works(client, "default policy"));
}

TEST_F(TlsEchoIntegrationTest, ClientThatSendsDataAndClosesTheTlsSessionAtOnceIsNotAnError) {
   std::promise<void> finished;
   EXPECT_CALL(*logger, error(::testing::_, ::testing::_)).Times(0);
   EXPECT_CALL(*logger, info(::testing::_)).Times(::testing::AnyNumber());
   EXPECT_CALL(*logger, info(::testing::HasSubstr("echo finished")))
      .WillOnce(::testing::Invoke([&](const std::string&) { finished.set_value(); }));
   start(server_options(false));
   TlsTestClient client(server->port());

   client.handshake();
   client.write("fire and forget");
   client.close_cleanly();

   EXPECT_EQ(finished.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);
}
