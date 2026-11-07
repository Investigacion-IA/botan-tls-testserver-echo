#include "tls/TlsPolicyLoader.hpp"

#include "tls/ServerPolicy.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

TEST(TlsPolicyLoaderTest, DefaultAndEmptyGiveTheServerPolicy) {
   const auto by_name = load_tls_policy("default", false);
   const auto empty = load_tls_policy("", false);

   ASSERT_TRUE(by_name);
   ASSERT_TRUE(empty);
   EXPECT_NE(dynamic_cast<ServerPolicy*>(by_name.get()), nullptr);
   EXPECT_NE(dynamic_cast<ServerPolicy*>(empty.get()), nullptr);
}

TEST(TlsPolicyLoaderTest, UnknownNameThatIsNoFileThrows) {
   EXPECT_THROW(load_tls_policy("/definitely/not/a/policy.txt", false), std::runtime_error);
}

TEST(TlsPolicyLoaderTest, PolicyFileSettingsAreApplied) {
   const auto file = std::filesystem::temp_directory_path() / "testserver_echo_policy_test.txt";
   {
      std::ofstream out(file);
      out << "key_exchange_groups = x25519\n";
   }

   const auto policy = load_tls_policy(file.string(), false);
   std::filesystem::remove(file);

   ASSERT_TRUE(policy);
   EXPECT_EQ(policy->key_exchange_groups().size(), 1u);
}

TEST(TlsPolicyLoaderTest, ClientCertificateRequirementIsPassedOn) {
   EXPECT_FALSE(load_tls_policy("default", false)->require_client_certificate_authentication());
   EXPECT_TRUE(load_tls_policy("default", true)->require_client_certificate_authentication());
}

TEST(TlsPolicyLoaderTest, ShippedPoliciesParseAndStayTls13Only) {
   for(const char* name : {"tls13_pqc.txt", "pqc_basic.txt"}) {
      const std::string path = std::string(TEST_FIXTURE_DIR) + "/../../policies/" + name;
      const auto policy = load_tls_policy(path, false);
      ASSERT_TRUE(policy) << path;
      EXPECT_TRUE(policy->allow_tls13()) << path;
      EXPECT_FALSE(policy->allow_tls12()) << path;
   }
}

TEST(TlsPolicyLoaderTest, RevocationInformationIsNotRequiredUnlessThePolicyFileAsksForIt) {
   EXPECT_FALSE(load_tls_policy("default", true)->require_cert_revocation_info());

   const auto file = std::filesystem::temp_directory_path() / "testserver_echo_policy_revocation.txt";
   {
      std::ofstream out(file);
      out << "require_cert_revocation_info = true\n";
   }
   const auto policy = load_tls_policy(file.string(), true);
   std::filesystem::remove(file);

   EXPECT_TRUE(policy->require_cert_revocation_info());
}
