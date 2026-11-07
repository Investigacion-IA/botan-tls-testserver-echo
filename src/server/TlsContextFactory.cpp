#include "server/TlsContextFactory.hpp"

#include "tls/BasicCredentialsManager.hpp"
#include "tls/TlsPolicyLoader.hpp"

#include <botan/asio_context.h>
#include <botan/auto_rng.h>
#include <botan/tls_session_manager_noop.h>

std::shared_ptr<Botan::TLS::Context> create_tls_context(const ServerOptions& options) {
   auto creds = std::make_shared<BasicCredentialsManager>(options.cert, options.key, options.client_ca);
   auto rng = std::make_shared<Botan::AutoSeeded_RNG>();
   auto session_mgr = std::make_shared<Botan::TLS::Session_Manager_Noop>();
   auto policy = load_tls_policy(options.policy, options.require_client_certificate());
   return std::make_shared<Botan::TLS::Context>(creds, rng, session_mgr, policy);
}
