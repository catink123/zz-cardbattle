#include "Router.h"

void Router::AddRoute(http::verb method, const std::string& path, RouteHandler handler)
{
  m_routes[{method, path}] = std::move(handler);
}

std::optional<std::reference_wrapper<const RouteHandler>> Router::Match(http::verb method, const std::string& path)
{
  auto it = m_routes.find({method, path});
  if (it != std::end(m_routes))
  {
    return std::cref(it->second);
  }
  return std::nullopt;
}
