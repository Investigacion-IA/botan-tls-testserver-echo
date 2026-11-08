#pragma once

#include "server/ISessionHandler.hpp"

#include <gmock/gmock.h>

class MockProtocolHandler : public IProtocolHandler {
   public:
      MOCK_METHOD(boost::asio::awaitable<void>, serve, (TlsStream & stream), (override));
};

class MockSessionHandler : public ISessionHandler {
   public:
      MOCK_METHOD(boost::asio::awaitable<void>, handle, (AwaitableTcp socket), (override));
};
