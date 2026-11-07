#pragma once

#include "server/ServerOptions.hpp"

#include <memory>

namespace Botan::TLS {
class Context;
}

/// Loads the certificate, key, optional client CA and TLS 1.3 policy named in the options.
std::shared_ptr<Botan::TLS::Context> create_tls_context(const ServerOptions& options);
