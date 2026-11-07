#pragma once

#include <botan/tls_policy.h>

#include <memory>
#include <string>

/// Builds the server policy: always TLS 1.3 only. "default" (or empty) uses Botan's default settings,
/// anything else is read as a policy file. `require_client_certificate` enables mutual TLS.
/// Revocation information for client certificates is not required unless the policy file asks for it.
/// Throws std::runtime_error when the file cannot be opened.
std::shared_ptr<Botan::TLS::Policy> load_tls_policy(const std::string& policy_type, bool require_client_certificate);
