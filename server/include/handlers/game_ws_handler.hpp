#pragma once
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/formats/json.hpp>
#include "../types.hpp"
#include "../../src/sqlite_db.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace cardbattle {

// Forward declarations
class BattleState;
class BattleManager;
class GameSessionManager;
class UserManager;

class BattleWebSocketHandler final : public userver::server::websocket::WebsocketHandlerBase {
public:
    static constexpr std::string_view kName = "handler-battle-ws";
    using WebsocketHandlerBase::WebsocketHandlerBase;

    struct ConnectionContext {
        std::string session_id;
        std::string user_id;
        bool session_joined = false;
    };

    void Handle(userver::server::websocket::WebSocketConnection& websocket, userver::server::request::RequestContext& context) const override;

private:
    // Connection contexts are stored in a static map for broadcasting

public:
    static void BroadcastSessionUpdate(const std::string& session_id);
    static void BroadcastBattleState(const std::string& session_id);
    static void RefreshAllClients(const std::string& session_id);
    static std::string BattleStateToJson(const BattleState& state);
    static std::string GetPlayerKey(const std::string& player_id, const BattleState& state);

    static void HandleStartBattle(userver::server::websocket::WebSocketConnection& websocket,
                                 const std::string& session_id, const std::string& user_id,
                                 const userver::formats::json::Value& message);
    static void HandleJoinSession(userver::server::websocket::WebSocketConnection& websocket,
                                 const std::string& session_id, const std::string& user_id);
    static void HandleLeaveSession(userver::server::websocket::WebSocketConnection& websocket,
                                  const std::string& session_id, const std::string& user_id);
};

extern BattleWebSocketHandler* g_battle_ws_handler;

// Initialize the WebSocket handler with managers
void InitWebSocketHandler(BattleManager* battle_mgr, GameSessionManager* session_mgr, UserManager* user_mgr);

} // namespace cardbattle 