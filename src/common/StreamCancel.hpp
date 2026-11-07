#pragma once

#include <boost/system/error_code.hpp>

/// Cancels pending async operations on a socket or on a TLS stream wrapping one.
template <typename AsyncStream>
void cancel_pending_io(AsyncStream& stream) {
   boost::system::error_code ignore;
   if constexpr(requires(AsyncStream& s, boost::system::error_code ec) { s.next_layer().cancel(ec); }) {
      stream.next_layer().cancel(ignore);
   } else if constexpr(requires(AsyncStream& s, boost::system::error_code ec) { s.cancel(ec); }) {
      stream.cancel(ignore);
   }
}
