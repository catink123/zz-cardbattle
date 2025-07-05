#include "../include/managers/battle_manager.hpp"
#include "../include/utils.hpp"
#include "../sqlite_db.hpp"
#include "../include/types.hpp"
#include <algorithm>
#include <random>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>

namespace cardbattle {

static SQLiteDB* db = nullptr;

// Global managers (in a real app, these would be dependency injected)
extern DeckManager* deck_manager;
extern GameSessionManager* session_manager;

BattleManager::BattleManager() : db_(nullptr) {
    // Initialize with some default cards
    InitializeDefaultCards();
}

void BattleManager::Init(SQLiteDB* sqlite_db) {
    db = sqlite_db;
    db_ = sqlite_db;
    InitializeCardDatabase();
    LOG_INFO() << "BattleManager initialized";
}

void BattleManager::StartBattle(const std::string& session_id, const std::string& player1_deck_id, const std::string& player2_deck_id) {
    try {
        auto session = session_manager->GetSession(session_id);
        
        // Create battle state
        BattleState battle_state;
        battle_state.session_id = session_id;
        battle_state.current_turn = session.host_id; // Host goes first
        battle_state.turn_number = 1;
        battle_state.is_finished = false;
        battle_state.last_action = "Battle started";
        
        // Initialize host player
        PlayerState host_state;
        host_state.player_id = session.host_id;
        host_state.health = 30;
        host_state.max_health = 30;
        host_state.mana = 1; // Start with 1 mana
        host_state.max_mana = 1;
        host_state.is_active = true;
        
        // Load host deck
        auto host_deck = deck_manager->GetDeck(player1_deck_id);
        host_state.deck = LoadDeckCards(host_deck.card_ids);
        host_state.hand = DrawInitialHand(host_state.deck);
        
        // Initialize guest player
        PlayerState guest_state;
        guest_state.player_id = session.guest_id;
        guest_state.health = 30;
        guest_state.max_health = 30;
        guest_state.mana = 1;
        guest_state.max_mana = 1;
        guest_state.is_active = false;
        
        // Load guest deck
        auto guest_deck = deck_manager->GetDeck(player2_deck_id);
        guest_state.deck = LoadDeckCards(guest_deck.card_ids);
        guest_state.hand = DrawInitialHand(guest_state.deck);
        
        // Store battle state
        battle_state.players[session.host_id] = host_state;
        battle_state.players[session.guest_id] = guest_state;
        
        active_battles_[session_id] = battle_state;
        
        LOG_INFO() << "Battle started for session: " << session_id;
        
        // Save battle state
        std::string state_json = BattleStateToJson(battle_state);
        std::string now = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string battle_id = GenerateId();
        
        std::string sql = "INSERT INTO battles (id, session_id, state, last_update) VALUES (?, ?, ?, ?)";
        if (!db->Execute(sql, {battle_id, session_id, state_json, now})) {
            throw std::runtime_error("Battle creation failed: " + db->GetLastError());
        }
        
        // Update session status
        sql = "UPDATE sessions SET status = 'active' WHERE id = ?";
        db->Execute(sql, {session_id});
    } catch (const std::exception& e) {
        LOG_ERROR() << "Failed to start battle: " << e.what();
        throw;
    }
}

void BattleManager::PlayCard(const std::string& session_id, const std::string& player_id, int hand_index) {
    auto& battle_state = active_battles_[session_id];
    auto& player_state = battle_state.players[player_id];
    
    // Check if it's the player's turn
    if (battle_state.current_turn != player_id) {
        throw std::runtime_error("Not your turn");
    }
    
    // Check if hand index is valid
    if (hand_index < 0 || hand_index >= static_cast<int>(player_state.hand.size())) {
        throw std::runtime_error("Invalid hand index");
    }
    
    Card card_to_play = player_state.hand[hand_index];
    
    // Check if player has enough mana
    if (player_state.mana < card_to_play.mana_cost) {
        throw std::runtime_error("Not enough mana");
    }
    
    // Remove card from hand
    player_state.hand.erase(player_state.hand.begin() + hand_index);
    
    // Spend mana
    player_state.mana -= card_to_play.mana_cost;
    
    // Play the card based on its type
    if (card_to_play.type == CardType::CREATURE) {
        // Add creature to field
        player_state.field.push_back(card_to_play);
        battle_state.last_action = player_id + " played " + card_to_play.name;
    } else if (card_to_play.type == CardType::SPELL) {
        // Handle spell effects
        HandleSpellEffect(battle_state, player_id, card_to_play);
    }
    
    LOG_INFO() << "Card played: " << card_to_play.name << " by " << player_id;
    
    // Save updated state
    SaveBattleState(session_id, battle_state);
}

void BattleManager::Attack(const std::string& session_id, const std::string& attacker_id, int attacker_index, int target_index) {
    auto& battle_state = active_battles_[session_id];
    auto& attacker_state = battle_state.players[attacker_id];
    
    // Check if it's the attacker's turn
    if (battle_state.current_turn != attacker_id) {
        throw std::runtime_error("Not your turn");
    }
    
    // Check if attacker index is valid
    if (attacker_index < 0 || attacker_index >= static_cast<int>(attacker_state.field.size())) {
        throw std::runtime_error("Invalid attacker index");
    }
    
    // Find the opponent
    std::string opponent_id;
    for (const auto& pair : battle_state.players) {
        if (pair.first != attacker_id) {
            opponent_id = pair.first;
            break;
        }
    }
    
    auto& opponent_state = battle_state.players[opponent_id];
    
    Card& attacker_card = attacker_state.field[attacker_index];
    
    if (target_index >= 0 && target_index < static_cast<int>(opponent_state.field.size())) {
        // Attack opponent's creature
        Card& target_card = opponent_state.field[target_index];
        
        // Apply damage
        target_card.defense -= attacker_card.attack;
        attacker_card.defense -= target_card.attack;
        
        battle_state.last_action = attacker_id + "'s " + attacker_card.name + " attacked " + opponent_id + "'s " + target_card.name;
        
        // Remove dead creatures
        if (target_card.defense <= 0) {
            opponent_state.field.erase(opponent_state.field.begin() + target_index);
            opponent_state.graveyard.push_back(target_card);
        }
        
        if (attacker_card.defense <= 0) {
            attacker_state.field.erase(attacker_state.field.begin() + attacker_index);
            attacker_state.graveyard.push_back(attacker_card);
        }
        
    } else {
        // Attack opponent directly
        opponent_state.health -= attacker_card.attack;
        battle_state.last_action = attacker_id + "'s " + attacker_card.name + " attacked " + opponent_id + " directly";
        
        // Check for game over
        if (opponent_state.health <= 0) {
            EndGame(battle_state, attacker_id);
        }
    }
    
    LOG_INFO() << "Attack executed: " << battle_state.last_action;
    
    // Save updated state
    SaveBattleState(session_id, battle_state);
}

void BattleManager::EndTurn(const std::string& session_id, const std::string& player_id) {
    auto& battle_state = active_battles_[session_id];
    
    // Check if it's the player's turn
    if (battle_state.current_turn != player_id) {
        throw std::runtime_error("Not your turn");
    }
    
    // Find the next player
    std::string next_player_id;
    for (const auto& pair : battle_state.players) {
        if (pair.first != player_id) {
            next_player_id = pair.first;
            break;
        }
    }
    
    // Switch turns
    battle_state.current_turn = next_player_id;
    battle_state.turn_number++;
    
    // Update player states for new turn
    for (auto& pair : battle_state.players) {
        auto& player_state = pair.second;
        player_state.is_active = (pair.first == next_player_id);
        
        if (player_state.is_active) {
            // Increase mana for the active player
            player_state.max_mana = std::min(10, battle_state.turn_number);
            player_state.mana = player_state.max_mana;
            
            // Draw a card
            if (!player_state.deck.empty()) {
                player_state.hand.push_back(player_state.deck.back());
                player_state.deck.pop_back();
            }
        }
    }
    
    battle_state.last_action = "Turn ended, " + next_player_id + "'s turn";
    
    LOG_INFO() << "Turn ended: " << battle_state.last_action;
    
    // Save updated state
    SaveBattleState(session_id, battle_state);
}

BattleState BattleManager::GetBattleState(const std::string& session_id) {
    auto it = active_battles_.find(session_id);
    if (it == active_battles_.end()) {
        throw std::runtime_error("Battle not found for session: " + session_id);
    }
    return it->second;
}

std::string BattleManager::GenerateId() {
    static const char* chars = "0123456789abcdef";
    std::string id;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) id += '-';
        id += chars[rand() % 16];
    }
    return id;
}

