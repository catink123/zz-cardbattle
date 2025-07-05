#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/utils/datetime.hpp>

#include <unordered_map>
#include <memory>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>

namespace {

// Forward declarations
class UserManager;
class DeckManager;
class GameSessionManager;
class BattleManager;

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
};

struct Card {
    std::string id;
    std::string name;
    std::string description;
    int attack;
    int defense;
    int cost;
    std::string rarity; // "common", "rare", "epic", "legendary"
    std::string type;   // "creature", "spell", "artifact"
    std::vector<std::string> abilities;
};

struct Deck {
    std::string id;
    std::string user_id;
    std::string name;
    std::vector<std::string> card_ids;
    bool is_active = false;
};

struct GameSession {
    std::string id;
    std::string player1_id;
    std::string player2_id;
    std::string status; // "waiting", "active", "finished"
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point finished_at;
};

struct BattleState {
    std::string session_id;
    std::string current_turn; // player1_id or player2_id
    std::unordered_map<std::string, int> player_health; // player_id -> health
    std::unordered_map<std::string, std::vector<Card>> player_hands;
    std::unordered_map<std::string, std::vector<Card>> player_fields;
    std::unordered_map<std::string, std::vector<Card>> player_decks;
    std::vector<std::string> battle_log;
};

// Global managers
std::unique_ptr<UserManager> user_manager;
std::unique_ptr<DeckManager> deck_manager;
std::unique_ptr<GameSessionManager> session_manager;
std::unique_ptr<BattleManager> battle_manager;

// Helper function to convert time_point to string
std::string TimePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::ctime(&time_t);
    std::string result = ss.str();
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
    }
    return result;
}

// User Manager
class UserManager {
private:
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> username_to_id_;
    std::mt19937 rng_;

public:
    UserManager() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    std::string CreateUser(const std::string& username, const std::string& email, const std::string& password) {
        if (username_to_id_.find(username) != username_to_id_.end()) {
            throw std::runtime_error("Username already exists");
        }

        std::string user_id = GenerateId();
        std::string password_hash = userver::crypto::hash::Sha256(password);

        User user{
            .id = user_id,
            .username = username,
            .email = email,
            .password_hash = password_hash,
            .created_at = std::chrono::system_clock::now(),
            .last_login = std::chrono::system_clock::now()
        };

        users_[user_id] = user;
        username_to_id_[username] = user_id;

        LOG_INFO() << "Created user: " << username << " with ID: " << user_id;
        return user_id;
    }

    std::string AuthenticateUser(const std::string& username, const std::string& password) {
        auto it = username_to_id_.find(username);
        if (it == username_to_id_.end()) {
            throw std::runtime_error("User not found");
        }

        const User& user = users_[it->second];
        std::string password_hash = userver::crypto::hash::Sha256(password);
        
        if (user.password_hash != password_hash) {
            throw std::runtime_error("Invalid password");
        }

        // Update last login
        users_[user.id].last_login = std::chrono::system_clock::now();
        
        LOG_INFO() << "User authenticated: " << username;
        return user.id;
    }

    User GetUser(const std::string& user_id) {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            throw std::runtime_error("User not found");
        }
        return it->second;
    }

    void UpdateUserExperience(const std::string& user_id, int experience_gain) {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            throw std::runtime_error("User not found");
        }

        User& user = it->second;
        user.experience += experience_gain;
        
        // Simple leveling system
        int new_level = (user.experience / 100) + 1;
        if (new_level > user.level) {
            user.level = new_level;
            user.gold += (new_level - user.level) * 50; // Bonus gold for leveling up
        }
    }

    void UpdateUserGold(const std::string& user_id, int gold_change) {
        auto it = users_.find(user_id);
        if (it == users_.end()) {
            throw std::runtime_error("User not found");
        }

        User& user = it->second;
        user.gold += gold_change;
        if (user.gold < 0) user.gold = 0;
    }

private:
    std::string GenerateId() {
        static const std::string chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += chars[rng_() % chars.length()];
        }
        return id;
    }
};

// Deck Manager
class DeckManager {
private:
    std::unordered_map<std::string, Deck> decks_;
    std::unordered_map<std::string, std::vector<std::string>> user_decks_; // user_id -> deck_ids
    std::mt19937 rng_;

public:
    DeckManager() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    std::string CreateDeck(const std::string& user_id, const std::string& name, const std::vector<std::string>& card_ids) {
        if (card_ids.size() != 30) {
            throw std::runtime_error("Deck must contain exactly 30 cards");
        }

        std::string deck_id = GenerateId();
        
        Deck deck{
            .id = deck_id,
            .user_id = user_id,
            .name = name,
            .card_ids = card_ids
        };

        decks_[deck_id] = deck;
        user_decks_[user_id].push_back(deck_id);

        LOG_INFO() << "Created deck: " << name << " for user: " << user_id;
        return deck_id;
    }

