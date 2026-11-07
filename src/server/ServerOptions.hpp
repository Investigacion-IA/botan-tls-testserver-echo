#pragma once

#include <cstdint>
#include <string>

struct ServerOptions {
      std::uint16_t port = 0;
      std::string policy = "default";
      std::string cert;
      std::string key;
      std::string client_ca;  ///< PEM file with the CAs trusted for client certificates; empty disables mutual TLS
      std::uint64_t idle_timeout_seconds = 60;

      bool require_client_certificate() const { return !client_ca.empty(); }
};
