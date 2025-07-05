#include "sqlite_db.hpp"
#include <iostream>

SQLiteDB::SQLiteDB(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        db_ = nullptr;
    }
}

SQLiteDB::~SQLiteDB() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool SQLiteDB::Execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        last_error_ = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SQLiteDB::Execute(const std::string& sql, const std::vector<std::string>& params) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    if (!success) {
        last_error_ = sqlite3_errmsg(db_);
    }
    sqlite3_finalize(stmt);
    return success;
}

bool SQLiteDB::Query(const std::string& sql, std::vector<std::vector<std::string>>& results) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < cols; ++i) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }
        results.push_back(row);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool SQLiteDB::Query(const std::string& sql, const std::vector<std::string>& params, std::vector<std::vector<std::string>>& results) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        return false;
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < cols; ++i) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }
        results.push_back(row);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string SQLiteDB::GetLastError() const {
    return last_error_;
}

void SQLiteDB::InitSchema() {
    // Users
    Execute("CREATE TABLE IF NOT EXISTS users (id TEXT PRIMARY KEY, username TEXT UNIQUE, email TEXT, password_hash TEXT, level INTEGER, experience INTEGER, gold INTEGER, created_at TEXT, last_login TEXT)");
    // Decks
    Execute("CREATE TABLE IF NOT EXISTS decks (id TEXT PRIMARY KEY, user_id TEXT, name TEXT, is_active INTEGER, FOREIGN KEY(user_id) REFERENCES users(id))");
    // Deck cards
    Execute("CREATE TABLE IF NOT EXISTS deck_cards (id INTEGER PRIMARY KEY AUTOINCREMENT, deck_id TEXT, card_id TEXT, FOREIGN KEY(deck_id) REFERENCES decks(id))");
    // Cards
    Execute("CREATE TABLE IF NOT EXISTS cards (id TEXT PRIMARY KEY, name TEXT, description TEXT, attack INTEGER, defense INTEGER, cost INTEGER, rarity TEXT, type TEXT, abilities TEXT)");
    // Sessions
    Execute("CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, player1_id TEXT, player2_id TEXT, status TEXT, created_at TEXT, started_at TEXT, finished_at TEXT)");
    // Battles
    Execute("CREATE TABLE IF NOT EXISTS battles (id TEXT PRIMARY KEY, session_id TEXT, state TEXT, last_update TEXT, FOREIGN KEY(session_id) REFERENCES sessions(id))");
} 