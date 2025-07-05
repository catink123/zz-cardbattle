#include "../include/managers/deck_manager.hpp"
#include "../sqlite_db.hpp"
#include "../include/types.hpp"
#include <vector>
#include <string>
#include <stdexcept>

namespace cardbattle {

static SQLiteDB* db = nullptr;

void DeckManager::Init(SQLiteDB* sqlite_db) {
    db = sqlite_db;
}

std::string DeckManager::CreateDeck(const std::string& user_id, const std::string& name, const std::vector<std::string>& card_ids) {
    std::string deck_id = GenerateId();
    std::string sql = "INSERT INTO decks (id, user_id, name, is_active) VALUES (?, ?, ?, 0)";
    if (!db->Execute(sql, {deck_id, user_id, name})) {
        throw std::runtime_error("Deck creation failed: " + db->GetLastError());
    }
    for (const auto& card_id : card_ids) {
        db->Execute("INSERT INTO deck_cards (deck_id, card_id) VALUES (?, ?)", {deck_id, card_id});
    }
    return deck_id;
}

Deck DeckManager::GetDeck(const std::string& deck_id) {
    std::string sql = "SELECT id, user_id, name, is_active FROM decks WHERE id = ?";
    std::vector<std::vector<std::string>> results;
    if (!db->Query(sql, {deck_id}, results) || results.empty()) {
        throw std::runtime_error("Deck not found");
    }
    Deck deck;
    deck.id = results[0][0];
    deck.user_id = results[0][1];
    deck.name = results[0][2];
    deck.is_active = (results[0][3] == "1");
    // Get card_ids
    std::vector<std::vector<std::string>> card_rows;
    db->Query("SELECT card_id FROM deck_cards WHERE deck_id = ?", {deck_id}, card_rows);
    for (const auto& row : card_rows) {
        deck.card_ids.push_back(row[0]);
    }
    return deck;
}

std::vector<Deck> DeckManager::GetUserDecks(const std::string& user_id) {
    std::string sql = "SELECT id FROM decks WHERE user_id = ?";
    std::vector<std::vector<std::string>> results;
    std::vector<Deck> decks;
    if (db->Query(sql, {user_id}, results)) {
        for (const auto& row : results) {
            decks.push_back(GetDeck(row[0]));
        }
    }
    return decks;
}

void DeckManager::SetActiveDeck(const std::string& user_id, const std::string& deck_id) {
    // Deactivate all decks for this user
    db->Execute("UPDATE decks SET is_active = 0 WHERE user_id = ?", {user_id});
    // Activate the specified deck
    db->Execute("UPDATE decks SET is_active = 1 WHERE id = ? AND user_id = ?", {deck_id, user_id});
}

std::string DeckManager::GenerateId() {
    static const char* chars = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) id += '-';
        id += chars[rand() % 16];
    }
    return id;
}

std::string DeckManager::CreateDefaultDeck(const std::string& user_id) {
    // Create a balanced default deck with 20 cards
    std::vector<std::string> default_card_ids = {
        "card_001", // Fire Elemental
        "card_002", // Water Spirit
        "card_003", // Lightning Bolt
        "card_005", // Healing Potion
        "card_006", // Knight
        "card_007", // Magic Shield
        "card_008", // Goblin
        "card_009", // Wizard
        "card_010", // Forest Guardian
        // Add duplicates for a 20-card deck
        "card_001", // Fire Elemental
        "card_002", // Water Spirit
        "card_003", // Lightning Bolt
        "card_005", // Healing Potion
        "card_006", // Knight
        "card_007", // Magic Shield
        "card_008", // Goblin
        "card_009", // Wizard
        "card_010", // Forest Guardian
        "card_004", // Dragon (legendary)
    };
    
    return CreateDeck(user_id, "Starter Deck", default_card_ids);
}

} // namespace cardbattle 