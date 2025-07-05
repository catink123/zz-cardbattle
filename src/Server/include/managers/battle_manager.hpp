#pragma once

#include "../types.hpp"
#include "deck_manager.hpp"
#include "session_manager.hpp"
#include <unordered_map>
#include <random>
#include "../../src/sqlite_db.hpp"
#include <string>
#include <vector>

namespace cardbattle {

class BattleManager {
private:
    std::unordered_map<std::string, BattleState> active_battles_;
    std::vector<Card> default_cards_;
    SQLiteDB* db_;

public:
    BattleManager();
    void Init(SQLiteDB* sqlite_db);
    void StartBattle(const std::string& session_id, const std::string& player1_deck_id, const std::string& player2_deck_id);
    void PlayCard(const std::string& session_id, const std::string& player_id, int hand_index);
    void Attack(const std::string& session_id, const std::string& attacker_id, int attacker_index, int target_index);
    void EndTurn(const std::string& session_id, const std::string& player_id);
    BattleState GetBattleState(const std::string& session_id);
    std::vector<BattleState> GetAllBattles();
    void EndBattle(const std::string& session_id);
    std::string GenerateId();

private:
    void InitializeDefaultCards();
    void InitializeCardDatabase();
    void InitializePlayerDeck(BattleState& battle, const std::string& player_id, const std::string& deck_id);
    void DrawCards(BattleState& battle, const std::string& player_id, int count);
    void ApplySpellEffect(BattleState& state, const std::string& player_id, const Card& spell);
    void HandleSpellEffect(BattleState& battle_state, const std::string& player_id, const Card& spell);
    void EndGame(BattleState& battle_state, const std::string& winner_id);
    std::vector<Card> LoadDeckCards(const std::vector<std::string>& card_ids);
    std::vector<Card> DrawInitialHand(std::vector<Card>& deck);
    void SaveBattleState(const std::string& session_id, const BattleState& state);
    std::string BattleStateToJson(const BattleState& state);
    BattleState BattleStateFromJson(const std::string& json_str);
    void UpdatePlayerStats(const std::string& player_id, bool won);
};

} // namespace cardbattle 