void BattleManager::InitializeCardDatabase() {
    // This method is now handled by InitializeDefaultCards()
    LOG_INFO() << "Card database initialized";
}

void BattleManager::InitializePlayerDeck(BattleState& battle, const std::string& player_id, const std::string& deck_id) {
    Deck deck = deck_manager->GetDeck(deck_id);
    
    // Convert card IDs to actual cards using default_cards_
    for (const auto& card_id : deck.card_ids) {
        auto card_it = std::find_if(default_cards_.begin(), default_cards_.end(),
                                   [&card_id](const Card& card) { return card.id == card_id; });
        if (card_it != default_cards_.end()) {
            battle.players[player_id].deck.push_back(*card_it);
        }
    }
    
    // Shuffle the deck
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(battle.players[player_id].deck.begin(), battle.players[player_id].deck.end(), g);
}

void BattleManager::DrawCards(BattleState& battle, const std::string& player_id, int count) {
    auto& deck = battle.players[player_id].deck;
    auto& hand = battle.players[player_id].hand;
    
    for (int i = 0; i < count && !deck.empty(); ++i) {
        hand.push_back(deck.back());
        deck.pop_back();
    }
}

void BattleManager::ApplySpellEffect(BattleState& state, const std::string& player_id, const Card& spell) {
    if (spell.name == "Lightning Bolt") {
        // Deal 6 damage to opponent
        std::string opponent_id;
        for (const auto& pair : state.players) {
            if (pair.first != player_id) {
                opponent_id = pair.first;
                break;
            }
        }
        state.players[opponent_id].health -= 6;
    } else if (spell.name == "Healing Potion") {
        // Heal 5 health
        state.players[player_id].health = std::min(state.players[player_id].max_health, 
                                                  state.players[player_id].health + 5);
    } else if (spell.name == "Mana Crystal") {
        // Gain 2 mana
        state.players[player_id].mana = std::min(state.players[player_id].max_mana + 2, 
                                                state.players[player_id].mana + 2);
    }
}

