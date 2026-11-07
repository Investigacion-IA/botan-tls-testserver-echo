#include "pkcs11/Pkcs11Module.hpp"

std::unique_ptr<Botan::PKCS11::Module> initialize_pkcs11_module(const std::string& library_path) {
   return std::make_unique<Botan::PKCS11::Module>(library_path);
}
