#pragma once

#include "../types.hpp"
#include <unordered_map>
#include <vector>
#include <random>
#include "../../src/sqlite_db.hpp"
#include <string>

namespace cardbattle {

// Forward declaration
class BattleWebSocketHandler;

class GameSessionManager {
private:
    std::unordered_map<std::string, GameSession> sessions_;
    std::unordered_map<std::string, std::string> user_active_session_; // user_id -> session_id
    std::mt19937 rng_;
    BattleWebSocketHandler* ws_handler_;

public:
    GameSessionManager() : ws_handler_(nullptr) {}
    ~GameSessionManager() = default;
    void Init(SQLiteDB* sqlite_db);
    void SetWebSocketHandler(BattleWebSocketHandler* handler) { ws_handler_ = handler; }

    std::string CreateSession(const std::string& player1_id);
    void JoinSession(const std::string& session_id, const std::string& player2_id);
    void SetDeckSelection(const std::string& session_id, const std::string& player_id, const std::string& deck_id);
    GameSession GetSession(const std::string& session_id);
    std::vector<GameSession> GetWaitingSessions();
    void EndSession(const std::string& session_id);
    void RemovePlayerFromSession(const std::string& session_id, const std::string& player_id);
    void ClearOldSessions();
    void BroadcastSessionUpdate(const std::string& session_id);

private:
    std::string GenerateId();
};

} // namespace cardbattle 