void BattleManager::SaveBattleState(const std::string& session_id, const BattleState& state) {
    std::string state_json = BattleStateToJson(state);
    std::string now = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    std::string sql = "UPDATE battles SET state = ?, last_update = ? WHERE session_id = ?";
    db->Execute(sql, {state_json, now, session_id});
}

std::string BattleManager::BattleStateToJson(const BattleState& state) {
    userver::formats::json::ValueBuilder builder;
    builder["session_id"] = state.session_id;
    builder["current_turn"] = state.current_turn;
    builder["turn_number"] = state.turn_number;
    builder["winner"] = state.winner;
    builder["is_finished"] = state.is_finished;
    builder["last_action"] = state.last_action;
    
    userver::formats::json::ValueBuilder players_builder;
    for (const auto& pair : state.players) {
        userver::formats::json::ValueBuilder player_builder;
        const PlayerState& player = pair.second;
        
        player_builder["player_id"] = player.player_id;
        player_builder["health"] = player.health;
        player_builder["max_health"] = player.max_health;
        player_builder["mana"] = player.mana;
        player_builder["max_mana"] = player.max_mana;
        player_builder["is_active"] = player.is_active;
        
        // Convert cards to JSON
        userver::formats::json::ValueBuilder hand_builder;
        for (const auto& card : player.hand) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["description"] = card.description;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["rarity"] = static_cast<int>(card.rarity);
            card_builder["type"] = static_cast<int>(card.type);
            hand_builder.PushBack(card_builder.ExtractValue());
        }
        player_builder["hand"] = hand_builder.ExtractValue();
        
        // Similar for field, deck, graveyard...
        userver::formats::json::ValueBuilder field_builder;
        for (const auto& card : player.field) {
            userver::formats::json::ValueBuilder card_builder;
            card_builder["id"] = card.id;
            card_builder["name"] = card.name;
            card_builder["description"] = card.description;
            card_builder["attack"] = card.attack;
            card_builder["defense"] = card.defense;
            card_builder["mana_cost"] = card.mana_cost;
            card_builder["rarity"] = static_cast<int>(card.rarity);
            card_builder["type"] = static_cast<int>(card.type);
            field_builder.PushBack(card_builder.ExtractValue());
        }
        player_builder["field"] = field_builder.ExtractValue();
        
        players_builder[pair.first] = player_builder.ExtractValue();
    }
    builder["players"] = players_builder.ExtractValue();
    
    return userver::formats::json::ToString(builder.ExtractValue());
}

