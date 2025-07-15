#include <iostream>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/utils/datetime.hpp>
#include <filesystem>

// Include all our headers
#include "../include/types.hpp"
#include "../include/utils.hpp"
#include "../include/managers/user_manager.hpp"
#include "../include/managers/session_manager.hpp"
#include "../include/managers/battle_manager.hpp"
#include "../include/handlers/health_handler.hpp"
#include "../include/handlers/auth_handlers.hpp"
#include "../include/handlers/game_handlers.hpp"
#include "../include/handlers/game_ws_handler.hpp"
#include "sqlite_db.hpp"

namespace {

std::unique_ptr<cardbattle::UserManager> user_manager;
std::unique_ptr<cardbattle::GameSessionManager> session_manager;
std::unique_ptr<cardbattle::BattleManager> battle_manager;

} // namespace

// Define the global manager variables for the handlers
namespace cardbattle {
    UserManager* user_manager = nullptr;
    GameSessionManager* session_manager = nullptr;
    BattleManager* battle_manager = nullptr;
    }

int main(int argc, char* argv[]) {
    std::cout << "Server CWD: " << std::filesystem::current_path() << std::endl;
    const char* db_path = std::getenv("TEST_DB_PATH");
    if (!db_path) db_path = "cardbattle.db";
    
    std::filesystem::path db_path_obj(db_path);
    if (std::filesystem::exists(db_path_obj)) {
        std::cout << "Server DB absolute path: " << std::filesystem::absolute(db_path_obj) << std::endl;
    } else {
        std::cout << "Server DB absolute path: (not found)" << std::endl;
    }
    
    LOG_INFO() << "Server using DB path: " << db_path;
    SQLiteDB db(db_path);
    db.InitSchema();

    // Initialize managers
    user_manager = std::make_unique<cardbattle::UserManager>();
    user_manager->Init(&db);  // UserManager needs database for authentication
    session_manager = std::make_unique<cardbattle::GameSessionManager>();  // In-memory only
    battle_manager = std::make_unique<cardbattle::BattleManager>();  // In-memory only

    // Set the global references
    cardbattle::user_manager = user_manager.get();
    cardbattle::session_manager = session_manager.get();
    cardbattle::battle_manager = battle_manager.get();

    // Initialize WebSocket handler
    cardbattle::InitWebSocketHandler(battle_manager.get(), session_manager.get(), user_manager.get());

    const auto component_list = userver::components::MinimalServerComponentList()
        .Append<cardbattle::HealthCheckHandler>()
        .Append<cardbattle::RegisterHandler>()
        .Append<cardbattle::LoginHandler>()
        .Append<cardbattle::UserProfileHandler>()
        .Append<cardbattle::CreateSessionHandler>()
        .Append<cardbattle::JoinSessionHandler>()
        .Append<cardbattle::LeaveSessionHandler>()
        .Append<cardbattle::GetSessionsHandler>()
        .Append<cardbattle::BattleWebSocketHandler>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}