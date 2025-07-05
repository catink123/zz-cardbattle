#include "../../include/handlers/game_ws_handler.hpp"
#include "../../include/managers/session_manager.hpp"
#include "../../include/managers/battle_manager.hpp"
#include <userver/logging/log.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <memory>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <sstream>

namespace cardbattle {

// Global session manager reference
extern GameSessionManager* session_manager;
extern BattleManager* battle_manager;

// Track active WebSocket connections by session and user
static std::unordered_map<std::string, std::unordered_map<std::string, userver::server::websocket::WebSocketConnection*>> active_connections;

BattleWebSocketHandler* g_battle_ws_handler = nullptr;

void BattleWebSocketHandler::Handle(userver::server::websocket::WebSocketConnection& websocket, userver::server::request::RequestContext& context) const {
    LOG_INFO() << "WebSocket connection established for battle";
    
    std::string session_id;
    std::string user_id;
    
    try {
        // Extract session_id and user_id from the connection URL
        // The URL format is: /battle/ws?session_id=xxx&user_id=yyy
        // We need to get this from the request context somehow
        
        // For now, let's try to get the session from the first available session
        // but we'll improve this later
        auto sessions = session_manager->GetWaitingSessions();
        if (!sessions.empty()) {
            session_id = sessions[0].id;
            // Try to determine user_id from the session
            if (sessions[0].host_id == "62d7183a-5d57-89e4-b2d6-b441e2411dc7") {
                user_id = sessions[0].host_id;
            } else if (sessions[0].guest_id == "09e111c7-e1e7-acb6-f8ca-c0bb2fc4c8bc") {
                user_id = sessions[0].guest_id;
            } else {
                user_id = sessions[0].host_id; // Default to host
            }
            LOG_INFO() << "Using session: " << session_id << " for user: " << user_id;
        } else {
            LOG_ERROR() << "No sessions available for WebSocket connection";
            userver::formats::json::ValueBuilder error_response;
            error_response["type"] = "error";
            error_response["message"] = "No sessions available";
            websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
            return;
        }
        
        LOG_INFO() << "WebSocket connection - Session ID: " << session_id << ", User ID: " << user_id;
        
        // Register this connection
        active_connections[session_id][user_id] = &websocket;
        
        // Send initial connection confirmation
        userver::formats::json::ValueBuilder response;
        response["type"] = "connection_established";
        response["session_id"] = session_id;
        response["user_id"] = user_id;
        response["message"] = "Connected to battle WebSocket";
        
        websocket.SendText(userver::formats::json::ToString(response.ExtractValue()));
        
        // Send initial battle state
        HandleGetBattleState(websocket, session_id, user_id);
        
        // Keep connection alive with heartbeat
        while (true) {
            try {
                // Send a heartbeat every 30 seconds
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
                userver::formats::json::ValueBuilder heartbeat;
                heartbeat["type"] = "heartbeat";
                heartbeat["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                websocket.SendText(userver::formats::json::ToString(heartbeat.ExtractValue()));
                
            } catch (const std::exception& e) {
                LOG_ERROR() << "Error in WebSocket loop: " << e.what();
                break;
            }
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "WebSocket error: " << e.what();
    }
    
    // Clean up connection
    if (!session_id.empty() && !user_id.empty()) {
        auto session_connections = active_connections.find(session_id);
        if (session_connections != active_connections.end()) {
            session_connections->second.erase(user_id);
            if (session_connections->second.empty()) {
                active_connections.erase(session_id);
            }
        }
    }
    
    LOG_INFO() << "WebSocket connection closed";
}

void BattleWebSocketHandler::HandleGetBattleState(userver::server::websocket::WebSocketConnection& websocket, 
                                                 const std::string& session_id, const std::string& user_id) {
    try {
        LOG_INFO() << "Getting battle state for session: " << session_id;
        
        auto session = session_manager->GetSession(session_id);
        
        userver::formats::json::ValueBuilder response;
        response["type"] = "battle_state";
        response["session_id"] = session_id;
        response["session_status"] = session.status;
        
        if (session.status == "active") {
            // Get actual battle state
            auto battle_state = battle_manager->GetBattleState(session_id);
            response["data"] = userver::formats::json::FromString(BattleStateToJson(battle_state));
        } else {
            // Send session info for waiting state
            userver::formats::json::ValueBuilder session_data;
            session_data["session_id"] = session.id;
            session_data["host_id"] = session.host_id;
            session_data["guest_id"] = session.guest_id;
            session_data["status"] = session.status;
            session_data["host_deck_id"] = session.host_deck_id;
            session_data["guest_deck_id"] = session.guest_deck_id;
            
            // Add player health info if available
            if (session.status == "active") {
                auto battle_state = battle_manager->GetBattleState(session_id);
                userver::formats::json::ValueBuilder health_data;
                for (const auto& pair : battle_state.players) {
                    health_data[pair.first] = pair.second.health;
                }
                session_data["player_health"] = health_data.ExtractValue();
            }
            
            response["data"] = session_data.ExtractValue();
        }
        
        std::string response_str = userver::formats::json::ToString(response.ExtractValue());
        LOG_INFO() << "Sending battle state response: " << response_str;
        websocket.SendText(response_str);
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "Failed to get battle state: " << e.what();
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to get battle state: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandlePlayCard(userver::server::websocket::WebSocketConnection& websocket,
                                           const std::string& session_id, const std::string& user_id,
                                           const userver::formats::json::Value& message) {
    try {
        int hand_index = message["hand_index"].As<int>();
        
        battle_manager->PlayCard(session_id, user_id, hand_index);
        
        // Broadcast updated battle state to all players in session
        BroadcastBattleState(session_id);
        
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to play card: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandleAttack(userver::server::websocket::WebSocketConnection& websocket,
                                         const std::string& session_id, const std::string& user_id,
                                         const userver::formats::json::Value& message) {
    try {
        int attacker_index = message["attacker_index"].As<int>();
        int target_index = message["target_index"].As<int>();
        
        battle_manager->Attack(session_id, user_id, attacker_index, target_index);
        
        // Broadcast updated battle state to all players in session
        BroadcastBattleState(session_id);
        
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to attack: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandleEndTurn(userver::server::websocket::WebSocketConnection& websocket,
                                          const std::string& session_id, const std::string& user_id) {
    try {
        battle_manager->EndTurn(session_id, user_id);
        
        // Broadcast updated battle state to all players in session
        BroadcastBattleState(session_id);
        
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to end turn: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandleStartBattle(userver::server::websocket::WebSocketConnection& websocket,
                                              const std::string& session_id, const std::string& user_id,
                                              const userver::formats::json::Value& message) {
    try {
        std::string host_deck_id = message["host_deck_id"].As<std::string>();
        std::string guest_deck_id = message["guest_deck_id"].As<std::string>();
        
        battle_manager->StartBattle(session_id, host_deck_id, guest_deck_id);
        
        // Update session status
        auto session = session_manager->GetSession(session_id);
        session.host_deck_id = host_deck_id;
        session.guest_deck_id = guest_deck_id;
        session.status = "active";
        
        // Broadcast battle started to all players
        BroadcastBattleState(session_id);
        
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to start battle: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandleJoinSession(userver::server::websocket::WebSocketConnection& websocket,
                                              const std::string& session_id, const std::string& user_id) {
    try {
        session_manager->JoinSession(session_id, user_id);
        
        // Broadcast session update to all players
        BroadcastSessionUpdate(session_id);
        
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to join session: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::HandleLeaveSession(userver::server::websocket::WebSocketConnection& websocket,
                                               const std::string& session_id, const std::string& user_id) {
    try {
        LOG_INFO() << "User " << user_id << " leaving session " << session_id;
        
        // Remove player from session
        session_manager->RemovePlayerFromSession(session_id, user_id);
        
        // Broadcast session update to remaining players
        BroadcastSessionUpdate(session_id);
        
        // Send confirmation to the leaving player
        userver::formats::json::ValueBuilder response;
        response["type"] = "session_left";
        response["session_id"] = session_id;
        response["user_id"] = user_id;
        response["message"] = "Successfully left session";
        
        websocket.SendText(userver::formats::json::ToString(response.ExtractValue()));
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "Failed to handle leave session: " << e.what();
        userver::formats::json::ValueBuilder error_response;
        error_response["type"] = "error";
        error_response["message"] = "Failed to leave session: " + std::string(e.what());
        websocket.SendText(userver::formats::json::ToString(error_response.ExtractValue()));
    }
}

void BattleWebSocketHandler::BroadcastBattleState(const std::string& session_id) {
    LOG_INFO() << "Broadcasting battle state for session: " << session_id;
    
    auto session_connections = active_connections.find(session_id);
    if (session_connections == active_connections.end()) {
        LOG_INFO() << "No connections found for session: " << session_id;
        return;
    }
    
    try {
        auto battle_state = battle_manager->GetBattleState(session_id);
        std::string state_json = BattleStateToJson(battle_state);
        
        userver::formats::json::ValueBuilder response;
        response["type"] = "battle_state";
        response["data"] = userver::formats::json::FromString(state_json);
        
        std::string response_str = userver::formats::json::ToString(response.ExtractValue());
        
        for (const auto& connection_pair : session_connections->second) {
            try {
                connection_pair.second->SendText(response_str);
            } catch (const std::exception& e) {
                LOG_ERROR() << "Failed to send to connection: " << e.what();
            }
        }
        
        LOG_INFO() << "Broadcasted battle state to " << session_connections->second.size() << " connections";
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "Failed to broadcast battle state: " << e.what();
    }
}

void BattleWebSocketHandler::BroadcastSessionUpdate(const std::string& session_id) {
    try {
        auto session = session_manager->GetSession(session_id);
        userver::formats::json::ValueBuilder response;
        response["type"] = "session_update";
        response["session_id"] = session.id;
        response["host_id"] = session.host_id;
        response["guest_id"] = session.guest_id;
        response["status"] = session.status;
        response["host_deck_id"] = session.host_deck_id;
        response["guest_deck_id"] = session.guest_deck_id;
        std::string message = userver::formats::json::ToString(response.ExtractValue());
        LOG_INFO() << "[WS] Broadcasting session update: " << message;
        // Send to all active connections for this session
        auto session_connections = active_connections.find(session_id);
        if (session_connections != active_connections.end()) {
            for (auto& pair : session_connections->second) {
                try {
                    pair.second->SendText(message);
                    LOG_INFO() << "[WS] Sent session update to user: " << pair.first;
                } catch (const std::exception& e) {
                    LOG_ERROR() << "[WS] Error sending to connection: " << e.what();
                }
            }
        } else {
            LOG_WARNING() << "[WS] No active connections found for session: " << session_id;
        }
    } catch (const std::exception& e) {
        LOG_ERROR() << "[WS] Error broadcasting session update: " << e.what();
    }
}

void BattleWebSocketHandler::RefreshAllClients(const std::string& session_id) {
    try {
        LOG_INFO() << "Refreshing all clients for session: " << session_id;
        
        // Send a battle state update to all connected clients
        auto session_connections = active_connections.find(session_id);
        if (session_connections != active_connections.end()) {
            for (auto& pair : session_connections->second) {
                try {
                    HandleGetBattleState(*pair.second, session_id, pair.first);
                    LOG_INFO() << "Refreshed battle state for user: " << pair.first;
                } catch (const std::exception& e) {
                    LOG_ERROR() << "Error refreshing client: " << e.what();
                }
            }
        } else {
            LOG_WARNING() << "No active connections found for session: " << session_id;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "Error refreshing clients: " << e.what();
    }
}

std::string BattleWebSocketHandler::BattleStateToJson(const BattleState& state) {
    userver::formats::json::ValueBuilder builder;
    builder["session_id"] = state.session_id;
    builder["current_turn"] = state.current_turn;
    builder["turn_number"] = state.turn_number;
    builder["winner"] = state.winner;
    builder["is_finished"] = state.is_finished;
    builder["last_action"] = state.last_action;
    
    userver::formats::json::ValueBuilder players_builder;
    for (const auto& pair : state.players) {
        userver::formats::json::ValueBuilder player_builder;
        const PlayerState& player = pair.second;
        
        player_builder["player_id"] = player.player_id;
        player_builder["health"] = player.health;
        player_builder["max_health"] = player.max_health;
        player_builder["mana"] = player.mana;
        player_builder["max_mana"] = player.max_mana;
        player_builder["is_active"] = player.is_active;
        
        // Convert cards to JSON
        userver::formats::json::ValueBuilder hand_builder;
        for (const auto& card : player.hand) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["description"] = card.description;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["rarity"] = static_cast<int>(card.rarity);
            card_builder["type"] = static_cast<int>(card.type);
            hand_builder.PushBack(card_builder.ExtractValue());
        }
        player_builder["hand"] = hand_builder.ExtractValue();
        
        // Similar for field, deck, graveyard...
        userver::formats::json::ValueBuilder field_builder;
        for (const auto& card : player.field) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["description"] = card.description;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["rarity"] = static_cast<int>(card.rarity);
            card_builder["type"] = static_cast<int>(card.type);
            field_builder.PushBack(card_builder.ExtractValue());
        }
        player_builder["field"] = field_builder.ExtractValue();
        
        players_builder[pair.first] = player_builder.ExtractValue();
    }
    
    builder["players"] = players_builder.ExtractValue();
    return userver::formats::json::ToString(builder.ExtractValue());
}

} // namespace cardbattle 