#pragma once

#include "server/ServerOptions.hpp"

#include <string>

struct CommandLineResult {
      enum class Status {
         Run,    ///< options are valid, start the server
         Help,   ///< usage was requested (or no arguments given); message holds the text
         Error,  ///< invalid arguments; message holds the reason
      };

      Status status = Status::Run;
      ServerOptions options;
      std::string message;
};

CommandLineResult parse_command_line(int argc, const char* const argv[]);
