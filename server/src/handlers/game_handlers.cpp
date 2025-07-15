#include "../include/handlers/game_handlers.hpp"
#include "../include/managers/session_manager.hpp"
#include "../include/managers/battle_manager.hpp"
#include "../include/utils.hpp"
#include "../include/handlers/game_ws_handler.hpp"
#include <iostream>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/value.hpp>

namespace cardbattle {

extern GameSessionManager* session_manager;
extern BattleManager* battle_manager;

std::string CreateSessionHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        // Extract player1_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string player1_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        std::string session_id = session_manager->CreateSession(player1_id);
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["session_id"] = session_id;
        response["message"] = "Session created successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string JoinSessionHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        // Extract player2_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string player2_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        
        session_manager->JoinSession(session_id, player2_id);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Joined session successfully";
        response["session_status"] = "ready";
        response["session_id"] = session_id;
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string GetSessionsHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto sessions = session_manager->GetWaitingSessions();
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["sessions"] = userver::formats::json::MakeArray();
        for (const auto& session : sessions) {
            userver::formats::json::ValueBuilder session_obj;
            session_obj["id"] = session.id;
            session_obj["host_id"] = session.host_id;
            session_obj["guest_id"] = session.guest_id;
            session_obj["status"] = session.status;
            session_obj["created_at"] = session.created_at;
            response["sessions"].PushBack(session_obj.ExtractValue());
        }
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string LeaveSessionHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        // Extract player_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string player_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        
        session_manager->RemovePlayerFromSession(session_id, player_id);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Left session successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

} // namespace cardbattle 
