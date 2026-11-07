#include "server/TcpListener.hpp"

#include "common/NetErrors.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/this_coro.hpp>

boost::asio::awaitable<void> run_tcp_listener(boost::asio::ip::tcp::endpoint endpoint,
                                              std::shared_ptr<ISessionHandler> handler,
                                              std::shared_ptr<ILogger> logger,
                                              std::string session_context) {
   namespace net = boost::asio;
   using tcp = net::ip::tcp;

   auto acceptor = net::use_awaitable.as_default_on(tcp::acceptor(co_await net::this_coro::executor));
   acceptor.open(endpoint.protocol());
   acceptor.set_option(net::socket_base::reuse_address(true));
   acceptor.bind(endpoint);
   acceptor.listen(net::socket_base::max_listen_connections);

   while(true) {
      AwaitableTcp socket(co_await acceptor.async_accept());
      net::co_spawn(acceptor.get_executor(),
                    handler->handle(std::move(socket)),
                    make_completion_handler(logger, session_context));
   }
}