    Deck GetDeck(const std::string& deck_id) {
        auto it = decks_.find(deck_id);
        if (it == decks_.end()) {
            throw std::runtime_error("Deck not found");
        }
        return it->second;
    }

    std::vector<Deck> GetUserDecks(const std::string& user_id) {
        std::vector<Deck> result;
        auto it = user_decks_.find(user_id);
        if (it != user_decks_.end()) {
            for (const auto& deck_id : it->second) {
                result.push_back(decks_[deck_id]);
            }
        }
        return result;
    }

    void SetActiveDeck(const std::string& user_id, const std::string& deck_id) {
        // Deactivate all other decks for this user
        auto it = user_decks_.find(user_id);
        if (it != user_decks_.end()) {
            for (const auto& id : it->second) {
                decks_[id].is_active = false;
            }
        }

        // Activate the specified deck
        auto deck_it = decks_.find(deck_id);
        if (deck_it != decks_.end()) {
            deck_it->second.is_active = true;
        }
    }

private:
    std::string GenerateId() {
        static const std::string chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += chars[rng_() % chars.length()];
        }
        return id;
    }
};

// Game Session Manager
class GameSessionManager {
private:
    std::unordered_map<std::string, GameSession> sessions_;
    std::unordered_map<std::string, std::string> user_active_session_; // user_id -> session_id
    std::mt19937 rng_;

public:
    GameSessionManager() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    std::string CreateSession(const std::string& player1_id) {
        std::string session_id = GenerateId();
        
        GameSession session{
            .id = session_id,
            .player1_id = player1_id,
            .player2_id = "",
            .status = "waiting",
            .created_at = std::chrono::system_clock::now(),
            .started_at = std::chrono::system_clock::time_point{},
            .finished_at = std::chrono::system_clock::time_point{}
        };

        sessions_[session_id] = session;
        user_active_session_[player1_id] = session_id;

        LOG_INFO() << "Created game session: " << session_id << " for player: " << player1_id;
        return session_id;
    }

    void JoinSession(const std::string& session_id, const std::string& player2_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            throw std::runtime_error("Session not found");
        }

        if (it->second.status != "waiting") {
            throw std::runtime_error("Session is not waiting for players");
        }

        if (it->second.player1_id == player2_id) {
            throw std::runtime_error("Player cannot join their own session");
        }

        it->second.player2_id = player2_id;
        it->second.status = "active";
        it->second.started_at = std::chrono::system_clock::now();
        user_active_session_[player2_id] = session_id;

        LOG_INFO() << "Player " << player2_id << " joined session " << session_id;
    }

    GameSession GetSession(const std::string& session_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            throw std::runtime_error("Session not found");
        }
        return it->second;
    }

    std::vector<GameSession> GetWaitingSessions() {
        std::vector<GameSession> result;
        for (const auto& [id, session] : sessions_) {
            if (session.status == "waiting") {
                result.push_back(session);
            }
        }
        return result;
    }

    void EndSession(const std::string& session_id) {
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.status = "finished";
            it->second.finished_at = std::chrono::system_clock::now();
            
            // Remove from active sessions
            user_active_session_.erase(it->second.player1_id);
            user_active_session_.erase(it->second.player2_id);
        }
    }

private:
    std::string GenerateId() {
        static const std::string chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += chars[rng_() % chars.length()];
        }
        return id;
    }
};

