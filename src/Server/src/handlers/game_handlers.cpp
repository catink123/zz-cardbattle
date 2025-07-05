#include "../include/handlers/game_handlers.hpp"
#include "../include/managers/session_manager.hpp"
#include "../include/managers/battle_manager.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/value.hpp>

namespace cardbattle {

// Global managers (in a real app, these would be dependency injected)
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
        
        // Broadcast session update to all connected WebSocket clients
        // We need to access the WebSocket handler's broadcast method
        // For now, we'll just return success and let the WebSocket handle updates
        
        // Trigger a session state refresh for all connected clients
        // This will be handled by the WebSocket handler when clients request battle state
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Joined session successfully";
        response["session_status"] = "ready_for_decks"; // Include the new status
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
            session_obj["host_deck_id"] = session.host_deck_id;
            session_obj["guest_deck_id"] = session.guest_deck_id;
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

std::string StartBattleHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        
        // Get the session to check if it's ready
        auto session = session_manager->GetSession(session_id);
        if (session.status != "ready") {
            throw std::runtime_error("Session is not ready for battle. Both players must select decks first.");
        }
        
        // Start the battle with the selected decks
        battle_manager->StartBattle(session_id, session.host_deck_id, session.guest_deck_id);
        
        // Update session status to active
        // Note: We need to add a method to update session status
        // For now, we'll just start the battle
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Battle started successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string GetBattleStateHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        std::string session_id = request.GetArg("session_id");
        
        // Get the session first
        auto session = session_manager->GetSession(session_id);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        
        if (session.status == "active") {
            // Return actual battle state for active battles
            auto state = battle_manager->GetBattleState(session_id);
            response["battle_state"] = userver::formats::json::FromString("{}"); // TODO: serialize BattleState
        } else {
            // Return session information for waiting/ready sessions
            userver::formats::json::ValueBuilder session_data;
            session_data["session_id"] = session.id;
            session_data["host_id"] = session.host_id;
            session_data["guest_id"] = session.guest_id;
            session_data["status"] = session.status;
            session_data["host_deck_id"] = session.host_deck_id;
            session_data["guest_deck_id"] = session.guest_deck_id;
            
            response["battle_state"] = session_data.ExtractValue();
        }
        
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string PlayCardHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        std::string player_id = body["player_id"].As<std::string>();
        int hand_index = body["hand_index"].As<int>();
        battle_manager->PlayCard(session_id, player_id, hand_index);
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Card played successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string AttackHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        std::string attacker_id = body["attacker_id"].As<std::string>();
        int attacker_index = body["attacker_index"].As<int>();
        int target_index = body["target_index"].As<int>();
        battle_manager->Attack(session_id, attacker_id, attacker_index, target_index);
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Attack performed successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string EndTurnHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string session_id = body["session_id"].As<std::string>();
        std::string player_id = body["user_id"].As<std::string>();
        battle_manager->EndTurn(session_id, player_id);
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Turn ended successfully";
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
        
        // Remove player from session
        session_manager->RemovePlayerFromSession(session_id, player_id);
        
        // Log the session update (WebSocket clients will handle their own updates)
        std::cout << "[LeaveSessionHandler] Player " << player_id << " left session " << session_id << std::endl;
        
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

std::string SelectDeckHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
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
        std::string deck_id = body["deck_id"].As<std::string>();
        
        // Set the deck selection for this player
        session_manager->SetDeckSelection(session_id, player_id, deck_id);
        
        userver::formats::json::ValueBuilder response;
        response["success"] = true;
        response["message"] = "Deck selected successfully";
        return userver::formats::json::ToString(response.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

} // namespace cardbattle 