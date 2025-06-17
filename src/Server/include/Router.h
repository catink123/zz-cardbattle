#pragma once

#include "common.h"
#include <boost/beast/http/verb.hpp>
#include <functional>
#include <utility>
#include <map>

using MessageGenerator = http::message_generator;
using Request = http::request<http::string_body>;
using RouteHandler = std::function<MessageGenerator(Request&&)>;
using Middleware = std::function<MessageGenerator(Request&&, RouteHandler)>;

class Router
{
private:
  using RouteKey = std::pair<http::verb, std::string>;

public:
  void AddRoute(http::verb method, const std::string& path, RouteHandler handler);
  std::optional<std::reference_wrapper<const RouteHandler>> Match(http::verb method, const std::string& path);

private:
  std::map<RouteKey, RouteHandler> m_routes;
};
