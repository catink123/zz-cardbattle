cmake_minimum_required(VERSION 3.22)
project(zzcb-server)

# Enable testing
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Debug as none was specified")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

find_program(CCACHE ccache)
if (CCACHE)
  message(STATUS "Using ccache")
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
else()
  message(STATUS "Ccache not found")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (PROJECT_IS_TOP_LEVEL AND UNIX)
    # Create symlink to compile_commands.json for IDE to pick it up
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    )
endif()

# Libraries
include(FetchContent)

# Configure Abseil
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TEST_HELPERS OFF CACHE BOOL "" FORCE)
set(ABSL_USE_GOOGLETEST_HEAD OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  abseil-cpp
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20250512.1
)
FetchContent_MakeAvailable(abseil-cpp)

# Configure re2 to not export targets when using FetchContent
set(RE2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(RE2_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  re2
  GIT_REPOSITORY https://github.com/google/re2.git
  GIT_TAG 2024-07-02
)
FetchContent_MakeAvailable(re2)

set(USERVER_FEATURE_GRPC OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_CLICKHOUSE OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_POSTGRESQL OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_MONGODB OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_REDIS OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_RABBITMQ OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_MYSQL OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_DOCKER OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_CRYPTOPP OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_METRICS_PROMETHEUS OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_TESTSUITE ON CACHE BOOL "" FORCE)
set(USERVER_FEATURE_UNIVERSAL OFF CACHE BOOL "" FORCE)
set(USERVER_FEATURE_YDB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  userver
  GIT_REPOSITORY https://github.com/userver-framework/userver.git
  GIT_TAG v2.10
)
FetchContent_MakeAvailable(userver)

userver_setup_environment()

# Server sources
set(SERVER_SOURCES
  src/main.cpp
  src/utils.cpp
  src/sqlite_db.cpp
  src/managers/user_manager.cpp
  src/managers/session_manager.cpp
  src/managers/battle_manager.cpp
  src/handlers/health_handler.cpp
  src/handlers/auth_handlers.cpp
  src/handlers/game_handlers.cpp
  src/handlers/game_ws_handler.cpp
)

add_executable(Server ${SERVER_SOURCES})

target_include_directories(Server PRIVATE include)

target_link_libraries(Server PRIVATE userver-core sqlite3)

add_custom_command(TARGET Server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE_DIR:Server>/static_config.yaml
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config.yaml
    $<TARGET_FILE_DIR:Server>/static_config.yaml
)

userver_testsuite_add(
  SERVICE_TARGET
  Server
  WORKING_DIRECTORY
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
  PYTHON_BINARY
  ${TESTSUITE_PYTHON_BINARY}
  PYTEST_ARGS
  --service-config=${CMAKE_CURRENT_SOURCE_DIR}/configs/static_config_test.yaml
  --service-binary=${CMAKE_CURRENT_BINARY_DIR}/Server
) 