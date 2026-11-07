#include "common/NetErrors.hpp"

#include <botan/asio_error.h>

#include <boost/asio/error.hpp>

#include <utility>

bool is_benign_disconnect(const boost::system::error_code& ec) {
   namespace error = boost::asio::error;
   return ec == error::eof || ec == error::connection_reset || ec == error::operation_aborted ||
          ec == error::broken_pipe || ec == error::not_connected ||
          // TLS peer closed the TCP connection without sending close_notify
          ec == Botan::TLS::StreamTruncated;
}

bool is_tls_channel_closed(const boost::system::error_code& ec) {
   return ec == Botan::ErrorType::InvalidObjectState;
}

void log_exception(ILogger& logger, const std::string& context, std::exception_ptr error) {
   if(!error) {
      return;
   }
   try {
      std::rethrow_exception(std::move(error));
   } catch(const std::exception& ex) {
      logger.error(context, ex.what());
   } catch(...) {
      logger.error(context, "unknown exception");
   }
}

std::function<void(std::exception_ptr)> make_completion_handler(std::shared_ptr<ILogger> logger, std::string context) {
   return [logger = std::move(logger), context = std::move(context)](std::exception_ptr error) {
      log_exception(*logger, context, std::move(error));
   };
}
