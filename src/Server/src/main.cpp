#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/utils/datetime.hpp>

// Include all our headers
#include "../include/types.hpp"
#include "../include/utils.hpp"
#include "../include/managers/user_manager.hpp"
#include "../include/managers/deck_manager.hpp"
#include "../include/managers/session_manager.hpp"
#include "../include/managers/battle_manager.hpp"
#include "../include/handlers/health_handler.hpp"
#include "../include/handlers/auth_handlers.hpp"
#include "../include/handlers/deck_handlers.hpp"
#include "../include/handlers/game_handlers.hpp"
#include "../include/handlers/game_ws_handler.hpp"
#include "sqlite_db.hpp"

namespace {

// Global managers (in a real app, these would be dependency injected)
std::unique_ptr<cardbattle::UserManager> user_manager;
std::unique_ptr<cardbattle::DeckManager> deck_manager;
std::unique_ptr<cardbattle::GameSessionManager> session_manager;
std::unique_ptr<cardbattle::BattleManager> battle_manager;

} // namespace

// Define the global manager variables for the handlers
namespace cardbattle {
    UserManager* user_manager = nullptr;
    DeckManager* deck_manager = nullptr;
    GameSessionManager* session_manager = nullptr;
    BattleManager* battle_manager = nullptr;
    }

int main(int argc, char* argv[]) {
    // Initialize SQLiteDB
    SQLiteDB db("cardbattle.db");
    db.InitSchema();

    // Initialize managers with DB
    user_manager = std::make_unique<cardbattle::UserManager>();
    user_manager->Init(&db);
    deck_manager = std::make_unique<cardbattle::DeckManager>();
    deck_manager->Init(&db);
    session_manager = std::make_unique<cardbattle::GameSessionManager>();
    session_manager->Init(&db);
    battle_manager = std::make_unique<cardbattle::BattleManager>();
    battle_manager->Init(&db);

    // Set the global references
    cardbattle::user_manager = user_manager.get();
    cardbattle::deck_manager = deck_manager.get();
    cardbattle::session_manager = session_manager.get();
    cardbattle::battle_manager = battle_manager.get();

    const auto component_list = userver::components::MinimalServerComponentList()
        .Append<cardbattle::HealthCheckHandler>()
        .Append<cardbattle::RegisterHandler>()
        .Append<cardbattle::LoginHandler>()
        .Append<cardbattle::UserProfileHandler>()
        .Append<cardbattle::CreateDeckHandler>()
        .Append<cardbattle::GetDecksHandler>()
        .Append<cardbattle::UpdateDeckHandler>()
        .Append<cardbattle::DeleteDeckHandler>()
        .Append<cardbattle::CreateSessionHandler>()
        .Append<cardbattle::JoinSessionHandler>()
        .Append<cardbattle::LeaveSessionHandler>()
        .Append<cardbattle::GetSessionsHandler>()
        .Append<cardbattle::SelectDeckHandler>()
        .Append<cardbattle::StartBattleHandler>()
        .Append<cardbattle::GetBattleStateHandler>()
        .Append<cardbattle::PlayCardHandler>()
        .Append<cardbattle::AttackHandler>()
        .Append<cardbattle::EndTurnHandler>()
        .Append<cardbattle::BattleWebSocketHandler>();

    // Set the global pointer for the WebSocket handler
    cardbattle::g_battle_ws_handler = static_cast<cardbattle::BattleWebSocketHandler*>(nullptr); // Will be set by the framework

    return userver::utils::DaemonMain(argc, argv, component_list);
}