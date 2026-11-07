#pragma once

#include <botan/tls_policy.h>

#include <string_view>

/// A text policy that can only ever negotiate TLS 1.3. Mutual TLS is switched on by the server, not by the file.
class ServerPolicy final : public Botan::TLS::Text_Policy {
   public:
      /// `policy_text` uses Botan's policy file syntax; empty gives Botan's defaults.
      ServerPolicy(std::string_view policy_text, bool require_client_certificate);

      bool allow_tls12() const override;
      bool allow_tls13() const override;
      bool allow_dtls12() const override;
      bool require_client_certificate_authentication() const override;

   private:
      bool require_client_certificate_;
};
