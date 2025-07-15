#pragma once

#include "../types.hpp"
#include <unordered_map>
#include <vector>
#include "../../src/sqlite_db.hpp"
#include <string>

namespace cardbattle {

class UserManager {
private:
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> sessions_; // session_token -> user_id

public:
    UserManager() = default;
    ~UserManager() = default;
    void Init(SQLiteDB* sqlite_db);
    std::string CreateUser(const std::string& username, const std::string& email, const std::string& password);
    std::string LoginUser(const std::string& username, const std::string& password);
    User GetUser(const std::string& user_id);
    User GetUserByUsername(const std::string& username);
    std::string GetUserIdFromSession(const std::string& session_token);
    void LogoutUser(const std::string& session_token);
    void UpdateUserStats(const std::string& user_id, bool won);
    std::vector<User> GetAllUsers();

private:
    std::string GenerateId();
    std::string GenerateSessionToken();
    std::string HashPassword(const std::string& password);
    bool VerifyPassword(const std::string& password, const std::string& hash);
};

} // namespace cardbattle 