/**
 * Botan TLS 1.3 echo server
 *
 * A raw TCP-over-TLS 1.3 echo service (Boost.Asio + Botan::TLS::Stream) for testing clients of
 * HSMs and communication middleware.
 */

#include "common/StderrLogger.hpp"
#include "server/CommandLine.hpp"
#include "server/ServerApp.hpp"

#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
   const CommandLineResult command_line = parse_command_line(argc, argv);

   switch(command_line.status) {
      case CommandLineResult::Status::Help:
         std::cerr << command_line.message << std::endl;
         return 0;
      case CommandLineResult::Status::Error:
         std::cerr << command_line.message << std::endl;
         return 1;
      case CommandLineResult::Status::Run:
         break;
   }

   ServerApp app(command_line.options, std::make_shared<StderrLogger>());
   return app.run();
}
