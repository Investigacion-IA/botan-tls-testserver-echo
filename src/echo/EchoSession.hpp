#pragma once

#include "common/NetErrors.hpp"
#include "common/StreamCancel.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

/**
 * Raw echo: every byte read from the stream is written back unchanged, with no framing.
 *
 * Returns the number of bytes echoed when the peer closes the stream, disconnects, or stays silent for
 * longer than `idle_timeout` (zero or negative disables the timeout). Other I/O errors are thrown.
 */
template <typename AsyncStream>
boost::asio::awaitable<std::uint64_t> serve_echo(AsyncStream& stream, std::chrono::milliseconds idle_timeout) {
   auto executor = co_await boost::asio::this_coro::executor;

   struct Idle {
         std::atomic<bool> alive{true};
         std::atomic<std::uint64_t> epoch{0};
   };

   auto idle = std::make_shared<Idle>();
   boost::asio::steady_timer idle_timer(executor);
   auto cancel_stream = [&stream] { cancel_pending_io(stream); };

   auto arm_idle = [&] {
      if(idle_timeout.count() <= 0) {
         return;
      }
      const auto epoch = idle->epoch.fetch_add(1) + 1;
      idle_timer.expires_after(idle_timeout);
      idle_timer.async_wait([idle, epoch, cancel_stream](boost::system::error_code ec) {
         if(ec || !idle->alive.load() || idle->epoch.load() != epoch) {
            return;
         }
         cancel_stream();
      });
   };

   auto disarm_idle = [&] {
      idle->alive.store(false);
      idle_timer.cancel();
   };

   std::uint64_t echoed = 0;
   try {
      for(;;) {
         arm_idle();

         std::array<std::uint8_t, 16384> buffer{};
         boost::system::error_code ec;
         const std::size_t n = co_await stream.async_read_some(
            boost::asio::buffer(buffer), boost::asio::redirect_error(boost::asio::use_awaitable, ec));

         if(n > 0) {
            // The peer is alive: do not let the timer fire while we write the answer back.
            idle->epoch.fetch_add(1);
            idle_timer.cancel();

            boost::system::error_code write_ec;
            co_await boost::asio::async_write(stream,
                                              boost::asio::buffer(buffer.data(), n),
                                              boost::asio::redirect_error(boost::asio::use_awaitable, write_ec));
            if(is_benign_disconnect(write_ec) || is_tls_channel_closed(write_ec)) {
               break;
            }
            if(write_ec) {
               throw boost::system::system_error(write_ec);
            }
            echoed += n;
         }

         if(is_benign_disconnect(ec) || (n == 0 && !ec)) {
            break;
         }
         if(ec) {
            throw boost::system::system_error(ec);
         }
      }
   } catch(...) {
      disarm_idle();
      throw;
   }
   disarm_idle();
   co_return echoed;
}
