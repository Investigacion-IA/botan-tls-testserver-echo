#pragma once

#include "common/ILogger.hpp"

#include <mutex>

/// info goes to stdout, error to stderr prefixed with a timestamp.
class StderrLogger final : public ILogger {
   public:
      void info(const std::string& message) override;

      void error(const std::string& context, const std::string& message) override;

   private:
      std::mutex mutex_;
};
