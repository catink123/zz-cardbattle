#include "ConfigurableServer.h"

HttpSession::HttpSession
(
  tcp::socket&& socket,
  std::shared_ptr<std::string const> docRoot,
  std::shared_ptr<Router> router,
  std::shared_ptr<std::vector<Middleware>> middleware
) : m_stream(std::move(socket)),
    m_docRoot(docRoot),
    m_router(router),
    m_middleware(middleware) {}

void HttpSession::Run()
{
  net::dispatch
  (
    m_stream.get_executor(),
    beast::bind_front_handler(&HttpSession::DoRead, shared_from_this())
  );
}

void HttpSession::DoRead()
{
  m_req = {};

  m_stream.expires_after(std::chrono::seconds(30));

  http::async_read
  (
    m_stream,
    m_buffer,
    m_req,
    beast::bind_front_handler(&HttpSession::OnRead, shared_from_this())
  );
}

void HttpSession::OnRead(beast::error_code ec, std::size_t bytesTransferred)
{
  boost::ignore_unused(bytesTransferred);

  if (ec == http::error::end_of_stream)
  {
    return DoClose();
  }

  if (ec)
  {
    return;
  }

  // Apply middleware
  std::optional<http::message_generator> response;
  auto handler = m_router->Match(m_req.method(), m_req.target());
  for (auto it = std::begin(*m_middleware); it != std::end(*m_middleware); ++it)
  {
    response = (*it)(std::move(m_req), handler.value());
  }

  // SendResponse()
}

void HttpSession::SendResponse(http::message_generator&& msg)
{
  bool keepAlive = msg.keep_alive();

  beast::async_write
  (
    m_stream,
    std::move(msg),
    beast::bind_front_handler(&HttpSession::OnWrite, shared_from_this(), keepAlive)
  );
}

void HttpSession::OnWrite(bool keepAlive, beast::error_code ec, std::size_t bytesTransferred)
{
  boost::ignore_unused(bytesTransferred);

  if (ec)
  {
    return;
  }

  if (!keepAlive)
  {
    return DoClose();
  }

  DoRead();
}

void HttpSession::DoClose()
{
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

ConfigurableServer::ConfigurableServer
(
  std::shared_ptr<net::io_context> ioc,
  tcp::endpoint endpoint,
  std::shared_ptr<std::string const> docRoot,
  std::shared_ptr<Router> router,
  std::shared_ptr<std::vector<Middleware>> middleware
) : m_ioc(ioc),
    m_acceptor(net::make_strand(*ioc)),
    m_docRoot(docRoot),
    m_router(router),
    m_middleware(middleware)
{
  beast::error_code ec;

  m_acceptor.open(endpoint.protocol(), ec);
  if (ec)
  {
    return;
  }

  m_acceptor.set_option(net::socket_base::reuse_address(true), ec);
  if (ec)
  {
    return;
  }

  m_acceptor.bind(endpoint, ec);
  if (ec)
  {
    return;
  }

  m_acceptor.listen(net::socket_base::max_listen_connections, ec);
  if (ec)
  {
    return;
  }
}

void ConfigurableServer::Run()
{
  DoAccept();
}

void ConfigurableServer::DoAccept()
{
  m_acceptor.async_accept
  (
    net::make_strand(*m_ioc),
    beast::bind_front_handler(&ConfigurableServer::OnAccept, shared_from_this())
  );
}

void ConfigurableServer::OnAccept(beast::error_code ec, tcp::socket socket)
{
  if (ec)
  {
    return;
  }
  else
  {
    std::make_shared<HttpSession>(std::move(socket), m_docRoot)->Run();
  }

  DoAccept();
}
