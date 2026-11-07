#include "tls/ServerPolicy.hpp"

ServerPolicy::ServerPolicy(std::string_view policy_text, bool require_client_certificate) :
      Botan::TLS::Text_Policy(policy_text), require_client_certificate_(require_client_certificate) {}

bool ServerPolicy::allow_tls12() const {
   return false;
}

bool ServerPolicy::allow_tls13() const {
   return true;
}

bool ServerPolicy::allow_dtls12() const {
   return false;
}

bool ServerPolicy::require_client_certificate_authentication() const {
   return require_client_certificate_;
}
