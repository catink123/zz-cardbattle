#pragma once

#include "../types.hpp"
#include <unordered_map>
#include <memory>
#include <random>
#include "../../src/sqlite_db.hpp"
#include <string>
#include <chrono>

namespace cardbattle {

class UserManager {
private:
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> username_to_id_;
    std::mt19937 rng_;

public:
    UserManager();
    ~UserManager() = default;
    void Init(SQLiteDB* sqlite_db);

    std::string CreateUser(const std::string& username, const std::string& email, const std::string& password);
    std::string AuthenticateUser(const std::string& username, const std::string& password);
    User GetUser(const std::string& user_id);
    void UpdateUserExperience(const std::string& user_id, int experience_gain);
    void UpdateUserGold(const std::string& user_id, int gold_change);

private:
    std::string GenerateId();
};

std::chrono::system_clock::time_point StringToTimePoint(const std::string& str);

} // namespace cardbattle 