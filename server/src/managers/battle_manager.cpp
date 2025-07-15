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

// Global managers (in a real app, these would be dependency injected)
extern GameSessionManager* session_manager;

BattleManager::BattleManager() {
    // Initialize with some default cards
    InitializeDefaultCards();
}

std::string BattleManager::StartBattle(const std::string& session_id) {
    try {
        auto session = session_manager->GetSession(session_id);
        // Ensure both players are present
        if (session.host_id.empty() || session.guest_id.empty()) {
            throw std::runtime_error("Cannot start battle: both host and guest must be present in the session");
        }
        
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
        
        // Initialize guest player
        PlayerState guest_state;
        guest_state.player_id = session.guest_id;
        guest_state.health = 30;
        guest_state.max_health = 30;
        guest_state.mana = 1;
        guest_state.max_mana = 1;
        guest_state.is_active = false;
        
        // Store player states in battle state first
        battle_state.players[session.host_id] = host_state;
        battle_state.players[session.guest_id] = guest_state;
        
        // Initialize decks for both players (after storing player states)
        InitializePlayerDeck(battle_state, session.host_id);
        InitializePlayerDeck(battle_state, session.guest_id);
        
        LOG_INFO() << "Before saving battle state for session " << session_id << ":";
        for (const auto& pair : battle_state.players) {
            const std::string& player_id = pair.first;
            const PlayerState& player = pair.second;
            LOG_INFO() << "  Player " << player_id << ": hand size=" << player.hand.size() 
                       << ", deck size=" << player.deck.size() 
                       << ", field size=" << player.field.size() 
                       << ", graveyard size=" << player.graveyard.size();
        }
        
        active_battles_[session_id] = battle_state;
        
        LOG_INFO() << "Battle started for session: " << session_id;
        
        return session_id;
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
        // Add creature to field (reset used_this_turn flag for new cards)
        card_to_play.used_this_turn = false;
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
    
    // Check if the card has already been used this turn
    if (attacker_card.used_this_turn) {
        throw std::runtime_error("Card has already been used this turn");
    }
    
    if (target_index >= 0 && target_index < static_cast<int>(opponent_state.field.size())) {
        // Attack opponent's creature
        Card& target_card = opponent_state.field[target_index];
        
        // Apply damage
        target_card.defense -= attacker_card.attack;
        attacker_card.defense -= target_card.attack;
        
        battle_state.last_action = attacker_id + "'s " + attacker_card.name + " attacked " + opponent_id + "'s " + target_card.name;
        
        // Mark the attacker card as used this turn
        attacker_card.used_this_turn = true;
        
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
        
        // Mark the attacker card as used this turn
        attacker_card.used_this_turn = true;
        
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
        
        // Reset used_this_turn flag for all cards on the field
        for (auto& card : player_state.field) {
            card.used_this_turn = false;
        }
        
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
    
    // Check for deck exhaustion (both players have no cards in deck and hand)
    bool both_players_exhausted = true;
    for (const auto& pair : battle_state.players) {
        const auto& player_state = pair.second;
        if (!player_state.deck.empty() || !player_state.hand.empty()) {
            both_players_exhausted = false;
            break;
        }
    }
    
    if (both_players_exhausted) {
        // Find the player with more health
        std::string winner_id;
        int max_health = -1;
        
        for (const auto& pair : battle_state.players) {
            const auto& player_state = pair.second;
            if (player_state.health > max_health) {
                max_health = player_state.health;
                winner_id = pair.first;
            }
        }
        
        // End the game with the player who has more health as winner
        EndGame(battle_state, winner_id);
        battle_state.last_action = "Game ended by deck exhaustion! " + winner_id + " wins with " + std::to_string(max_health) + " health!";
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
    
    const BattleState& state = it->second;
    LOG_INFO() << "GetBattleState for session " << session_id << ":";
    for (const auto& pair : state.players) {
        const std::string& player_id = pair.first;
        const PlayerState& player = pair.second;
        LOG_INFO() << "  Player " << player_id << ": hand size=" << player.hand.size() 
                   << ", deck size=" << player.deck.size() 
                   << ", field size=" << player.field.size() 
                   << ", graveyard size=" << player.graveyard.size();
    }
    
    return state;
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

void BattleManager::InitializePlayerDeck(BattleState& battle, const std::string& player_id) {
    // Use a simple prespecified deck for all players
    std::vector<std::string> deck_card_ids = {
        "card_001", "card_002", "card_003", "card_004", "card_005",
        "card_006", "card_007", "card_008", "card_009", "card_010"
    };
    
    // Convert card IDs to actual cards using default_cards_
    for (const auto& card_id : deck_card_ids) {
        auto card_it = std::find_if(default_cards_.begin(), default_cards_.end(),
                                   [&card_id](const Card& card) { return card.id == card_id; });
        if (card_it != default_cards_.end()) {
            battle.players[player_id].deck.push_back(*card_it);
        } else {
            LOG_ERROR() << "Card not found in default_cards_: " << card_id;
        }
    }
    
    // Shuffle the deck
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(battle.players[player_id].deck.begin(), battle.players[player_id].deck.end(), g);
    
    // Draw initial hand
    battle.players[player_id].hand = DrawInitialHand(battle.players[player_id].deck);
    LOG_INFO() << "Initialized deck for player " << player_id << ", deck size: " << battle.players[player_id].deck.size() << ", hand size: " << battle.players[player_id].hand.size();
}

void BattleManager::DrawCards(BattleState& battle, const std::string& player_id, int count) {
    auto& deck = battle.players[player_id].deck;
    auto& hand = battle.players[player_id].hand;
    
    for (int i = 0; i < count && !deck.empty(); ++i) {
        hand.push_back(deck.back());
        deck.pop_back();
    }
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

void BattleManager::Surrender(const std::string& session_id, const std::string& player_id) {
    auto& battle_state = active_battles_[session_id];
    
    // Find the opponent
    std::string opponent_id;
    for (const auto& pair : battle_state.players) {
        if (pair.first != player_id) {
            opponent_id = pair.first;
            break;
        }
    }
    
    // End the game with the opponent as winner
    EndGame(battle_state, opponent_id);
    battle_state.last_action = "Player " + player_id + " surrendered. " + opponent_id + " wins!";
    
    LOG_INFO() << "Player " << player_id << " surrendered. " << opponent_id << " wins!";
    
    // Save updated state
    SaveBattleState(session_id, battle_state);
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
            Card placeholder(card_id, "Unknown Card", "Card not found", 1, 1, 1, CardType::CREATURE);
            cards.push_back(placeholder);
        }
    }
    
    return cards;
}

std::vector<Card> BattleManager::DrawInitialHand(std::vector<Card>& deck) {
    std::vector<Card> hand;
    int cards_to_draw = std::min(4, static_cast<int>(deck.size()));
    
    // Draw cards
    for (int i = 0; i < cards_to_draw; i++) {
        hand.push_back(deck.back());
        deck.pop_back();
    }
    
    return hand;
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
    default_cards_.push_back(Card("card_001", "Fire Elemental", "A powerful fire creature", 5, 3, 4, CardType::CREATURE));
    default_cards_.push_back(Card("card_002", "Water Spirit", "A mystical water being", 3, 5, 3, CardType::CREATURE));
    default_cards_.push_back(Card("card_003", "Lightning Bolt", "Deal 3 damage to target", 0, 0, 2, CardType::SPELL));
    default_cards_.push_back(Card("card_004", "Dragon", "A mighty dragon", 8, 6, 7, CardType::CREATURE));
    default_cards_.push_back(Card("card_005", "Healing Potion", "Restore 4 health", 0, 0, 3, CardType::SPELL));
    default_cards_.push_back(Card("card_006", "Knight", "A noble warrior", 4, 4, 4, CardType::CREATURE));
    default_cards_.push_back(Card("card_007", "Magic Shield", "Gain 3 defense", 0, 0, 2, CardType::SPELL));
    default_cards_.push_back(Card("card_008", "Goblin", "A small but fierce creature", 2, 1, 1, CardType::CREATURE));
    default_cards_.push_back(Card("card_009", "Wizard", "A powerful spellcaster", 3, 2, 5, CardType::CREATURE));
    default_cards_.push_back(Card("card_010", "Forest Guardian", "Protector of nature", 6, 7, 6, CardType::CREATURE));
    LOG_INFO() << "Initialized default cards, count: " << default_cards_.size();
}

void BattleManager::SaveBattleState(const std::string& session_id, const BattleState& state) {
    // No database update, just save to memory
    // In a real app, you'd serialize to JSON and save to a file or database
    // For now, we'll just re-add it to the active_battles_ map
    LOG_INFO() << "SaveBattleState for session " << session_id << ":";
    for (const auto& pair : state.players) {
        const std::string& player_id = pair.first;
        const PlayerState& player = pair.second;
        LOG_INFO() << "  Saving player " << player_id << ": hand size=" << player.hand.size() 
                   << ", deck size=" << player.deck.size() 
                   << ", field size=" << player.field.size() 
                   << ", graveyard size=" << player.graveyard.size();
    }
    active_battles_[session_id] = state;
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
    // No database query needed here
    std::string player1_id = players_json["player1_id"].As<std::string>();
    std::string player2_id = players_json["player2_id"].As<std::string>();
    
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
            card.type = static_cast<CardType>(card_json["type"].As<int>());
            player.field.push_back(card);
        }
        
        state.players[player2_id] = player;
    }
    
    return state;
}

void BattleManager::UpdatePlayerStats(const std::string& player_id, bool won) {
    // No database update, just update in memory
    // In a real app, you'd update the user's wins/losses in the database
    // For now, we'll just print a message
    LOG_INFO() << "Updating player stats for " << player_id << ": " << (won ? "Won" : "Lost");
}

} // namespace cardbattle 