// Battle Manager
class BattleManager {
private:
    std::unordered_map<std::string, BattleState> battles_;
    std::unordered_map<std::string, Card> card_database_;
    std::mt19937 rng_;

public:
    BattleManager() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
        InitializeCardDatabase();
    }

    void StartBattle(const std::string& session_id, const std::string& player1_deck_id, const std::string& player2_deck_id) {
        BattleState battle;
        battle.session_id = session_id;
        
        // Initialize player health
        battle.player_health["player1"] = 30;
        battle.player_health["player2"] = 30;
        
        // Set initial turn
        battle.current_turn = "player1";
        
        // Initialize decks and hands
        InitializePlayerDeck(battle, "player1", player1_deck_id);
        InitializePlayerDeck(battle, "player2", player2_deck_id);
        
        // Draw initial hands
        DrawCards(battle, "player1", 4);
        DrawCards(battle, "player2", 4);
        
        battles_[session_id] = battle;
        
        LOG_INFO() << "Started battle for session: " << session_id;
    }

    BattleState GetBattleState(const std::string& session_id) {
        auto it = battles_.find(session_id);
        if (it == battles_.end()) {
            throw std::runtime_error("Battle not found");
        }
        return it->second;
    }

    void PlayCard(const std::string& session_id, const std::string& player_id, int hand_index) {
        auto it = battles_.find(session_id);
        if (it == battles_.end()) {
            throw std::runtime_error("Battle not found");
        }

        BattleState& battle = it->second;
        
        if (battle.current_turn != player_id) {
            throw std::runtime_error("Not your turn");
        }

        if (hand_index < 0 || static_cast<size_t>(hand_index) >= battle.player_hands[player_id].size()) {
            throw std::runtime_error("Invalid card index");
        }

        Card card = battle.player_hands[player_id][hand_index];
        
        // Remove card from hand
        battle.player_hands[player_id].erase(battle.player_hands[player_id].begin() + hand_index);
        
        // Add card to field
        battle.player_fields[player_id].push_back(card);
        
        // Log the action
        battle.battle_log.push_back(player_id + " played " + card.name);
        
        // Switch turns
        battle.current_turn = (player_id == "player1") ? "player2" : "player1";
        
        // Draw a card for the next player
        DrawCards(battle, battle.current_turn, 1);
    }

    void Attack(const std::string& session_id, const std::string& attacker_id, int attacker_index, int target_index) {
        auto it = battles_.find(session_id);
        if (it == battles_.end()) {
            throw std::runtime_error("Battle not found");
        }

        BattleState& battle = it->second;
        
        if (battle.current_turn != attacker_id) {
            throw std::runtime_error("Not your turn");
        }

        std::string target_id = (attacker_id == "player1") ? "player2" : "player1";
        
        if (attacker_index < 0 || static_cast<size_t>(attacker_index) >= battle.player_fields[attacker_id].size()) {
            throw std::runtime_error("Invalid attacker index");
        }

        if (target_index < 0 || static_cast<size_t>(target_index) >= battle.player_fields[target_id].size()) {
            throw std::runtime_error("Invalid target index");
        }

        Card& attacker = battle.player_fields[attacker_id][attacker_index];
        Card& target = battle.player_fields[target_id][target_index];
        
        // Perform attack
        target.defense -= attacker.attack;
        attacker.defense -= target.attack;
        
        // Log the action
        battle.battle_log.push_back(attacker_id + "'s " + attacker.name + " attacked " + target_id + "'s " + target.name);
        
        // Remove destroyed cards
        if (target.defense <= 0) {
            battle.player_fields[target_id].erase(battle.player_fields[target_id].begin() + target_index);
            battle.battle_log.push_back(target.name + " was destroyed");
        }
        
        if (attacker.defense <= 0) {
            battle.player_fields[attacker_id].erase(battle.player_fields[attacker_id].begin() + attacker_index);
            battle.battle_log.push_back(attacker.name + " was destroyed");
        }
        
        // Switch turns
        battle.current_turn = (attacker_id == "player1") ? "player2" : "player1";
    }

private:
    void InitializeCardDatabase() {
        // Create some sample cards
        card_database_["card_001"] = Card{
            .id = "card_001",
            .name = "Fire Dragon",
            .description = "A powerful dragon with fire breath",
            .attack = 8,
            .defense = 6,
            .cost = 5,
            .rarity = "rare",
            .type = "creature",
            .abilities = {"fire_breath"}
        };
        
        card_database_["card_002"] = Card{
            .id = "card_002",
            .name = "Knight",
            .description = "A brave knight in shining armor",
            .attack = 4,
            .defense = 5,
            .cost = 3,
            .rarity = "common",
            .type = "creature",
            .abilities = {"shield_bash"}
        };
        
        card_database_["card_003"] = Card{
            .id = "card_003",
            .name = "Lightning Bolt",
            .description = "A powerful lightning spell",
            .attack = 6,
            .defense = 0,
            .cost = 2,
            .rarity = "common",
            .type = "spell",
            .abilities = {"direct_damage"}
        };
        
        // Add more cards as needed...
    }

    void InitializePlayerDeck(BattleState& battle, const std::string& player_id, const std::string& deck_id) {
        Deck deck = deck_manager->GetDeck(deck_id);
        for (const auto& card_id : deck.card_ids) {
            auto it = card_database_.find(card_id);
            if (it != card_database_.end()) {
                battle.player_decks[player_id].push_back(it->second);
            }
        }
        
        // Shuffle the deck
        std::shuffle(battle.player_decks[player_id].begin(), battle.player_decks[player_id].end(), rng_);
    }

    void DrawCards(BattleState& battle, const std::string& player_id, int count) {
        for (int i = 0; i < count && !battle.player_decks[player_id].empty(); ++i) {
            Card card = battle.player_decks[player_id].back();
            battle.player_decks[player_id].pop_back();
            battle.player_hands[player_id].push_back(card);
        }
    }
};

