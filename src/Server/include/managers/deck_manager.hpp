#pragma once

#include "../types.hpp"
#include "../../src/sqlite_db.hpp"
#include <unordered_map>
#include <vector>
#include <random>
#include <string>

namespace cardbattle {

class DeckManager {
private:
    std::unordered_map<std::string, Deck> decks_;
    std::unordered_map<std::string, std::vector<std::string>> user_decks_; // user_id -> deck_ids
    std::mt19937 rng_;

public:
    DeckManager() = default;
    void Init(SQLiteDB* sqlite_db);
    std::string CreateDeck(const std::string& user_id, const std::string& name, const std::vector<std::string>& card_ids);
    Deck GetDeck(const std::string& deck_id);
    std::vector<Deck> GetUserDecks(const std::string& user_id);
    void SetActiveDeck(const std::string& user_id, const std::string& deck_id);
    std::string CreateDefaultDeck(const std::string& user_id);
    std::string GenerateId();
};

} // namespace cardbattle 