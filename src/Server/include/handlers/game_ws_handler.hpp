#pragma once
#include <userver/server/websocket/websocket_handler.hpp>
#include <userver/formats/json.hpp>
#include "../types.hpp"
#include <memory>
#include <string>

namespace cardbattle {

class BattleWebSocketHandler final : public userver::server::websocket::WebsocketHandlerBase {
public:
    static constexpr std::string_view kName = "handler-battle-ws";
    using WebsocketHandlerBase::WebsocketHandlerBase;

    void Handle(userver::server::websocket::WebSocketConnection& websocket, userver::server::request::RequestContext& context) const override;

    static void BroadcastSessionUpdate(const std::string& session_id);
    static void BroadcastBattleState(const std::string& session_id);
    static void RefreshAllClients(const std::string& session_id);
    static std::string BattleStateToJson(const BattleState& state);

    static void HandleGetBattleState(userver::server::websocket::WebSocketConnection& websocket, 
                                    const std::string& session_id, const std::string& user_id);
    static void HandlePlayCard(userver::server::websocket::WebSocketConnection& websocket,
                              const std::string& session_id, const std::string& user_id,
                              const userver::formats::json::Value& message);
    static void HandleAttack(userver::server::websocket::WebSocketConnection& websocket,
                            const std::string& session_id, const std::string& user_id,
                            const userver::formats::json::Value& message);
    static void HandleEndTurn(userver::server::websocket::WebSocketConnection& websocket,
                              const std::string& session_id, const std::string& user_id);
    static void HandleStartBattle(userver::server::websocket::WebSocketConnection& websocket,
                                 const std::string& session_id, const std::string& user_id,
                                 const userver::formats::json::Value& message);
    static void HandleJoinSession(userver::server::websocket::WebSocketConnection& websocket,
                                 const std::string& session_id, const std::string& user_id);
    static void HandleLeaveSession(userver::server::websocket::WebSocketConnection& websocket,
                                  const std::string& session_id, const std::string& user_id);
};

extern BattleWebSocketHandler* g_battle_ws_handler;

} // namespace cardbattle 