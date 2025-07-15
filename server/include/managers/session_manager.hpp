#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include "../types.hpp"

namespace cardbattle {

// GameSession struct is defined in types.hpp

class GameSessionManager {
public:
    std::string CreateSession(const std::string& player1_id);
    void JoinSession(const std::string& session_id, const std::string& player2_id);
    std::vector<GameSession> GetWaitingSessions();
    GameSession GetSession(const std::string& session_id);
    void EndSession(const std::string& session_id);
    void RemovePlayerFromSession(const std::string& session_id, const std::string& player_id);
    void ClearOldSessions();
private:
    std::string GenerateId();
    std::unordered_map<std::string, GameSession> sessions_;
    std::unordered_map<std::string, std::string> user_active_session_;
};

} // namespace cardbattle 