BattleState BattleManager::BattleStateFromJson(const std::string& json_str) {
    BattleState state;
    auto json = userver::formats::json::FromString(json_str);
    
    state.session_id = json["session_id"].As<std::string>();
    state.current_turn = json["current_turn"].As<std::string>();
    state.turn_number = json["turn_number"].As<int>();
    state.winner = json["winner"].As<std::string>();
    state.is_finished = json["is_finished"].As<bool>();
    state.last_action = json["last_action"].As<std::string>();
    
    auto players_json = json["players"];
    
    // For now, assume we know the player IDs from the session
    // In a real implementation, you'd store the player IDs in the battle state
    std::string sql = "SELECT host_id, guest_id FROM sessions WHERE id = ?";
    std::vector<std::vector<std::string>> results;
    if (db->Query(sql, {state.session_id}, results) && !results.empty()) {
        std::string player1_id = results[0][0];
        std::string player2_id = results[0][1];
        
        // Load player 1 data
        if (players_json.HasMember(player1_id)) {
            auto player_data = players_json[player1_id];
            PlayerState player;
            
            player.player_id = player_data["player_id"].As<std::string>();
            player.health = player_data["health"].As<int>();
            player.max_health = player_data["max_health"].As<int>();
            player.mana = player_data["mana"].As<int>();
            player.max_mana = player_data["max_mana"].As<int>();
            player.is_active = player_data["is_active"].As<bool>();
            
            // Load hand cards
            auto hand_json = player_data["hand"];
            for (const auto& card_json : hand_json) {
                Card card;
                card.id = card_json["id"].As<std::string>();
                card.name = card_json["name"].As<std::string>();
                card.description = card_json["description"].As<std::string>();
                card.attack = card_json["attack"].As<int>();
                card.defense = card_json["defense"].As<int>();
                card.mana_cost = card_json["mana_cost"].As<int>();
                card.rarity = static_cast<CardRarity>(card_json["rarity"].As<int>());
                card.type = static_cast<CardType>(card_json["type"].As<int>());
                player.hand.push_back(card);
            }
            
            // Load field cards
            auto field_json = player_data["field"];
            for (const auto& card_json : field_json) {
                Card card;
                card.id = card_json["id"].As<std::string>();
                card.name = card_json["name"].As<std::string>();
                card.description = card_json["description"].As<std::string>();
                card.attack = card_json["attack"].As<int>();
                card.defense = card_json["defense"].As<int>();
                card.mana_cost = card_json["mana_cost"].As<int>();
                card.rarity = static_cast<CardRarity>(card_json["rarity"].As<int>());
                card.type = static_cast<CardType>(card_json["type"].As<int>());
                player.field.push_back(card);
            }
            
            state.players[player1_id] = player;
        }
        
        // Load player 2 data
        if (players_json.HasMember(player2_id)) {
            auto player_data = players_json[player2_id];
            PlayerState player;
            
            player.player_id = player_data["player_id"].As<std::string>();
            player.health = player_data["health"].As<int>();
            player.max_health = player_data["max_health"].As<int>();
            player.mana = player_data["mana"].As<int>();
            player.max_mana = player_data["max_mana"].As<int>();
            player.is_active = player_data["is_active"].As<bool>();
            
            // Load hand cards
            auto hand_json = player_data["hand"];
            for (const auto& card_json : hand_json) {
                Card card;
                card.id = card_json["id"].As<std::string>();
                card.name = card_json["name"].As<std::string>();
                card.description = card_json["description"].As<std::string>();
                card.attack = card_json["attack"].As<int>();
                card.defense = card_json["defense"].As<int>();
                card.mana_cost = card_json["mana_cost"].As<int>();
                card.rarity = static_cast<CardRarity>(card_json["rarity"].As<int>());
                card.type = static_cast<CardType>(card_json["type"].As<int>());
                player.hand.push_back(card);
            }
            
            // Load field cards
            auto field_json = player_data["field"];
            for (const auto& card_json : field_json) {
                Card card;
                card.id = card_json["id"].As<std::string>();
                card.name = card_json["name"].As<std::string>();
                card.description = card_json["description"].As<std::string>();
                card.attack = card_json["attack"].As<int>();
                card.defense = card_json["defense"].As<int>();
                card.mana_cost = card_json["mana_cost"].As<int>();
                card.rarity = static_cast<CardRarity>(card_json["rarity"].As<int>());
                card.type = static_cast<CardType>(card_json["type"].As<int>());
                player.field.push_back(card);
            }
            
            state.players[player2_id] = player;
        }
    }
    
    return state;
}

void BattleManager::UpdatePlayerStats(const std::string& player_id, bool won) {
    std::string sql;
    if (won) {
        sql = "UPDATE users SET wins = wins + 1 WHERE id = ?";
    } else {
        sql = "UPDATE users SET losses = losses + 1 WHERE id = ?";
    }
    db->Execute(sql, {player_id});
}

