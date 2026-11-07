#pragma once

#include <botan/p11_types.h>

#include <memory>
#include <string>

/// Loads the PKCS#11 shared library and calls C_Initialize. The returned module calls C_Finalize
/// when it is destroyed, so keep it alive for as long as the token is used.
std::unique_ptr<Botan::PKCS11::Module> initialize_pkcs11_module(const std::string& library_path);
