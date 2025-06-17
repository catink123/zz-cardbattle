#pragma once

#include "common.h"
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core.hpp>
#include <boost/core/ignore_unused.hpp>
#include <chrono>
#include <memory>
#include <string>
#include "Router.h"

using Middleware = std::function<MessageGenerator(Request&, const RouteHandler&)>;

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
  HttpSession
  (
    tcp::socket&& socket,
    std::shared_ptr<std::string const> docRoot,
    std::shared_ptr<Router> router,
    std::shared_ptr<std::vector<Middleware>> middleware
  );

public:
  void Run();
  void DoRead();
  void OnRead(beast::error_code ec, std::size_t bytesTransferred);
  void SendResponse(http::message_generator&& msg);
  void OnWrite(bool keepAlive, beast::error_code ec, std::size_t bytesTransferred);
  void DoClose();

private:
  beast::tcp_stream m_stream;
  beast::flat_buffer m_buffer;
  std::shared_ptr<std::string const> m_docRoot;
  http::request<http::string_body> m_req;

  std::shared_ptr<Router> m_router;
  std::shared_ptr<std::vector<Middleware>> m_middleware;
};

class ConfigurableServer : public std::enable_shared_from_this<ConfigurableServer>
{
public:
  ConfigurableServer
  (
    std::shared_ptr<net::io_context> ioc,
    tcp::endpoint endpoint,
    std::shared_ptr<std::string const> docRoot,
    std::shared_ptr<Router> router
  );

public:
  void Run();

private:
  void DoAccept();
  void OnAccept(beast::error_code ec, tcp::socket socket);

private:
  std::shared_ptr<net::io_context> m_ioc;
  tcp::acceptor m_acceptor;
  std::shared_ptr<std::string const> m_docRoot;

  std::shared_ptr<Router> m_router;
  std::shared_ptr<std::vector<Middleware>> m_middleware;
};
