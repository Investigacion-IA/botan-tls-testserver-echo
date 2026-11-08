#pragma once

#include "common/ILogger.hpp"
#include "common/NetErrors.hpp"
#include "server/ISessionHandler.hpp"

#include <botan/asio_context.h>
#include <botan/asio_stream.h>
#include <botan/auto_rng.h>
#include <botan/certstor.h>
#include <botan/credentials_manager.h>
#include <botan/data_src.h>
#include <botan/pkcs8.h>
#include <botan/tls_policy.h>
#include <botan/tls_session.h>
#include <botan/tls_session_manager_noop.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace test_support {

inline std::string fixture(const std::string& name) {
   return std::string(TEST_FIXTURE_DIR) + "/" + name;
}

/// A TLS server on 127.0.0.1 and an ephemeral port that gives every accepted connection to the handler.
class LoopbackServer {
   public:
      LoopbackServer(std::shared_ptr<ISessionHandler> handler, std::shared_ptr<ILogger> logger) :
            acceptor_(io_, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {
         boost::asio::co_spawn(io_, accept_loop(std::move(handler), std::move(logger)), boost::asio::detached);
         thread_ = std::jthread([this] { io_.run(); });
      }

      ~LoopbackServer() { io_.stop(); }

      std::uint16_t port() const { return acceptor_.local_endpoint().port(); }

   private:
      boost::asio::awaitable<void> accept_loop(std::shared_ptr<ISessionHandler> handler,
                                               std::shared_ptr<ILogger> logger) {
         for(;;) {
            AwaitableTcp socket(co_await acceptor_.async_accept(boost::asio::use_awaitable));
            boost::asio::co_spawn(
               io_, handler->handle(std::move(socket)), make_completion_handler(logger, "Session"));
         }
      }

      boost::asio::io_context io_;
      boost::asio::ip::tcp::acceptor acceptor_;
      std::jthread thread_;
};

/// Trusts the test CA and optionally presents the test client certificate.
class ClientCredentials final : public Botan::Credentials_Manager {
   public:
      ClientCredentials(const std::string& ca_pem, bool with_client_certificate) :
            ca_(Botan::X509_Certificate(ca_pem)) {
         if(with_client_certificate) {
            Botan::DataSource_Stream crt(fixture("client.pem"));
            chain_.push_back(Botan::X509_Certificate(crt));
            Botan::DataSource_Stream key(fixture("client.key"));
            key_ = Botan::PKCS8::load_key(key);
         }
      }

      std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(const std::string&,
                                                                             const std::string&) override {
         return {&ca_};
      }

      std::vector<Botan::X509_Certificate> find_cert_chain(const std::vector<std::string>& key_types,
                                                           const std::vector<Botan::AlgorithmIdentifier>&,
                                                           const std::vector<Botan::X509_DN>&,
                                                           const std::string&,
                                                           const std::string&) override {
         if(!key_) {
            return {};
         }
         for(const auto& type : key_types) {
            if(type == key_->algo_name()) {
               return chain_;
            }
         }
         return key_types.empty() ? chain_ : std::vector<Botan::X509_Certificate>{};
      }

      std::shared_ptr<Botan::Private_Key> private_key_for(const Botan::X509_Certificate&,
                                                          const std::string&,
                                                          const std::string&) override {
         return key_;
      }

   private:
      Botan::Certificate_Store_In_Memory ca_;
      std::vector<Botan::X509_Certificate> chain_;
      std::shared_ptr<Botan::Private_Key> key_;
};

/// Records what the client side of the handshake negotiated.
class ClientCallbacks final : public Botan::TLS::StreamCallbacks {
   public:
      void tls_session_established(const Botan::TLS::Session_Summary& session) override {
         version = session.version().to_string();
         kex = session.kex_parameters().value_or(session.kex_algo());
      }

      std::string version;
      std::string kex;
};

constexpr const char* kTls13Client = "allow_tls12 = false\nallow_tls13 = true\nrequire_cert_revocation_info = false\n";
constexpr const char* kTls12OnlyClient = "allow_tls12 = true\nallow_tls13 = false\nrequire_cert_revocation_info = false\n";

/// A blocking TLS client for the loopback server.
class TlsTestClient {
   public:
      TlsTestClient(std::uint16_t port,
                    const std::string& policy_text = kTls13Client,
                    bool with_client_certificate = false) :
            callbacks_(std::make_shared<ClientCallbacks>()),
            context_(std::make_shared<Botan::TLS::Context>(
               std::make_shared<ClientCredentials>(fixture("ca.pem"), with_client_certificate),
               std::make_shared<Botan::AutoSeeded_RNG>(),
               std::make_shared<Botan::TLS::Session_Manager_Noop>(),
               std::make_shared<Botan::TLS::Text_Policy>(policy_text),
               Botan::TLS::Server_Information("localhost", port))),
            stream_(context_, std::shared_ptr<Botan::TLS::StreamCallbacks>(callbacks_), io_) {
         stream_.next_layer().connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));
      }

      /// Throws boost::system::system_error when the server refuses the handshake.
      void handshake() { stream_.handshake(Botan::TLS::Connection_Side::Client); }

      void write(const std::string& data) { boost::asio::write(stream_, boost::asio::buffer(data)); }

      std::string read_exactly(std::size_t size) {
         std::string data(size, '\0');
         boost::asio::read(stream_, boost::asio::buffer(data));
         return data;
      }

      /// Sends close_notify and then closes the TCP connection, like a well-behaved client.
      void close_cleanly() {
         boost::system::error_code ec;
         stream_.shutdown(ec);
         stream_.next_layer().close(ec);
      }

      /// Drops the TCP connection without close_notify.
      void drop() {
         boost::system::error_code ec;
         stream_.next_layer().close(ec);
      }

      /// True when the stream ends (close_notify or disconnect) without delivering more data.
      bool reaches_end_of_stream() {
         char byte = 0;
         boost::system::error_code ec;
         const std::size_t n = stream_.read_some(boost::asio::buffer(&byte, 1), ec);
         return n == 0 && ec;
      }

      const ClientCallbacks& negotiated() const { return *callbacks_; }

   private:
      boost::asio::io_context io_;
      std::shared_ptr<ClientCallbacks> callbacks_;
      std::shared_ptr<Botan::TLS::Context> context_;
      Botan::TLS::Stream<boost::asio::ip::tcp::socket> stream_;
};

}  // namespace test_support
