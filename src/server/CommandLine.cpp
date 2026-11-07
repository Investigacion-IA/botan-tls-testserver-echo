#include "server/CommandLine.hpp"

#include <boost/program_options.hpp>

#include <sstream>

CommandLineResult parse_command_line(int argc, const char* const argv[]) {
   namespace po = boost::program_options;

   // clang-format off
   po::options_description desc("Allowed options");
   desc.add_options()
      ("help", "produce help message")
      ("port", po::value<uint16_t>()->required(), "TLS 1.3 echo listen port")
      ("policy", po::value<std::string>()->default_value("default"), "Botan policy file (TLS 1.3 is always forced)")
      ("cert", po::value<std::string>()->required(), "Path to the server's certificate chain")
      ("key", po::value<std::string>()->required(), "Path to the server's certificate private key file")
      ("client-ca", po::value<std::string>()->default_value(""), "CA file for client certificates; enables mutual TLS")
      ("idle-timeout", po::value<uint64_t>()->default_value(60), "Close a connection after this many idle seconds (0 = never)");
   // clang-format on

   CommandLineResult result;
   po::variables_map vm;
   try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if(vm.count("help") != 0 || argc < 2) {
         std::ostringstream usage;
         usage << "Usage: \n" << desc;
         result.status = CommandLineResult::Status::Help;
         result.message = usage.str();
         return result;
      }

      po::notify(vm);
   } catch(const std::exception& ex) {
      result.status = CommandLineResult::Status::Error;
      result.message = ex.what();
      return result;
   }

   ServerOptions& options = result.options;
   options.port = vm["port"].as<uint16_t>();
   options.policy = vm["policy"].as<std::string>();
   options.cert = vm["cert"].as<std::string>();
   options.key = vm["key"].as<std::string>();
   options.client_ca = vm["client-ca"].as<std::string>();
   options.idle_timeout_seconds = vm["idle-timeout"].as<uint64_t>();
   return result;
}
