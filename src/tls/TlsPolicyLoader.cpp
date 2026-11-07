#include "tls/TlsPolicyLoader.hpp"

#include "tls/ServerPolicy.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

// The server has no OCSP/CRL source for client certificates, so Botan's default of demanding revocation
// information would reject every client. A policy file can still switch it back on.
constexpr const char* kServerDefaults = "require_cert_revocation_info = false\n";

}  // namespace

std::shared_ptr<Botan::TLS::Policy> load_tls_policy(const std::string& policy_type, bool require_client_certificate) {
   if(policy_type == "default" || policy_type.empty()) {
      return std::make_shared<ServerPolicy>(kServerDefaults, require_client_certificate);
   }

   // if something we don't recognize, assume it's a file
   std::ifstream policy_stream(policy_type);
   if(!policy_stream.good()) {
      throw std::runtime_error("Unknown TLS policy: not a file or known short name");
   }

   std::ostringstream text;
   text << kServerDefaults << policy_stream.rdbuf();
   return std::make_shared<ServerPolicy>(text.str(), require_client_certificate);
}
