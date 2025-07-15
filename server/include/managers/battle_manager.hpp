#pragma once

#include "../types.hpp"
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

public:
    BattleManager();
    std::string StartBattle(const std::string& session_id);
    void PlayCard(const std::string& session_id, const std::string& player_id, int hand_index);
    void Attack(const std::string& session_id, const std::string& attacker_id, int attacker_index, int target_index);
    void EndTurn(const std::string& session_id, const std::string& player_id);
    void Surrender(const std::string& session_id, const std::string& player_id);
    void EndGame(BattleState& battle_state, const std::string& winner_id);
    BattleState GetBattleState(const std::string& session_id);
    void EndBattle(const std::string& session_id);
    std::string GenerateId();
    void SaveBattleState(const std::string& session_id, const BattleState& state);

private:
    void InitializeDefaultCards();
    void InitializePlayerDeck(BattleState& battle, const std::string& player_id);
    void DrawCards(BattleState& battle, const std::string& player_id, int count);
    void HandleSpellEffect(BattleState& battle_state, const std::string& player_id, const Card& spell);
    std::vector<Card> LoadDeckCards(const std::vector<std::string>& card_ids);
    std::vector<Card> DrawInitialHand(std::vector<Card>& deck);
    std::string BattleStateToJson(const BattleState& state);
    BattleState BattleStateFromJson(const std::string& json_str);
    void UpdatePlayerStats(const std::string& player_id, bool won);
};

} // namespace cardbattle 