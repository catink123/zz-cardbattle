#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <optional>

namespace cardbattle {

// Card types and rarities
enum class CardType {
    CREATURE,
    SPELL,
    TRAP
};

enum class CardRarity {
    COMMON,
    RARE,
    EPIC,
    LEGENDARY
};

// Data structures
struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
    int level = 1;
    int experience = 0;
    int gold = 1000;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    int wins;
    int losses;

    User() : wins(0), losses(0) {}
};

struct Card {
    std::string id;
    std::string name;
    std::string description;
    int attack;
    int defense;
    int mana_cost;
    CardRarity rarity;
    CardType type;
    std::vector<std::string> abilities;
    
    Card() = default;
    Card(const std::string& id, const std::string& name, const std::string& description,
         int attack, int defense, int mana_cost, CardRarity rarity, CardType type,
         const std::vector<std::string>& abilities = {})
        : id(id), name(name), description(description), attack(attack), defense(defense),
          mana_cost(mana_cost), rarity(rarity), type(type), abilities(abilities) {}
};

struct Deck {
    std::string id;
    std::string user_id;
    std::string name;
    std::vector<std::string> card_ids;
    bool is_active = false;
    std::string created_at;
};

struct GameSession {
    std::string id;
    std::string host_id;
    std::string guest_id;
    std::string host_deck_id;
    std::string guest_deck_id;
    std::string status; // "waiting", "active", "finished"
    std::string created_at;
};

struct PlayerState {
    std::string player_id;
    int health;
    int max_health;
    int mana;
    int max_mana;
    std::vector<Card> hand;
    std::vector<Card> deck;
    std::vector<Card> field;  // Cards on the battlefield
    std::vector<Card> graveyard;
    bool is_active;  // Is it this player's turn
    
    PlayerState() : health(30), max_health(30), mana(0), max_mana(0), is_active(false) {}
};

struct BattleState {
    std::string session_id;
    std::string current_turn;  // Player ID whose turn it is
    int turn_number;
    std::unordered_map<std::string, PlayerState> players;
    std::string winner;  // Empty if no winner yet
    bool is_finished;
    std::string last_action;  // For logging
    
    BattleState() : turn_number(1), is_finished(false) {}
};

struct ApiResponse {
    bool success;
    std::string message;
    std::string data;  // JSON string
    
    ApiResponse(bool success, const std::string& message, const std::string& data = "")
        : success(success), message(message), data(data) {}
};

} // namespace cardbattle 