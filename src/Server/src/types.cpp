#include "../include/types.hpp"
#include <userver/formats/json/value_builder.hpp>

namespace cardbattle {

userver::formats::json::Value BattleState::ToJson() const {
    userver::formats::json::ValueBuilder builder;
    builder["session_id"] = session_id;
    builder["current_turn"] = current_turn;
    // Player health
    for (const auto& [pid, hp] : player_health) {
        builder["player_health"][pid] = hp;
    }
    // Player hands
    for (const auto& [pid, hand] : player_hands) {
        userver::formats::json::ValueBuilder hand_array;
        for (const auto& card : hand) {
            userver::formats::json::ValueBuilder card_obj;
            card_obj["id"] = card.id;
            card_obj["name"] = card.name;
            card_obj["attack"] = card.attack;
            card_obj["defense"] = card.defense;
            card_obj["cost"] = card.cost;
            card_obj["rarity"] = card.rarity;
            card_obj["type"] = card.type;
            hand_array.PushBack(card_obj.ExtractValue());
        }
        builder["player_hands"][pid] = hand_array.ExtractValue();
    }
    // Player fields
    for (const auto& [pid, field] : player_fields) {
        userver::formats::json::ValueBuilder field_array;
        for (const auto& card : field) {
            userver::formats::json::ValueBuilder card_obj;
            card_obj["id"] = card.id;
            card_obj["name"] = card.name;
            card_obj["attack"] = card.attack;
            card_obj["defense"] = card.defense;
            card_obj["cost"] = card.cost;
            card_obj["rarity"] = card.rarity;
            card_obj["type"] = card.type;
            field_array.PushBack(card_obj.ExtractValue());
        }
        builder["player_fields"][pid] = field_array.ExtractValue();
    }
    // Player decks (only card ids)
    for (const auto& [pid, deck] : player_decks) {
        userver::formats::json::ValueBuilder deck_array;
        for (const auto& card : deck) {
            deck_array.PushBack(card.id);
        }
        builder["player_decks"][pid] = deck_array.ExtractValue();
    }
    // Battle log
    userver::formats::json::ValueBuilder log_array;
    for (const auto& entry : battle_log) {
        log_array.PushBack(entry);
    }
    builder["battle_log"] = log_array.ExtractValue();
    return builder.ExtractValue();
}

} // namespace cardbattle 