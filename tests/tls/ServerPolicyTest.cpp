#include "tls/ServerPolicy.hpp"

#include <gtest/gtest.h>

TEST(ServerPolicyTest, OnlyTls13IsAllowedWhateverTheTextSays) {
   const ServerPolicy policy("allow_tls12 = true\nallow_tls13 = false\n", false);

   EXPECT_TRUE(policy.allow_tls13());
   EXPECT_FALSE(policy.allow_tls12());
   EXPECT_FALSE(policy.allow_dtls12());
}

TEST(ServerPolicyTest, TextSettingsAreStillUsed) {
   const ServerPolicy policy("key_exchange_groups = x25519 secp256r1\n", false);

   EXPECT_EQ(policy.key_exchange_groups().size(), 2u);
}

TEST(ServerPolicyTest, ClientCertificateIsRequiredOnlyWhenAsked) {
   EXPECT_FALSE(ServerPolicy("", false).require_client_certificate_authentication());
   EXPECT_TRUE(ServerPolicy("", true).require_client_certificate_authentication());
   EXPECT_TRUE(ServerPolicy("require_client_certificate_authentication = false\n", true)
                  .require_client_certificate_authentication());
}
