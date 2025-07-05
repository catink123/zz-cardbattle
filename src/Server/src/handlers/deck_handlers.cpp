#include "../include/handlers/deck_handlers.hpp"
#include "../include/managers/deck_manager.hpp"
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/json/value.hpp>

namespace cardbattle {

// Global managers (in a real app, these would be dependency injected)
extern DeckManager* deck_manager;

std::string CreateDeckHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        // Extract user_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string user_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string name = body["name"].As<std::string>();
        
        auto card_ids_array = body["card_ids"];
        std::vector<std::string> card_ids;
        for (size_t i = 0; i < card_ids_array.GetSize(); ++i) {
            card_ids.push_back(card_ids_array[i].As<std::string>());
        }

        std::string deck_id = deck_manager->CreateDeck(user_id, name, card_ids);

        userver::formats::json::ValueBuilder response_builder;
        response_builder["success"] = true;
        response_builder["deck_id"] = deck_id;
        response_builder["message"] = "Deck created successfully";

        return userver::formats::json::ToString(response_builder.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string GetDecksHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    try {
        // Extract user_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string user_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto decks = deck_manager->GetUserDecks(user_id);

        userver::formats::json::ValueBuilder response_builder;
        response_builder["success"] = true;
        response_builder["decks"] = userver::formats::json::MakeArray();
        
        for (const auto& deck : decks) {
            userver::formats::json::ValueBuilder deck_obj;
            deck_obj["id"] = deck.id;
            deck_obj["name"] = deck.name;
            deck_obj["card_count"] = deck.card_ids.size();
            deck_obj["is_active"] = deck.is_active;
            response_builder["decks"].PushBack(deck_obj.ExtractValue());
        }

        return userver::formats::json::ToString(response_builder.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string UpdateDeckHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "PUT, OPTIONS");
    try {
        // Extract user_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string user_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string deck_id = body["deck_id"].As<std::string>();
        std::string name = body["name"].As<std::string>();
        
        auto card_ids_array = body["card_ids"];
        std::vector<std::string> card_ids;
        for (size_t i = 0; i < card_ids_array.GetSize(); ++i) {
            card_ids.push_back(card_ids_array[i].As<std::string>());
        }

        // TODO: Implement deck update in DeckManager
        // For now, just return success
        userver::formats::json::ValueBuilder response_builder;
        response_builder["success"] = true;
        response_builder["message"] = "Deck updated successfully";

        return userver::formats::json::ToString(response_builder.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

std::string DeleteDeckHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "DELETE, OPTIONS");
    try {
        // Extract user_id from Authorization token
        std::string auth_header = request.GetHeader("Authorization");
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
            throw std::runtime_error("Invalid or missing authorization token");
        }
        std::string user_id = auth_header.substr(7); // Remove "Bearer " prefix
        
        auto body = userver::formats::json::FromString(request.RequestBody());
        std::string deck_id = body["deck_id"].As<std::string>();

        // TODO: Implement deck deletion in DeckManager
        // For now, just return success
        userver::formats::json::ValueBuilder response_builder;
        response_builder["success"] = true;
        response_builder["message"] = "Deck deleted successfully";

        return userver::formats::json::ToString(response_builder.ExtractValue());
    } catch (const std::exception& e) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;
        response["error"] = e.what();
        return userver::formats::json::ToString(response.ExtractValue());
    }
}

} // namespace cardbattle 