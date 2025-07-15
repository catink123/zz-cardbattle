#include "sqlite_db.hpp"
#include <iostream>
#include <stdexcept>

SQLiteDB::SQLiteDB(const std::string& db_path) {
    // Open database with read/write permissions, create if doesn't exist
    int rc = sqlite3_open_v2(db_path.c_str(), &db_, 
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 
                            nullptr);
    if (rc != SQLITE_OK) {
        std::string error_msg = db_ ? sqlite3_errmsg(db_) : "Failed to open database";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        last_error_ = error_msg;
        throw std::runtime_error("Failed to open SQLite database: " + error_msg);
    }
    
    // Enable foreign key constraints
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
    
    // Set journal mode to WAL for better concurrency
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL", nullptr, nullptr, nullptr);
    
    std::cout << "SQLite database opened successfully: " << db_path << std::endl;
}

SQLiteDB::~SQLiteDB() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool SQLiteDB::Execute(const std::string& sql) {
    if (!db_) {
        last_error_ = "Database not initialized";
        return false;
    }
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
    if (!db_) {
        last_error_ = "Database not initialized";
        return false;
    }
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
    if (!db_) {
        last_error_ = "Database not initialized";
        return false;
    }
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
    if (!db_) {
        last_error_ = "Database not initialized";
        return false;
    }
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
    std::cout << "Initializing database schema..." << std::endl;
    
    // Users (simplified)
    if (!Execute("CREATE TABLE IF NOT EXISTS users (id TEXT PRIMARY KEY, username TEXT UNIQUE, email TEXT, password_hash TEXT, wins INTEGER DEFAULT 0, losses INTEGER DEFAULT 0)")) {
        std::cout << "Failed to create users table: " << GetLastError() << std::endl;
    } else {
        std::cout << "Users table created successfully" << std::endl;
    }
    
    // Sessions
    if (!Execute("CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, player1_id TEXT, player2_id TEXT, status TEXT, created_at TEXT, started_at TEXT, finished_at TEXT)")) {
        std::cout << "Failed to create sessions table: " << GetLastError() << std::endl;
    } else {
        std::cout << "Sessions table created successfully" << std::endl;
    }
    
    // Battles
    if (!Execute("CREATE TABLE IF NOT EXISTS battles (id TEXT PRIMARY KEY, session_id TEXT, state TEXT, last_update TEXT, FOREIGN KEY(session_id) REFERENCES sessions(id))")) {
        std::cout << "Failed to create battles table: " << GetLastError() << std::endl;
    } else {
        std::cout << "Battles table created successfully" << std::endl;
    }
    
    std::cout << "Database schema initialization completed" << std::endl;
} 