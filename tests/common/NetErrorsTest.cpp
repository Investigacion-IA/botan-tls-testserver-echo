#include "common/NetErrors.hpp"

#include "mocks/MockLogger.hpp"

#include <botan/asio_error.h>

#include <boost/asio/error.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

using ::testing::StrEq;

TEST(NetErrorsTest, DisconnectsAreBenign) {
   namespace error = boost::asio::error;
   EXPECT_TRUE(is_benign_disconnect(error::eof));
   EXPECT_TRUE(is_benign_disconnect(error::connection_reset));
   EXPECT_TRUE(is_benign_disconnect(error::operation_aborted));
   EXPECT_TRUE(is_benign_disconnect(error::broken_pipe));
   EXPECT_TRUE(is_benign_disconnect(error::not_connected));
}

TEST(NetErrorsTest, TlsStreamTruncatedIsBenign) {
   EXPECT_TRUE(is_benign_disconnect(Botan::TLS::StreamTruncated));
}

TEST(NetErrorsTest, ClosedTlsChannelIsRecognised) {
   EXPECT_TRUE(is_tls_channel_closed(Botan::ErrorType::InvalidObjectState));
   EXPECT_FALSE(is_tls_channel_closed(boost::asio::error::eof));
   EXPECT_FALSE(is_tls_channel_closed(boost::system::error_code{}));
}

TEST(NetErrorsTest, TlsAlertsAreNotBenign) {
   EXPECT_FALSE(is_benign_disconnect(Botan::TLS::Alert::ProtocolVersion));
}

TEST(NetErrorsTest, OtherErrorsAreNotBenign) {
   EXPECT_FALSE(is_benign_disconnect(boost::system::error_code{}));
   EXPECT_FALSE(is_benign_disconnect(boost::asio::error::access_denied));
   EXPECT_FALSE(is_benign_disconnect(boost::asio::error::timed_out));
}

TEST(NetErrorsTest, LogExceptionReportsContextAndMessage) {
   MockLogger logger;
   EXPECT_CALL(logger, error(StrEq("Session"), StrEq("boom")));

   log_exception(logger, "Session", std::make_exception_ptr(std::runtime_error("boom")));
}

TEST(NetErrorsTest, LogExceptionIgnoresNullPointer) {
   MockLogger logger;
   EXPECT_CALL(logger, error(::testing::_, ::testing::_)).Times(0);

   log_exception(logger, "Session", nullptr);
}

TEST(NetErrorsTest, LogExceptionHandlesNonStdExceptions) {
   MockLogger logger;
   EXPECT_CALL(logger, error(StrEq("Acceptor"), StrEq("unknown exception")));

   log_exception(logger, "Acceptor", std::make_exception_ptr(42));
}

TEST(NetErrorsTest, CompletionHandlerForwardsToLogger) {
   auto logger = std::make_shared<MockLogger>();
   EXPECT_CALL(*logger, error(StrEq("Acceptor"), StrEq("bad")));

   const auto handler = make_completion_handler(logger, "Acceptor");
   handler(std::make_exception_ptr(std::runtime_error("bad")));
   handler(nullptr);
}
