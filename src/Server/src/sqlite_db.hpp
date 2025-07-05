#ifndef SQLITE_DB_HPP
#define SQLITE_DB_HPP

#if __has_include(<sqlite3.h>)
#include <sqlite3.h>
#else
#error "sqlite3.h not found. Please install the SQLite3 development package."
#endif
#include <string>
#include <vector>

class SQLiteDB {
public:
    SQLiteDB(const std::string& db_path);
    ~SQLiteDB();

    bool Execute(const std::string& sql);
    bool Execute(const std::string& sql, const std::vector<std::string>& params);
    bool Query(const std::string& sql, std::vector<std::vector<std::string>>& results);
    bool Query(const std::string& sql, const std::vector<std::string>& params, std::vector<std::vector<std::string>>& results);
    std::string GetLastError() const;
    void InitSchema();

private:
    sqlite3* db_ = nullptr;
    std::string last_error_;
};

#endif // SQLITE_DB_HPP 