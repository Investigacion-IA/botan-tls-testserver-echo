#include "echo/EchoSession.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <exception>
#include <future>
#include <string>
#include <thread>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::chrono_literals;

namespace {

/// Both ends of a loopback TCP connection.
struct SocketPair {
      explicit SocketPair(net::io_context& io) : client(io), server(io) {
         tcp::acceptor acceptor(io, tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
         client.connect(acceptor.local_endpoint());
         acceptor.accept(server);
      }

      tcp::socket client;
      tcp::socket server;
};

struct EchoOutcome {
      std::uint64_t echoed = 0;
      std::exception_ptr failure;
};

}  // namespace

class EchoSessionTest : public ::testing::Test {
   protected:
      /// Starts the echo on the server socket; the io_context runs on its own thread.
      std::future<EchoOutcome> start_echo(tcp::socket& server, std::chrono::milliseconds idle_timeout) {
         auto outcome = std::make_shared<std::promise<EchoOutcome>>();
         auto future = outcome->get_future();
         net::co_spawn(io, serve_echo(server, idle_timeout), [outcome](std::exception_ptr e, std::uint64_t n) {
            outcome->set_value(EchoOutcome{n, e});
         });
         thread = std::jthread([this] { io.run(); });
         return future;
      }

      void TearDown() override { io.stop(); }

      EchoOutcome finish(std::future<EchoOutcome>& future) {
         EXPECT_EQ(future.wait_for(5s), std::future_status::ready);
         return future.get();
      }

      net::io_context io;
      std::jthread thread;
};

TEST_F(EchoSessionTest, EchoesTheBytesBackUnchanged) {
   SocketPair sockets(io);
   auto result = start_echo(sockets.server, 0ms);

   const std::string message("hello\0echo\xff", 11);
   net::write(sockets.client, net::buffer(message));
   std::string reply(message.size(), '\0');
   net::read(sockets.client, net::buffer(reply));
   sockets.client.shutdown(tcp::socket::shutdown_send);

   EXPECT_EQ(reply, message);
   const EchoOutcome outcome = finish(result);
   EXPECT_FALSE(outcome.failure);
   EXPECT_EQ(outcome.echoed, message.size());
}

TEST_F(EchoSessionTest, EchoesALargeStreamInOrder) {
   SocketPair sockets(io);
   auto result = start_echo(sockets.server, 0ms);

   constexpr std::size_t kChunk = 64 * 1024;
   constexpr std::size_t kChunks = 16;  // 1 MiB
   std::string chunk(kChunk, '\0');
   std::string reply(kChunk, '\0');
   for(std::size_t i = 0; i < kChunks; ++i) {
      for(std::size_t j = 0; j < kChunk; ++j) {
         chunk[j] = static_cast<char>((i * 31 + j) & 0xff);
      }
      net::write(sockets.client, net::buffer(chunk));
      net::read(sockets.client, net::buffer(reply));
      ASSERT_EQ(reply, chunk) << "chunk " << i;
   }
   sockets.client.shutdown(tcp::socket::shutdown_send);

   const EchoOutcome outcome = finish(result);
   EXPECT_FALSE(outcome.failure);
   EXPECT_EQ(outcome.echoed, kChunk * kChunks);
}

TEST_F(EchoSessionTest, EndsWhenThePeerClosesWithoutSendingAnything) {
   SocketPair sockets(io);
   sockets.client.shutdown(tcp::socket::shutdown_send);

   auto result = start_echo(sockets.server, 0ms);

   const EchoOutcome outcome = finish(result);
   EXPECT_FALSE(outcome.failure);
   EXPECT_EQ(outcome.echoed, 0u);
}

TEST_F(EchoSessionTest, AbruptDisconnectIsNotAnError) {
   SocketPair sockets(io);
   sockets.client.set_option(tcp::socket::linger(true, 0));
   sockets.client.close();

   auto result = start_echo(sockets.server, 0ms);

   EXPECT_FALSE(finish(result).failure);
}

TEST_F(EchoSessionTest, SilentConnectionIsClosedAfterTheIdleTimeout) {
   SocketPair sockets(io);
   const auto start = std::chrono::steady_clock::now();
   auto result = start_echo(sockets.server, 100ms);

   const EchoOutcome outcome = finish(result);

   EXPECT_FALSE(outcome.failure);
   EXPECT_EQ(outcome.echoed, 0u);
   EXPECT_GE(std::chrono::steady_clock::now() - start, 80ms);
}

TEST_F(EchoSessionTest, ActivityKeepsTheConnectionAlive) {
   SocketPair sockets(io);
   auto result = start_echo(sockets.server, 150ms);

   // Total time (500 ms) is far above the idle timeout, but no gap is.
   std::string reply(1, '\0');
   for(int i = 0; i < 10; ++i) {
      net::write(sockets.client, net::buffer(std::string(1, static_cast<char>('a' + i))));
      net::read(sockets.client, net::buffer(reply));
      EXPECT_EQ(reply[0], static_cast<char>('a' + i));
      std::this_thread::sleep_for(50ms);
   }
   sockets.client.shutdown(tcp::socket::shutdown_send);

   const EchoOutcome outcome = finish(result);
   EXPECT_FALSE(outcome.failure);
   EXPECT_EQ(outcome.echoed, 10u);
}

TEST_F(EchoSessionTest, ZeroTimeoutNeverClosesAQuietConnection) {
   SocketPair sockets(io);
   auto result = start_echo(sockets.server, 0ms);

   std::this_thread::sleep_for(200ms);
   EXPECT_NE(result.wait_for(0ms), std::future_status::ready);

   net::write(sockets.client, net::buffer(std::string("late")));
   std::string reply(4, '\0');
   net::read(sockets.client, net::buffer(reply));
   sockets.client.shutdown(tcp::socket::shutdown_send);

   EXPECT_EQ(reply, "late");
   EXPECT_EQ(finish(result).echoed, 4u);
}
