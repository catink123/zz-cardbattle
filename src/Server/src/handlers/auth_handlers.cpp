#include "../include/handlers/auth_handlers.hpp"
#include "../include/managers/user_manager.hpp"
#include "../include/managers/deck_manager.hpp"
#include "../include/utils.hpp"
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/value.hpp>
#include <iostream>

namespace cardbattle {

// Global managers (in a real app, these would be dependency injected)
extern UserManager* user_manager;
extern DeckManager* deck_manager;

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
        
        // Create a default deck for the new user
        try {
            deck_manager->CreateDefaultDeck(user_id);
        } catch (const std::exception& e) {
            // Log the error but don't fail registration
            std::cout << "[RegisterHandler] Warning: Failed to create default deck: " << e.what() << std::endl;
        }
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "User registered successfully";
        response["user"] = userver::formats::json::MakeObject("id", user_id, "username", username, "email", email);
        response["token"] = user_id; // Simple token for now
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

        std::string user_id = user_manager->AuthenticateUser(username, password);
        User user = user_manager->GetUser(user_id);

        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["token"] = user_id;
        response["user"]["id"] = user.id;
        response["user"]["username"] = user.username;
        response["user"]["email"] = user.email;
        response["user"]["level"] = user.level;
        response["user"]["experience"] = user.experience;
        response["user"]["gold"] = user.gold;
        response["user"]["created_at"] = TimePointToString(user.created_at);
        response["user"]["last_login"] = TimePointToString(user.last_login);
        response["message"] = "Login successful";
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
        response["user"]["level"] = user.level;
        response["user"]["experience"] = user.experience;
        response["user"]["gold"] = user.gold;
        response["user"]["created_at"] = TimePointToString(user.created_at);
        response["user"]["last_login"] = TimePointToString(user.last_login);

        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

} // namespace cardbattle 