#include "../include/managers/user_manager.hpp"
#include "../sqlite_db.hpp"
#include "../include/utils.hpp"
#include <random>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <userver/crypto/hash.hpp>

namespace cardbattle {

static SQLiteDB* db = nullptr;

UserManager::UserManager() : rng_(std::random_device{}()) {
}

void UserManager::Init(SQLiteDB* sqlite_db) {
    db = sqlite_db;
}

std::string UserManager::CreateUser(const std::string& username, const std::string& email, const std::string& password) {
    std::string user_id = GenerateId();
    std::string password_hash = userver::crypto::hash::Sha256(password); // hash password
    std::string now = TimePointToString(std::chrono::system_clock::now());
    int level = 1, experience = 0, gold = 1000;
    std::string sql = "INSERT INTO users (id, username, email, password_hash, level, experience, gold, created_at, last_login) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    std::vector<std::string> params = {user_id, username, email, password_hash, std::to_string(level), std::to_string(experience), std::to_string(gold), now, now};
    if (!db->Execute(sql, params)) {
        throw std::runtime_error("User creation failed: " + db->GetLastError());
    }
    return user_id;
}

std::string UserManager::AuthenticateUser(const std::string& username, const std::string& password) {
    std::string sql = "SELECT id, password_hash FROM users WHERE username = ?";
    std::vector<std::vector<std::string>> results;
    if (!db->Query(sql, {username}, results) || results.empty()) {
        throw std::runtime_error("User not found");
    }
    std::string user_id = results[0][0];
    std::string stored_hash = results[0][1];
    std::string password_hash = userver::crypto::hash::Sha256(password); // hash password
    if (stored_hash != password_hash) {
        throw std::runtime_error("Invalid password");
    }
    // Update last_login
    std::string now = TimePointToString(std::chrono::system_clock::now());
    db->Execute("UPDATE users SET last_login = ? WHERE id = ?", {now, user_id});
    return user_id;
}

User UserManager::GetUser(const std::string& user_id) {
    std::string sql = "SELECT id, username, email, password_hash, level, experience, gold, created_at, last_login FROM users WHERE id = ?";
    std::vector<std::vector<std::string>> results;
    if (!db->Query(sql, {user_id}, results) || results.empty()) {
        throw std::runtime_error("User not found");
    }
    const auto& row = results[0];
    User user;
    user.id = row[0];
    user.username = row[1];
    user.email = row[2];
    user.password_hash = row[3];
    user.level = std::stoi(row[4]);
    user.experience = std::stoi(row[5]);
    user.gold = std::stoi(row[6]);
    user.created_at = StringToTimePoint(row[7]);
    user.last_login = StringToTimePoint(row[8]);
    return user;
}

void UserManager::UpdateUserExperience(const std::string& user_id, int experience_gain) {
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        throw std::runtime_error("User not found");
    }

    it->second.experience += experience_gain;
    
    // Simple leveling system
    int new_level = (it->second.experience / 100) + 1;
    if (new_level > it->second.level) {
        it->second.level = new_level;
    }
}

void UserManager::UpdateUserGold(const std::string& user_id, int gold_change) {
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        throw std::runtime_error("User not found");
    }

    it->second.gold += gold_change;
    if (it->second.gold < 0) {
        it->second.gold = 0;
    }
}

std::string UserManager::GenerateId() {
    // Simple UUID-like generator
    static const char* chars = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) id += '-';
        id += chars[rand() % 16];
    }
    return id;
}

} // namespace cardbattle 