// API Handlers

class HealthCheckHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-health";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest&, userver::server::request::RequestContext&) const override {
        userver::formats::json::ValueBuilder response;
        response["status"] = "healthy";
        response["timestamp"] = userver::utils::datetime::Now();
        response["version"] = "1.0.0";
        return userver::formats::json::ToString(response.ExtractValue());
    }
};

class RegisterHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-register";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string username = body["username"].As<std::string>();
            std::string email = body["email"].As<std::string>();
            std::string password = body["password"].As<std::string>();

            std::string user_id = user_manager->CreateUser(username, email, password);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["user_id"] = user_id;
            response["message"] = "User registered successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class LoginHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-login";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string username = body["username"].As<std::string>();
            std::string password = body["password"].As<std::string>();

            std::string user_id = user_manager->AuthenticateUser(username, password);
            User user = user_manager->GetUser(user_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["user_id"] = user_id;
            response["username"] = user.username;
            response["level"] = user.level;
            response["experience"] = user.experience;
            response["gold"] = user.gold;
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class UserProfileHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-profile";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            std::string user_id = request.GetArg("user_id");
            User user = user_manager->GetUser(user_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["user"]["id"] = user.id;
            response["user"]["username"] = user.username;
            response["user"]["email"] = user.email;
            response["user"]["level"] = user.level;
            response["user"]["experience"] = user.experience;
            response["user"]["gold"] = user.gold;
            response["user"]["created_at"] = TimePointToString(user.created_at);
            response["user"]["last_login"] = TimePointToString(user.last_login);
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class CreateDeckHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-create-deck";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string user_id = body["user_id"].As<std::string>();
            std::string name = body["name"].As<std::string>();
            auto card_ids_array = body["card_ids"];
            
            std::vector<std::string> card_ids;
            for (const auto& card_id : card_ids_array) {
                card_ids.push_back(card_id.As<std::string>());
            }

            std::string deck_id = deck_manager->CreateDeck(user_id, name, card_ids);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["deck_id"] = deck_id;
            response["message"] = "Deck created successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class GetDecksHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-get-decks";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            std::string user_id = request.GetArg("user_id");
            std::vector<Deck> decks = deck_manager->GetUserDecks(user_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            
            // Create decks array manually
            userver::formats::json::ValueBuilder decks_array;
            for (const auto& deck : decks) {
                userver::formats::json::ValueBuilder deck_obj;
                deck_obj["id"] = deck.id;
                deck_obj["name"] = deck.name;
                deck_obj["card_count"] = deck.card_ids.size();
                deck_obj["is_active"] = deck.is_active;
                decks_array.PushBack(deck_obj.ExtractValue());
            }
            response["decks"] = decks_array.ExtractValue();
            
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class CreateSessionHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-create-session";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string player_id = body["player_id"].As<std::string>();

            std::string session_id = session_manager->CreateSession(player_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["session_id"] = session_id;
            response["message"] = "Game session created successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class JoinSessionHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-join-session";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string session_id = body["session_id"].As<std::string>();
            std::string player_id = body["player_id"].As<std::string>();

            session_manager->JoinSession(session_id, player_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["message"] = "Joined game session successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class GetWaitingSessionsHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-waiting-sessions";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest&, userver::server::request::RequestContext&) const override {
        try {
            std::vector<GameSession> sessions = session_manager->GetWaitingSessions();

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            
            // Create sessions array manually
            userver::formats::json::ValueBuilder sessions_array;
            for (const auto& session : sessions) {
                userver::formats::json::ValueBuilder session_obj;
                session_obj["id"] = session.id;
                session_obj["player1_id"] = session.player1_id;
                session_obj["status"] = session.status;
                session_obj["created_at"] = TimePointToString(session.created_at);
                sessions_array.PushBack(session_obj.ExtractValue());
            }
            response["sessions"] = sessions_array.ExtractValue();
            
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class StartBattleHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-start-battle";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string session_id = body["session_id"].As<std::string>();
            std::string player1_deck_id = body["player1_deck_id"].As<std::string>();
            std::string player2_deck_id = body["player2_deck_id"].As<std::string>();

            battle_manager->StartBattle(session_id, player1_deck_id, player2_deck_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["message"] = "Battle started successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class GetBattleStateHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-battle-state";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            std::string session_id = request.GetArg("session_id");
            BattleState battle = battle_manager->GetBattleState(session_id);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["battle"]["session_id"] = battle.session_id;
            response["battle"]["current_turn"] = battle.current_turn;
            
            // Add player health
            for (const auto& [player, health] : battle.player_health) {
                response["battle"]["player_health"][player] = health;
            }
            
            // Add hands
            for (const auto& [player, hand] : battle.player_hands) {
                userver::formats::json::ValueBuilder hand_array;
                for (const auto& card : hand) {
                    userver::formats::json::ValueBuilder card_obj;
                    card_obj["id"] = card.id;
                    card_obj["name"] = card.name;
                    card_obj["attack"] = card.attack;
                    card_obj["defense"] = card.defense;
                    card_obj["cost"] = card.cost;
                    hand_array.PushBack(card_obj.ExtractValue());
                }
                response["battle"]["hands"][player] = hand_array.ExtractValue();
            }
            
            // Add fields
            for (const auto& [player, field] : battle.player_fields) {
                userver::formats::json::ValueBuilder field_array;
                for (const auto& card : field) {
                    userver::formats::json::ValueBuilder card_obj;
                    card_obj["id"] = card.id;
                    card_obj["name"] = card.name;
                    card_obj["attack"] = card.attack;
                    card_obj["defense"] = card.defense;
                    field_array.PushBack(card_obj.ExtractValue());
                }
                response["battle"]["fields"][player] = field_array.ExtractValue();
            }
            
            // Add battle log
            userver::formats::json::ValueBuilder battle_log_array;
            for (const auto& log_entry : battle.battle_log) {
                battle_log_array.PushBack(log_entry);
            }
            response["battle"]["battle_log"] = battle_log_array.ExtractValue();
            
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class PlayCardHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-play-card";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string session_id = body["session_id"].As<std::string>();
            std::string player_id = body["player_id"].As<std::string>();
            int hand_index = body["hand_index"].As<int>();

            battle_manager->PlayCard(session_id, player_id, hand_index);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["message"] = "Card played successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

class AttackHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-attack";
    using HttpHandlerBase::HttpHandlerBase;

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const override {
        try {
            auto body = userver::formats::json::FromString(request.RequestBody());
            std::string session_id = body["session_id"].As<std::string>();
            std::string attacker_id = body["attacker_id"].As<std::string>();
            int attacker_index = body["attacker_index"].As<int>();
            int target_index = body["target_index"].As<int>();

            battle_manager->Attack(session_id, attacker_id, attacker_index, target_index);

            userver::formats::json::ValueBuilder response;
            response["success"] = true;
            response["message"] = "Attack executed successfully";
            return userver::formats::json::ToString(response.ExtractValue());
        } catch (const std::exception& e) {
            userver::formats::json::ValueBuilder response;
            response["success"] = false;
            response["error"] = e.what();
            return userver::formats::json::ToString(response.ExtractValue());
        }
    }
};

} // namespace

int main(int argc, char* argv[]) {
    // Initialize managers
    user_manager = std::make_unique<UserManager>();
    deck_manager = std::make_unique<DeckManager>();
    session_manager = std::make_unique<GameSessionManager>();
    battle_manager = std::make_unique<BattleManager>();

    const auto component_list = userver::components::MinimalServerComponentList()
        .Append<HealthCheckHandler>()
        .Append<RegisterHandler>()
        .Append<LoginHandler>()
        .Append<UserProfileHandler>()
        .Append<CreateDeckHandler>()
        .Append<GetDecksHandler>()
        .Append<CreateSessionHandler>()
        .Append<JoinSessionHandler>()
        .Append<GetWaitingSessionsHandler>()
        .Append<StartBattleHandler>()
        .Append<GetBattleStateHandler>()
        .Append<PlayCardHandler>()
        .Append<AttackHandler>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
