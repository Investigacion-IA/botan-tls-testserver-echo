#include "server/CommandLine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

namespace {

CommandLineResult parse(std::initializer_list<const char*> args) {
   std::vector<const char*> argv = {"testserver_echo"};
   argv.insert(argv.end(), args.begin(), args.end());
   return parse_command_line(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(CommandLineTest, RequiredOptionsOnlyUseTheDefaults) {
   const auto result = parse({"--port", "50445", "--cert", "server.pem", "--key", "server.key"});

   ASSERT_EQ(result.status, CommandLineResult::Status::Run) << result.message;
   EXPECT_EQ(result.options.port, 50445);
   EXPECT_EQ(result.options.cert, "server.pem");
   EXPECT_EQ(result.options.key, "server.key");
   EXPECT_EQ(result.options.policy, "default");
   EXPECT_EQ(result.options.client_ca, "");
   EXPECT_FALSE(result.options.require_client_certificate());
   EXPECT_EQ(result.options.idle_timeout_seconds, 60u);
   EXPECT_EQ(result.options.pkcs11_module, "");
}

TEST(CommandLineTest, AllOptionsAreRead) {
   const auto result = parse({"--port", "8443",
                              "--policy", "policies/tls13_pqc.txt",
                              "--cert", "c.pem",
                              "--key", "k.pem",
                              "--client-ca", "ca.pem",
                              "--idle-timeout", "5",
                              "--pkcs11-module", "/usr/lib/softhsm/libsofthsm2.so"});

   ASSERT_EQ(result.status, CommandLineResult::Status::Run) << result.message;
   EXPECT_EQ(result.options.port, 8443);
   EXPECT_EQ(result.options.policy, "policies/tls13_pqc.txt");
   EXPECT_EQ(result.options.client_ca, "ca.pem");
   EXPECT_TRUE(result.options.require_client_certificate());
   EXPECT_EQ(result.options.idle_timeout_seconds, 5u);
   EXPECT_EQ(result.options.pkcs11_module, "/usr/lib/softhsm/libsofthsm2.so");
}

TEST(CommandLineTest, MissingRequiredOptionIsAnError) {
   const auto result = parse({"--port", "50445", "--cert", "server.pem"});

   EXPECT_EQ(result.status, CommandLineResult::Status::Error);
   EXPECT_THAT(result.message, ::testing::HasSubstr("key"));
}

TEST(CommandLineTest, UnknownOptionIsAnError) {
   const auto result = parse({"--bogus"});

   EXPECT_EQ(result.status, CommandLineResult::Status::Error);
   EXPECT_THAT(result.message, ::testing::HasSubstr("bogus"));
}

TEST(CommandLineTest, InvalidPortValueIsAnError) {
   const auto result = parse({"--port", "not-a-number", "--cert", "c", "--key", "k"});

   EXPECT_EQ(result.status, CommandLineResult::Status::Error);
}

TEST(CommandLineTest, HelpListsTheOptions) {
   const auto result = parse({"--help"});

   EXPECT_EQ(result.status, CommandLineResult::Status::Help);
   EXPECT_THAT(result.message, ::testing::StartsWith("Usage:"));
   EXPECT_THAT(result.message, ::testing::HasSubstr("--port"));
   EXPECT_THAT(result.message, ::testing::HasSubstr("--client-ca"));
}

TEST(CommandLineTest, NoArgumentsShowsTheUsage) {
   const auto result = parse({});

   EXPECT_EQ(result.status, CommandLineResult::Status::Help);
}
