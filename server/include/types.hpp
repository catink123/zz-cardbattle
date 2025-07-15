#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <optional>

namespace cardbattle {

// Card types
enum class CardType {
    CREATURE,
    SPELL
};

// Data structures
struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
    int wins = 0;
    int losses = 0;

    User() = default;
};

struct Card {
    std::string id;
    std::string name;
    std::string description;
    int attack;
    int defense;
    int mana_cost;
    CardType type;
    bool used_this_turn;  // Track if card has been used this turn
    
    Card() : used_this_turn(false) {}
    Card(const std::string& id, const std::string& name, const std::string& description,
         int attack, int defense, int mana_cost, CardType type)
        : id(id), name(name), description(description), attack(attack), defense(defense),
          mana_cost(mana_cost), type(type), used_this_turn(false) {}
};

struct Deck {
    std::string id;
    std::string name;
    std::vector<std::string> card_ids;
};

struct GameSession {
    std::string id;
    std::string host_id;
    std::string guest_id;
    std::string status; // "waiting", "ready", "active", "finished"
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
    
    PlayerState() : health(30), max_health(30), mana(1), max_mana(1), is_active(false) {}
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