set(SERVER_SOURCES
  src/main.cpp
  src/utils.cpp
  src/sqlite_db.cpp
  src/managers/user_manager.cpp
  src/managers/deck_manager.cpp
  src/managers/session_manager.cpp
  src/managers/battle_manager.cpp
  src/handlers/health_handler.cpp
  src/handlers/auth_handlers.cpp
  src/handlers/deck_handlers.cpp
  src/handlers/game_handlers.cpp
  src/handlers/game_ws_handler.cpp
)

add_executable(Server ${SERVER_SOURCES})

target_include_directories(Server PRIVATE include)

target_link_libraries(Server PRIVATE userver-core sqlite3)

add_custom_command(TARGET Server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE_DIR:Server>/static_config.yaml
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/static_config.yaml
    $<TARGET_FILE_DIR:Server>/static_config.yaml
)
