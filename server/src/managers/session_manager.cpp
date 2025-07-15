#include "../include/managers/session_manager.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace cardbattle {

std::string GameSessionManager::CreateSession(const std::string& player1_id) {
    ClearOldSessions();
    std::string session_id = GenerateId();
    auto now = std::chrono::system_clock::now();
    std::string created_at = std::to_string(now.time_since_epoch().count());
    
    GameSession session;
    session.id = session_id;
    session.host_id = player1_id;
    session.guest_id = "";
    session.status = "waiting";
    session.created_at = created_at;
    
    sessions_[session_id] = session;
    user_active_session_[player1_id] = session_id;
    return session_id;
}

void GameSessionManager::JoinSession(const std::string& session_id, const std::string& player2_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) throw std::runtime_error("Session not found");
    GameSession& session = it->second;
    if (!session.guest_id.empty()) throw std::runtime_error("Session is full");
    session.guest_id = player2_id;
    session.status = "ready";
    user_active_session_[player2_id] = session_id;
}

std::vector<GameSession> GameSessionManager::GetWaitingSessions() {
    std::vector<GameSession> waiting_sessions;
    for (const auto& pair : sessions_) {
        if (pair.second.status == "waiting" || pair.second.status == "ready") {
            waiting_sessions.push_back(pair.second);
        }
    }
    return waiting_sessions;
}

GameSession GameSessionManager::GetSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) return it->second;
    throw std::runtime_error("Session not found");
}

void GameSessionManager::EndSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    if (!it->second.host_id.empty()) user_active_session_.erase(it->second.host_id);
    if (!it->second.guest_id.empty()) user_active_session_.erase(it->second.guest_id);
    sessions_.erase(it);
}

void GameSessionManager::RemovePlayerFromSession(const std::string& session_id, const std::string& player_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    if (it->second.host_id == player_id) it->second.host_id = "";
    else if (it->second.guest_id == player_id) it->second.guest_id = "";
    user_active_session_.erase(player_id);
    if (it->second.host_id.empty() && it->second.guest_id.empty()) sessions_.erase(it);
    else it->second.status = "waiting";
}

void GameSessionManager::ClearOldSessions() {
    // Optionally remove sessions older than X time
}

std::string GameSessionManager::GenerateId() {
    // Generate a unique 6-digit code
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::string code;
    do {
        code = std::to_string(dis(gen));
    } while (sessions_.count(code) > 0);
    return code;
}

} // namespace cardbattle 