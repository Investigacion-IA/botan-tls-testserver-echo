#pragma once

#include "common/ILogger.hpp"

#include <gmock/gmock.h>

class MockLogger : public ILogger {
   public:
      MOCK_METHOD(void, info, (const std::string& message), (override));
      MOCK_METHOD(void, error, (const std::string& context, const std::string& message), (override));
};
