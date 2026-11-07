#pragma once

#include <botan/certstor.h>
#include <botan/credentials_manager.h>
#include <botan/x509cert.h>

#include <memory>
#include <string>
#include <vector>

/// Serves one certificate chain and its private key loaded from PEM files.
/// With a client CA file it also trusts those CAs for client certificates (mutual TLS).
class BasicCredentialsManager final : public Botan::Credentials_Manager {
   public:
      /// `client_ca` may be empty: then no client certificate can be validated.
      BasicCredentialsManager(const std::string& server_crt,
                              const std::string& server_key,
                              const std::string& client_ca = "");

      std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(const std::string& type,
                                                                             const std::string& context) override;

      std::vector<Botan::X509_Certificate> find_cert_chain(
         const std::vector<std::string>& algos,
         const std::vector<Botan::AlgorithmIdentifier>& cert_signature_schemes,
         const std::vector<Botan::X509_DN>& acceptable_cas,
         const std::string& type,
         const std::string& hostname) override;

      std::shared_ptr<Botan::Private_Key> private_key_for(const Botan::X509_Certificate& cert,
                                                          const std::string& type,
                                                          const std::string& context) override;

   private:
      struct CertificateInfo {
            std::vector<Botan::X509_Certificate> certs;
            std::shared_ptr<Botan::Private_Key> key;
      };

      std::vector<CertificateInfo> creds_;
      Botan::Certificate_Store_In_Memory client_ca_;
      bool has_client_ca_ = false;
};
