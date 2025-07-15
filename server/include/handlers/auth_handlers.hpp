#pragma once

#include <userver/server/handlers/http_handler_base.hpp>

namespace cardbattle {

class RegisterHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-register";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override;
};

class LoginHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-login";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override;
};

class UserProfileHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-profile";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override;
};

} // namespace cardbattle 