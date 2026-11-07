#include "tls/BasicCredentialsManager.hpp"

#include <botan/data_src.h>
#include <botan/pkcs8.h>

#include <algorithm>
#include <ranges>
#include <stdexcept>

BasicCredentialsManager::BasicCredentialsManager(const std::string& server_crt,
                                                 const std::string& server_key,
                                                 const std::string& client_ca) {
   CertificateInfo cert;

   Botan::DataSource_Stream key_in(server_key);
   cert.key = Botan::PKCS8::load_key(key_in);

   Botan::DataSource_Stream in(server_crt);
   while(!in.end_of_data()) {
      try {
         cert.certs.push_back(Botan::X509_Certificate(in));
      } catch(std::exception&) {}
   }

   creds_.push_back(cert);

   if(!client_ca.empty()) {
      Botan::DataSource_Stream ca_in(client_ca);
      while(!ca_in.end_of_data()) {
         try {
            client_ca_.add_certificate(Botan::X509_Certificate(ca_in));
            has_client_ca_ = true;
         } catch(std::exception&) {}
      }
      if(!has_client_ca_) {
         throw std::runtime_error("No certificate found in client CA file: " + client_ca);
      }
   }
}

std::vector<Botan::Certificate_Store*> BasicCredentialsManager::trusted_certificate_authorities(
   const std::string& /*type*/, const std::string& /*context*/) {
   if(has_client_ca_) {
      return {&client_ca_};
   }
   return {};
}

std::vector<Botan::X509_Certificate> BasicCredentialsManager::find_cert_chain(
   const std::vector<std::string>& algos,
   const std::vector<Botan::AlgorithmIdentifier>& /*cert_signature_schemes*/,
   const std::vector<Botan::X509_DN>& /*acceptable_cas*/,
   const std::string& /*type*/,
   const std::string& hostname) {
   const auto cred = std::ranges::find_if(creds_, [&](const auto& cred) {
      return std::ranges::any_of(algos, [&](const auto& algo) { return algo == cred.key->algo_name(); }) &&
             (hostname.empty() || cred.certs.front().matches_dns_name(hostname));
   });

   return (cred != creds_.end()) ? cred->certs : std::vector<Botan::X509_Certificate>{};
}

std::shared_ptr<Botan::Private_Key> BasicCredentialsManager::private_key_for(const Botan::X509_Certificate& cert,
                                                                             const std::string& /*type*/,
                                                                             const std::string& /*context*/) {
   const auto cred = std::ranges::find_if(creds_, [&](const auto& cred) { return cred.certs.front() == cert; });
   return (cred != creds_.end()) ? cred->key : nullptr;
}
