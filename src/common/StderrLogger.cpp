#include "common/StderrLogger.hpp"

#include <chrono>
#include <ctime>
#include <iostream>

void StderrLogger::info(const std::string& message) {
   std::lock_guard lock(mutex_);
   std::cout << message << std::endl;
}

void StderrLogger::error(const std::string& context, const std::string& message) {
   std::lock_guard lock(mutex_);
   const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
   std::cerr << std::ctime(&now) << " " << context << ": " << message << std::endl;
}
