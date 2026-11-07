#pragma once

#include "common/ILogger.hpp"
#include "server/ServerOptions.hpp"

#include <memory>

/// Wires everything together and runs the TLS 1.3 echo listener until it stops.
class ServerApp {
   public:
      ServerApp(ServerOptions options, std::shared_ptr<ILogger> logger);

      int run();

   private:
      ServerOptions options_;
      std::shared_ptr<ILogger> logger_;
};
