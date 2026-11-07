#include "tls/SessionCallbacks.hpp"

#include <botan/tls_session.h>
#include <botan/pkix_types.h>
#include <botan/x509cert.h>

SessionCallbacks::SessionCallbacks(std::shared_ptr<ILogger> logger, std::string peer) :
      logger_(std::move(logger)), peer_(std::move(peer)) {}

void SessionCallbacks::tls_session_established(const Botan::TLS::Session_Summary& session) {
   std::string kex = session.kex_parameters().value_or(session.kex_algo());
   if(kex.empty()) {
      kex = "unknown";
   }

   std::string client = "none";
   const auto& peer_certs = session.peer_certs();
   if(!peer_certs.empty()) {
      client = peer_certs.front().subject_dn().to_string();
   }

   summary_ = peer_ + " " + session.version().to_string() + " kex=" + kex +
              " cipher=" + session.ciphersuite().to_string() + " client_cert=" + client;
   logger_->info("TLS established: " + summary_);
   Botan::TLS::StreamCallbacks::tls_session_established(session);
}
