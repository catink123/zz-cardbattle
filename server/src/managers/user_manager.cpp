#include "../include/managers/user_manager.hpp"
#include "../include/types.hpp"
#include "../sqlite_db.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <userver/logging/log.hpp>

namespace cardbattle {

static SQLiteDB* db = nullptr;

void UserManager::Init(SQLiteDB* sqlite_db) {
    db = sqlite_db;
    LOG_INFO() << "UserManager initialized";
}

std::string UserManager::CreateUser(const std::string& username, const std::string& email, const std::string& password) {
    // Check if username already exists
    std::string check_sql = "SELECT id FROM users WHERE username = ?";
    std::vector<std::vector<std::string>> results;
    if (db->Query(check_sql, {username}, results) && !results.empty()) {
        throw std::runtime_error("Username already exists");
    }
    
    // Check if email already exists
    check_sql = "SELECT id FROM users WHERE email = ?";
    if (db->Query(check_sql, {email}, results) && !results.empty()) {
        throw std::runtime_error("Email already exists");
    }
    
    std::string user_id = GenerateId();
    std::string password_hash = HashPassword(password);
    
    std::string sql = "INSERT INTO users (id, username, email, password_hash, wins, losses) VALUES (?, ?, ?, ?, 0, 0)";
    if (!db->Execute(sql, {user_id, username, email, password_hash})) {
        throw std::runtime_error("Failed to create user: " + db->GetLastError());
    }
    
    // Create user object
    User user;
    user.id = user_id;
    user.username = username;
    user.email = email;
    user.password_hash = password_hash;
    user.wins = 0;
    user.losses = 0;
    
    users_[user_id] = user;
    
    LOG_INFO() << "User created: " << username << " with ID: " << user_id;
    return user_id;
}

std::string UserManager::LoginUser(const std::string& username, const std::string& password) {
    std::string sql = "SELECT id, username, email, password_hash, wins, losses FROM users WHERE username = ?";
    std::vector<std::vector<std::string>> results;
    
    if (!db->Query(sql, {username}, results) || results.empty()) {
        throw std::runtime_error("Invalid username or password");
    }
    
    const auto& row = results[0];
    std::string user_id = row[0];
    std::string stored_hash = row[3];
    
    if (!VerifyPassword(password, stored_hash)) {
        throw std::runtime_error("Invalid username or password");
    }
    
    // Generate session token
    std::string session_token = GenerateSessionToken();
    sessions_[session_token] = user_id;
    
    LOG_INFO() << "User logged in: " << username << " with session token: " << session_token;
    return session_token;
}

User UserManager::GetUser(const std::string& user_id) {
    auto it = users_.find(user_id);
    if (it != users_.end()) {
        return it->second;
    }
    
    // Try to load from database
    std::string sql = "SELECT id, username, email, password_hash, wins, losses FROM users WHERE id = ?";
    std::vector<std::vector<std::string>> results;
    
    if (db->Query(sql, {user_id}, results) && !results.empty()) {
        const auto& row = results[0];
        User user;
        user.id = row[0];
        user.username = row[1];
        user.email = row[2];
        user.password_hash = row[3];
        user.wins = std::stoi(row[4]);
        user.losses = std::stoi(row[5]);
        
        users_[user_id] = user;
        return user;
    }
    
    throw std::runtime_error("User not found");
}

User UserManager::GetUserByUsername(const std::string& username) {
    std::string sql = "SELECT id, username, email, password_hash, wins, losses FROM users WHERE username = ?";
    std::vector<std::vector<std::string>> results;
    
    if (db->Query(sql, {username}, results) && !results.empty()) {
        const auto& row = results[0];
        User user;
        user.id = row[0];
        user.username = row[1];
        user.email = row[2];
        user.password_hash = row[3];
        user.wins = std::stoi(row[4]);
        user.losses = std::stoi(row[5]);
        
        users_[user.id] = user;
        return user;
    }
    
    throw std::runtime_error("User not found");
}

std::string UserManager::GetUserIdFromSession(const std::string& session_token) {
    auto it = sessions_.find(session_token);
    if (it != sessions_.end()) {
        return it->second;
    }
    throw std::runtime_error("Invalid session token");
}

void UserManager::LogoutUser(const std::string& session_token) {
    sessions_.erase(session_token);
    LOG_INFO() << "User logged out with session token: " << session_token;
}

void UserManager::UpdateUserStats(const std::string& user_id, bool won) {
    std::string sql;
    if (won) {
        sql = "UPDATE users SET wins = wins + 1 WHERE id = ?";
    } else {
        sql = "UPDATE users SET losses = losses + 1 WHERE id = ?";
    }
    
    if (!db->Execute(sql, {user_id})) {
        LOG_ERROR() << "Failed to update user stats: " << db->GetLastError();
        return;
    }
    
    // Update in-memory cache
    auto it = users_.find(user_id);
    if (it != users_.end()) {
        if (won) {
            it->second.wins++;
        } else {
            it->second.losses++;
        }
    }
    
    LOG_INFO() << "Updated stats for user " << user_id << " (won: " << won << ")";
}

std::vector<User> UserManager::GetAllUsers() {
    std::vector<User> all_users;
    std::string sql = "SELECT id, username, email, password_hash, wins, losses FROM users";
    std::vector<std::vector<std::string>> results;
    
    if (db->Query(sql, {}, results)) {
        for (const auto& row : results) {
            User user;
            user.id = row[0];
            user.username = row[1];
            user.email = row[2];
            user.password_hash = row[3];
            user.wins = std::stoi(row[4]);
            user.losses = std::stoi(row[5]);
            
            users_[user.id] = user;
            all_users.push_back(user);
        }
    }
    
    return all_users;
}

std::string UserManager::GenerateId() {
    static const char* chars = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) id += '-';
        id += chars[rand() % 16];
    }
    return id;
}

std::string UserManager::GenerateSessionToken() {
    static const char* chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[rand() % 62];
    }
    return token;
}

std::string UserManager::HashPassword(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.length());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool UserManager::VerifyPassword(const std::string& password, const std::string& hash) {
    std::string password_hash = HashPassword(password);
    return password_hash == hash;
}

} // namespace cardbattle 