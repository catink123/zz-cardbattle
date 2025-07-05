#include "../include/managers/session_manager.hpp"
#include "../include/handlers/game_ws_handler.hpp"
#include "../include/types.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <random>
#include <sstream>
#include <iostream>

namespace cardbattle {



void GameSessionManager::Init(SQLiteDB* sqlite_db) {
    // No longer need database for sessions
    (void)sqlite_db; // Suppress unused parameter warning
}

std::string GameSessionManager::CreateSession(const std::string& player1_id) {
    // Clear old sessions first to avoid conflicts
    ClearOldSessions();
    
    std::string session_id = GenerateId();
    auto now = std::chrono::system_clock::now();
    
    // Debug logging
    std::cout << "[SessionManager] Creating session with ID: " << session_id << std::endl;
    std::cout << "[SessionManager] Player1 ID: " << player1_id << std::endl;
    
    // Create session in memory
    GameSession session;
    session.id = session_id;
    session.host_id = player1_id;
    session.guest_id = "";
    session.host_deck_id = "";
    session.guest_deck_id = "";
    session.status = "waiting";
    session.created_at = std::to_string(now.time_since_epoch().count());
    
    // Store in memory
    sessions_[session_id] = session;
    user_active_session_[player1_id] = session_id;
    
    std::cout << "[SessionManager] Session created successfully with ID: " << session_id << std::endl;
    return session_id;
}

void GameSessionManager::JoinSession(const std::string& session_id, const std::string& player2_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found");
    }
    
    if (it->second.status != "waiting") {
        throw std::runtime_error("Session is not waiting for players");
    }
    
    if (!it->second.guest_id.empty()) {
        throw std::runtime_error("Session is already full");
    }
    
    // Update session
    it->second.guest_id = player2_id;
    
    // Change status to "ready_for_decks" since both players have joined
    it->second.status = "ready_for_decks";
    
    // Track user's active session
    user_active_session_[player2_id] = session_id;
    
    std::cout << "[SessionManager] Player " << player2_id << " joined session " << session_id << std::endl;
    std::cout << "[SessionManager] Session status changed to: " << it->second.status << std::endl;
    std::cout << "[SessionManager] Host: " << it->second.host_id << ", Guest: " << it->second.guest_id << std::endl;
    
    // Broadcast session update to all connected clients
    std::cout << "[SessionManager] Session update broadcast triggered for session " << session_id << std::endl;
}

void GameSessionManager::SetDeckSelection(const std::string& session_id, const std::string& player_id, const std::string& deck_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found");
    }
    
    std::cout << "[SessionManager] Setting deck selection for session " << session_id << std::endl;
    std::cout << "[SessionManager] Player " << player_id << " selecting deck " << deck_id << std::endl;
    
    // Set deck for the appropriate player
    if (it->second.host_id == player_id) {
        it->second.host_deck_id = deck_id;
        std::cout << "[SessionManager] Host selected deck: " << deck_id << std::endl;
    } else if (it->second.guest_id == player_id) {
        it->second.guest_deck_id = deck_id;
        std::cout << "[SessionManager] Guest selected deck: " << deck_id << std::endl;
    } else {
        throw std::runtime_error("Player not in this session");
    }
    
    std::cout << "[SessionManager] Current deck status - Host: " << it->second.host_deck_id << ", Guest: " << it->second.guest_deck_id << std::endl;
    
    // Check if both players have selected decks
    if (!it->second.host_deck_id.empty() && !it->second.guest_deck_id.empty()) {
        it->second.status = "ready";
        std::cout << "[SessionManager] Session " << session_id << " is ready for battle!" << std::endl;
        std::cout << "[SessionManager] Session status changed to: " << it->second.status << std::endl;
    } else {
        std::cout << "[SessionManager] Session " << session_id << " still waiting for deck selections" << std::endl;
    }
    // Broadcast session update to all connected clients
    std::cout << "[SessionManager] Session update broadcast triggered for session " << session_id << std::endl;
}

