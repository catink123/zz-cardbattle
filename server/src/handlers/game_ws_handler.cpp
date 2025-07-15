#include "../include/handlers/game_ws_handler.hpp"
#include "../include/managers/battle_manager.hpp"
#include "../include/managers/session_manager.hpp"
#include "../include/managers/user_manager.hpp"
#include "../include/types.hpp"
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <algorithm>
#include <stdexcept>

namespace cardbattle {

static BattleManager* battle_manager = nullptr;
static GameSessionManager* session_manager = nullptr;
static UserManager* user_manager = nullptr;

// Global handler instance
BattleWebSocketHandler* g_battle_ws_handler = nullptr;

// Static connection map for broadcasting
static std::unordered_map<userver::server::websocket::WebSocketConnection*, BattleWebSocketHandler::ConnectionContext> g_connection_contexts;

void BattleWebSocketHandler::Handle(userver::server::websocket::WebSocketConnection& websocket, 
                                   userver::server::request::RequestContext& context) const {
    LOG_INFO() << "Handle() START for websocket: " << &websocket;
    // Get connection context
    auto& ctx = g_connection_contexts[const_cast<userver::server::websocket::WebSocketConnection*>(&websocket)];
    try {
        LOG_INFO() << "WebSocket connection opened for websocket: " << &websocket;
        
        // Handle incoming messages
        userver::server::websocket::Message message;
        while (true) {
            try {
                websocket.Recv(message); // blocks until a message is received or connection is closed
            } catch (const std::exception& e) {
                LOG_INFO() << "WebSocket Recv loop exited for websocket: " << &websocket << " with exception: " << e.what();
                break;
            }
            LOG_INFO() << "Received message on websocket: " << &websocket << ", data: " << message.data;
            if (!message.is_text || message.data.empty()) {
                continue;
            }
            try {
                auto json = userver::formats::json::FromString(message.data);
                std::string action = json["action"].As<std::string>();
                LOG_INFO() << "Processing action: " << action << " for websocket: " << &websocket;
                    
                if (action == "join_session") {
                    std::string session_id = json["session_id"].As<std::string>();
                    std::string user_id = json["user_id"].As<std::string>();
                    
                    ctx.user_id = user_id;
                    ctx.session_id = session_id;
                    ctx.session_joined = true;
                    
                    // Check session readiness before starting battle
                    auto session = session_manager->GetSession(session_id);
                    if (session.guest_id.empty() || session.status != "ready") {
                        websocket.SendText("{\"success\":false,\"error\":\"Cannot start battle: both players must join the session first.\"}");
                        LOG_ERROR() << "Attempted to start battle before both players joined.";
                        continue;
                    }
                    
                    // If the battle has already started, just send the current battle state to this client
                    try {
                        BattleState battle_state = battle_manager->GetBattleState(session_id);
                        std::string battle_json = BattleStateToJson(battle_state);
                        websocket.SendText(battle_json);
                        LOG_INFO() << "Sent current battle state to client " << user_id << " in session " << session_id;
                        continue;
                    } catch (const std::exception&) {
                        // If no battle state, proceed to start the battle as before
                    }
                    
                    // Only start the battle if BOTH host and guest are present in g_connection_contexts with session_joined=true
                    bool host_joined = false, guest_joined = false;
                    for (const auto& pair : g_connection_contexts) {
                        const auto& c = pair.second;
                        if (c.session_id == session_id && c.session_joined) {
                            if (c.user_id == session.host_id) host_joined = true;
                            if (c.user_id == session.guest_id) guest_joined = true;
                        }
                    }
                    if (host_joined && guest_joined) {
                        // Start battle if not already started
                        try {
                            battle_manager->GetBattleState(session_id);
                        } catch (const std::exception&) {
                            battle_manager->StartBattle(session_id);
                        }
                        // Broadcast battle state to all clients in the session (host and guest)
                        BroadcastBattleState(session_id);
                        LOG_INFO() << "Battle started and broadcasted for session " << session_id;
                    } else {
                        LOG_INFO() << "Waiting for both host and guest to join WebSocket for session " << session_id;
                    }
                    
                } else if (action == "play_card") {
                    if (!ctx.session_joined) {
                        websocket.SendText("{\"success\":false,\"error\":\"Not joined to session\"}");
                        LOG_ERROR() << "Play card attempted before joining session";
                        continue;
                    }
                    
                    try {
                        int hand_index = json["hand_index"].As<int>();
                        battle_manager->PlayCard(ctx.session_id, ctx.user_id, hand_index);
                        
                        // Broadcast updated battle state to all clients
                        BroadcastBattleState(ctx.session_id);
                    } catch (const std::runtime_error& e) {
                        // Handle game logic errors (like "Not enough mana")
                        websocket.SendText("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
                        LOG_ERROR() << "Game logic error in play_card: " << e.what() << " for user: " << ctx.user_id;
                        continue; // Continue processing messages, don't close connection
                    }
                    
                } else if (action == "attack") {
                    if (!ctx.session_joined) {
                        websocket.SendText("{\"success\":false,\"error\":\"Not joined to session\"}");
                        LOG_ERROR() << "Attack attempted before joining session";
                        continue;
                    }
                    
                    try {
                        int attacker_hand_index = json["attacker_hand_index"].As<int>();
                        int target_hand_index = json["target_hand_index"].As<int>();
                        
                        battle_manager->Attack(ctx.session_id, ctx.user_id, attacker_hand_index, target_hand_index);
                        
                        // Broadcast updated battle state to all clients
                        BroadcastBattleState(ctx.session_id);
                    } catch (const std::runtime_error& e) {
                        // Handle game logic errors
                        websocket.SendText("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
                        LOG_ERROR() << "Game logic error in attack: " << e.what() << " for user: " << ctx.user_id;
                        continue; // Continue processing messages, don't close connection
                    }
                    
                } else if (action == "end_turn") {
                    if (!ctx.session_joined) {
                        websocket.SendText("{\"success\":false,\"error\":\"Not joined to session\"}");
                        LOG_ERROR() << "End turn attempted before joining session";
                        continue;
                    }
                    
                    try {
                        battle_manager->EndTurn(ctx.session_id, ctx.user_id);
                        
                        // Broadcast updated battle state to all clients
                        BroadcastBattleState(ctx.session_id);
                    } catch (const std::runtime_error& e) {
                        // Handle game logic errors
                        websocket.SendText("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
                        LOG_ERROR() << "Game logic error in end_turn: " << e.what() << " for user: " << ctx.user_id;
                        continue; // Continue processing messages, don't close connection
                    }
                    
                } else if (action == "surrender") {
                    if (!ctx.session_joined) {
                        websocket.SendText("{\"success\":false,\"error\":\"Not joined to session\"}");
                        LOG_ERROR() << "Surrender attempted before joining session";
                        continue;
                    }
                    
                    try {
                        // Call the surrender method
                        battle_manager->Surrender(ctx.session_id, ctx.user_id);
                        
                        // Broadcast updated battle state to all clients
                        BroadcastBattleState(ctx.session_id);
                    } catch (const std::runtime_error& e) {
                        // Handle game logic errors
                        websocket.SendText("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}");
                        LOG_ERROR() << "Game logic error in surrender: " << e.what() << " for user: " << ctx.user_id;
                        continue; // Continue processing messages, don't close connection
                    }
                    
                } else if (action == "get_battle_state") {
                    if (!ctx.session_joined) {
                        websocket.SendText("{\"success\":false,\"error\":\"Not joined to session\"}");
                        LOG_ERROR() << "Get battle state attempted before joining session";
                        continue;
                    }
                    
                    // Send current battle state to this client
                    BattleState battle_state = battle_manager->GetBattleState(ctx.session_id);
                    std::string battle_json = BattleStateToJson(battle_state);
                    websocket.SendText(battle_json);
                    
                } else {
                    websocket.SendText("{\"success\":false,\"error\":\"Unknown action\"}");
                    LOG_ERROR() << "Unknown action: " << action;
                }
                    
            } catch (const std::exception& e) {
                websocket.SendText("{\"success\":false,\"error\":\"Invalid JSON: " + std::string(e.what()) + "\"}");
                LOG_ERROR() << "WebSocket JSON error: " << e.what() << " on websocket: " << &websocket;
                break; // Break the loop on error
            }
        }
        LOG_INFO() << "WebSocket message loop exited for websocket: " << &websocket;
    } catch (const std::exception& e) {
        LOG_ERROR() << "WebSocket error: " << e.what() << " on websocket: " << &websocket;
    }
    
    // Save session_id and user_id before erasing context
    std::string closed_session_id = ctx.session_id;
    std::string closed_user_id = ctx.user_id;
    LOG_INFO() << "Erasing connection context and closing websocket: " << &websocket;
    g_connection_contexts.erase(const_cast<userver::server::websocket::WebSocketConnection*>(&websocket));
    // If the context had a session, mark player left and broadcast
    if (!closed_session_id.empty()) {
        try {
            BattleState battle_state = battle_manager->GetBattleState(closed_session_id);
            battle_state.last_action = "Player left: " + closed_user_id;
            // Optionally, add a custom field to indicate a player left
            // (You may need to add this to the BattleState struct and serialization)
            // battle_state.player_left = closed_user_id;
            // Save and broadcast
            battle_manager->SaveBattleState(closed_session_id, battle_state);
            BroadcastBattleState(closed_session_id);
        } catch (const std::exception&) {
            // Battle may not exist, ignore
        }
    }
    LOG_INFO() << "Handle() END for websocket: " << &websocket;
}

void BattleWebSocketHandler::BroadcastSessionUpdate(const std::string& session_id) {
    // Implementation for broadcasting session updates
    LOG_INFO() << "Broadcasting session update for session: " << session_id;
}

void BattleWebSocketHandler::BroadcastBattleState(const std::string& session_id) {
    try {
        // Get current battle state
        BattleState battle_state = battle_manager->GetBattleState(session_id);
        std::string battle_json = BattleStateToJson(battle_state);
        LOG_INFO() << "Broadcasting battle state for session: " << session_id;
        // Collect keys to remove after iteration
        std::vector<userver::server::websocket::WebSocketConnection*> to_remove;
        std::vector<userver::server::websocket::WebSocketConnection*> keys;
        for (const auto& pair : g_connection_contexts) {
            keys.push_back(pair.first);
        }
        for (auto* conn : keys) {
            const auto& ctx = g_connection_contexts[conn];
            if (ctx.session_id == session_id && ctx.session_joined) {
                try {
                    conn->SendText(battle_json);
                    LOG_INFO() << "Broadcasted battle state to client " << ctx.user_id << " in session " << session_id;
                } catch (const std::exception& e) {
                    LOG_ERROR() << "Failed to send battle state to client " << ctx.user_id << ": " << e.what();
                    to_remove.push_back(conn);
                }
            }
        }
        for (auto* conn : to_remove) {
            g_connection_contexts.erase(conn);
            LOG_INFO() << "Removed closed/broken websocket connection: " << conn;
        }
        LOG_INFO() << "Broadcasted battle state to all clients in session: " << session_id;
    } catch (const std::exception& e) {
        LOG_ERROR() << "Error broadcasting battle state for session " << session_id << ": " << e.what();
    }
}

void BattleWebSocketHandler::RefreshAllClients(const std::string& session_id) {
    // Implementation for refreshing all clients
    LOG_INFO() << "Refreshing all clients for session: " << session_id;
}

std::string BattleWebSocketHandler::BattleStateToJson(const BattleState& state) {
    LOG_INFO() << "BattleStateToJson called for session " << state.session_id;
    for (const auto& pair : state.players) {
        const std::string& player_id = pair.first;
        const PlayerState& player = pair.second;
        LOG_INFO() << "  Serializing player " << player_id << ": hand size=" << player.hand.size() 
                   << ", deck size=" << player.deck.size() 
                   << ", field size=" << player.field.size() 
                   << ", graveyard size=" << player.graveyard.size();
    }
    
    userver::formats::json::ValueBuilder builder;
    
    builder["success"] = true;
    builder["session_id"] = state.session_id;
    builder["current_turn"] = state.current_turn;
    builder["turn_number"] = state.turn_number;
    builder["is_finished"] = state.is_finished;
    builder["winner"] = state.winner;
    builder["last_action"] = state.last_action;
    
    // Serialize players
    userver::formats::json::ValueBuilder players_builder;
    
    for (const auto& pair : state.players) {
        const std::string& player_id = pair.first;
        const PlayerState& player = pair.second;
        
        // Map player IDs to test-friendly names
        std::string player_key = GetPlayerKey(player_id, state);
        
        userver::formats::json::ValueBuilder player_builder;
        player_builder["health"] = player.health;
        player_builder["max_health"] = player.max_health;
        player_builder["mana"] = player.mana;
        player_builder["max_mana"] = player.max_mana;
        player_builder["is_active"] = player.is_active;
        
        // Serialize hand (always array, never null)
        userver::formats::json::ValueBuilder hand_builder;
        for (const auto& card : player.hand) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["type"] = static_cast<int>(card.type);
            hand_builder.PushBack(card_builder.ExtractValue());
        }
        // Always set as array, even if empty
        player_builder["hand"] = hand_builder.ExtractValue();
        
        // Serialize field (always array, never null)
        userver::formats::json::ValueBuilder field_builder;
        for (const auto& card : player.field) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["type"] = static_cast<int>(card.type);
            field_builder.PushBack(card_builder.ExtractValue());
        }
        // Always set as array, even if empty
        player_builder["field"] = field_builder.ExtractValue();
        
        // Serialize deck (just count)
        userver::formats::json::ValueBuilder deck_builder;
        deck_builder["count"] = static_cast<int>(player.deck.size());
        player_builder["deck"] = deck_builder.ExtractValue();
        
        // Serialize graveyard (always array, never null)
        userver::formats::json::ValueBuilder graveyard_builder;
        for (const auto& card : player.graveyard) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["type"] = static_cast<int>(card.type);
            graveyard_builder.PushBack(card_builder.ExtractValue());
        }
        // Always set as array, even if empty
        player_builder["graveyard"] = graveyard_builder.ExtractValue();
        
        players_builder[player_key] = player_builder.ExtractValue();
    }
    
    builder["players"] = players_builder.ExtractValue();
    
    std::string result = userver::formats::json::ToString(builder.ExtractValue());
    LOG_INFO() << "BattleStateToJson result: " << result;
    return result;
}

std::string BattleWebSocketHandler::GetPlayerKey(const std::string& player_id, const BattleState& state) {
    // In the simplified system, we'll use the player_id directly
    // This maps internal player IDs to the test-friendly names
    return player_id;
}

void InitWebSocketHandler(BattleManager* battle_mgr, GameSessionManager* session_mgr, UserManager* user_mgr) {
    battle_manager = battle_mgr;
    session_manager = session_mgr;
    user_manager = user_mgr;
    LOG_INFO() << "WebSocket handler managers initialized";
}

} // namespace cardbattle 