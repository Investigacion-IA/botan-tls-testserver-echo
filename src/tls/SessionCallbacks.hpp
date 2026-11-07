#pragma once

#include "common/ILogger.hpp"

#include <botan/tls_callbacks.h>
#include <botan/asio_stream.h>

#include <memory>
#include <string>

/// Stream callbacks that report what was negotiated (key exchange, cipher suite, client certificate).
class SessionCallbacks final : public Botan::TLS::StreamCallbacks {
   public:
      SessionCallbacks(std::shared_ptr<ILogger> logger, std::string peer);

      void tls_session_established(const Botan::TLS::Session_Summary& session) override;

      /// One line: "<peer> TLS 1.3 kex=... cipher=... client_cert=...", empty before the handshake finished.
      const std::string& summary() const { return summary_; }

   private:
      std::shared_ptr<ILogger> logger_;
      std::string peer_;
      std::string summary_;
};