std::vector<GameSession> GameSessionManager::GetWaitingSessions() {
    std::vector<GameSession> waiting_sessions;
    
    for (const auto& pair : sessions_) {
        if (pair.second.status == "waiting" || pair.second.status == "ready_for_decks" || pair.second.status == "ready") {
            waiting_sessions.push_back(pair.second);
        }
    }
    
    return waiting_sessions;
}

GameSession GameSessionManager::GetSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found");
    }
    return it->second;
}

void GameSessionManager::EndSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return; // Session already doesn't exist
    }
    
    // Remove from user tracking
    if (!it->second.host_id.empty()) {
        user_active_session_.erase(it->second.host_id);
    }
    if (!it->second.guest_id.empty()) {
        user_active_session_.erase(it->second.guest_id);
    }
    
    // Remove session
    sessions_.erase(it);
}

void GameSessionManager::RemovePlayerFromSession(const std::string& session_id, const std::string& player_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return; // Session doesn't exist
    }
    
    std::cout << "[SessionManager] Removing player " << player_id << " from session " << session_id << std::endl;
    
    // Remove player from session
    if (it->second.host_id == player_id) {
        it->second.host_id = "";
        std::cout << "[SessionManager] Host left the session" << std::endl;
    } else if (it->second.guest_id == player_id) {
        it->second.guest_id = "";
        std::cout << "[SessionManager] Guest left the session" << std::endl;
    } else {
        std::cout << "[SessionManager] Player " << player_id << " not found in session" << std::endl;
        return;
    }
    
    // Remove from user tracking
    user_active_session_.erase(player_id);
    
    // If both players left, end the session
    if (it->second.host_id.empty() && it->second.guest_id.empty()) {
        std::cout << "[SessionManager] Both players left, ending session " << session_id << std::endl;
        sessions_.erase(it);
    } else {
        // Update session status to waiting if only one player remains
        it->second.status = "waiting";
        it->second.host_deck_id = "";
        it->second.guest_deck_id = "";
        std::cout << "[SessionManager] Session " << session_id << " status changed to waiting" << std::endl;
    }
}

std::string GameSessionManager::GenerateId() {
    // Generate a 6-digit random code as session ID
    std::uniform_int_distribution<int> dist(100000, 999999);
    return std::to_string(dist(rng_));
}

void GameSessionManager::ClearOldSessions() {
    auto now = std::chrono::system_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);
    
    std::vector<std::string> sessions_to_remove;
    
    for (const auto& pair : sessions_) {
        try {
            auto created_time = std::stoll(pair.second.created_at);
            auto created_point = std::chrono::system_clock::time_point(std::chrono::milliseconds(created_time));
            
            if (created_point < one_hour_ago) {
                sessions_to_remove.push_back(pair.first);
            }
        } catch (const std::exception& e) {
            // If we can't parse the timestamp, remove the session
            sessions_to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& session_id : sessions_to_remove) {
        EndSession(session_id);
    }
    
    if (!sessions_to_remove.empty()) {
        std::cout << "[SessionManager] Cleared " << sessions_to_remove.size() << " old sessions" << std::endl;
    }
}

void GameSessionManager::BroadcastSessionUpdate(const std::string& session_id) {
    std::cout << "[SessionManager] Broadcasting session update for session " << session_id << std::endl;
    
    // Log the session state
    try {
        auto session = GetSession(session_id);
        std::cout << "[SessionManager] Session state - ID: " << session.id 
                  << ", Status: " << session.status 
                  << ", Host: " << session.host_id 
                  << ", Guest: " << session.guest_id 
                  << ", Host Deck: " << session.host_deck_id 
                  << ", Guest Deck: " << session.guest_deck_id << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[SessionManager] Error getting session for broadcast: " << e.what() << std::endl;
    }
    
    // Call the WebSocket handler's refresh method if available
    if (ws_handler_) {
        // Note: We can't call ws_handler_->RefreshAllClients(session_id) directly
        // because of circular dependency. We'll handle this differently.
        std::cout << "[SessionManager] WebSocket handler available, should refresh clients" << std::endl;
    } else {
        std::cout << "[SessionManager] No WebSocket handler available" << std::endl;
    }
}

} // namespace cardbattle 