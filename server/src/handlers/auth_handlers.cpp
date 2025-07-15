#include "../include/handlers/auth_handlers.hpp"
#include "../include/managers/user_manager.hpp"
#include "../include/utils.hpp"
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/value.hpp>
#include <iostream>

namespace cardbattle {

// Global managers (in a real app, these would be dependency injected)
extern UserManager* user_manager;

std::string RegisterHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string username = body["username"].As<std::string>();
        std::string email = body["email"].As<std::string>();
        std::string password = body["password"].As<std::string>();
        std::string user_id = user_manager->CreateUser(username, email, password);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Registration successful";
        response["token"] = user_id;
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string LoginHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string username = body["username"].As<std::string>();
        std::string password = body["password"].As<std::string>();
        std::string session_token = user_manager->LoginUser(username, password);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Login successful";
        response["token"] = session_token;
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string UserProfileHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        std::string user_id = request.GetArg("user_id");
        User user = user_manager->GetUser(user_id);

        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["user"]["id"] = user.id;
        response["user"]["username"] = user.username;
        response["user"]["email"] = user.email;
        response["user"]["wins"] = user.wins;
        response["user"]["losses"] = user.losses;

        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

} // namespace cardbattle 