void BattleManager::HandleSpellEffect(BattleState& battle_state, const std::string& player_id, const Card& spell) {
    if (spell.name == "Lightning Bolt") {
        // Deal 3 damage to opponent
        std::string opponent_id;
        for (const auto& pair : battle_state.players) {
            if (pair.first != player_id) {
                opponent_id = pair.first;
                break;
            }
        }
        
        auto& opponent_state = battle_state.players[opponent_id];
        opponent_state.health -= 3;
        
        if (opponent_state.health <= 0) {
            EndGame(battle_state, player_id);
        }
        
    } else if (spell.name == "Healing Potion") {
        // Restore 4 health
        auto& player_state = battle_state.players[player_id];
        player_state.health = std::min(player_state.max_health, player_state.health + 4);
        
    } else if (spell.name == "Magic Shield") {
        // Gain 3 defense (add to player's health)
        auto& player_state = battle_state.players[player_id];
        player_state.health = std::min(player_state.max_health, player_state.health + 3);
    }
}

void BattleManager::EndGame(BattleState& battle_state, const std::string& winner_id) {
    battle_state.winner = winner_id;
    battle_state.is_finished = true;
    battle_state.last_action = "Game over! " + winner_id + " wins!";
    
    LOG_INFO() << "Game ended: " << battle_state.last_action;
}

std::vector<Card> BattleManager::LoadDeckCards(const std::vector<std::string>& card_ids) {
    std::vector<Card> cards;
    
    for (const auto& card_id : card_ids) {
        // Try to find the card in default cards first
        auto it = std::find_if(default_cards_.begin(), default_cards_.end(),
                              [&card_id](const Card& card) { return card.id == card_id; });
        
        if (it != default_cards_.end()) {
            cards.push_back(*it);
        } else {
            // If not found, create a placeholder card
            Card placeholder(card_id, "Unknown Card", "Card not found", 1, 1, 1, CardRarity::COMMON, CardType::CREATURE);
            cards.push_back(placeholder);
        }
    }
    
    return cards;
}

std::vector<Card> BattleManager::DrawInitialHand(std::vector<Card>& deck) {
    std::vector<Card> hand;
    int cards_to_draw = std::min(4, static_cast<int>(deck.size()));
    
    // Shuffle deck
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);
    
    // Draw cards
    for (int i = 0; i < cards_to_draw; i++) {
        hand.push_back(deck.back());
        deck.pop_back();
    }
    
    return hand;
}

std::vector<BattleState> BattleManager::GetAllBattles() {
    std::vector<BattleState> battles;
    for (const auto& pair : active_battles_) {
        battles.push_back(pair.second);
    }
    return battles;
}

void BattleManager::EndBattle(const std::string& session_id) {
    auto it = active_battles_.find(session_id);
    if (it != active_battles_.end()) {
        active_battles_.erase(it);
        LOG_INFO() << "Battle ended for session: " << session_id;
    }
}

void BattleManager::InitializeDefaultCards() {
    default_cards_.clear();
    default_cards_.push_back(Card("card_001", "Fire Elemental", "A powerful fire creature", 5, 3, 4, CardRarity::RARE, CardType::CREATURE));
    default_cards_.push_back(Card("card_002", "Water Spirit", "A mystical water being", 3, 5, 3, CardRarity::COMMON, CardType::CREATURE));
    default_cards_.push_back(Card("card_003", "Lightning Bolt", "Deal 3 damage to target", 0, 0, 2, CardRarity::COMMON, CardType::SPELL));
    default_cards_.push_back(Card("card_004", "Dragon", "A mighty dragon", 8, 6, 7, CardRarity::LEGENDARY, CardType::CREATURE));
    default_cards_.push_back(Card("card_005", "Healing Potion", "Restore 4 health", 0, 0, 3, CardRarity::COMMON, CardType::SPELL));
    default_cards_.push_back(Card("card_006", "Knight", "A noble warrior", 4, 4, 4, CardRarity::RARE, CardType::CREATURE));
    default_cards_.push_back(Card("card_007", "Magic Shield", "Gain 3 defense", 0, 0, 2, CardRarity::COMMON, CardType::SPELL));
    default_cards_.push_back(Card("card_008", "Goblin", "A small but fierce creature", 2, 1, 1, CardRarity::COMMON, CardType::CREATURE));
    default_cards_.push_back(Card("card_009", "Wizard", "A powerful spellcaster", 3, 2, 5, CardRarity::RARE, CardType::CREATURE));
    default_cards_.push_back(Card("card_010", "Forest Guardian", "Protector of nature", 6, 7, 6, CardRarity::EPIC, CardType::CREATURE));
    // Add more cards as needed for a robust test set
}

} // namespace cardbattle 