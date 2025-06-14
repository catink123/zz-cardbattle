cmake_minimum_required(VERSION 4.0)
project(zz-cardbattle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Debug as none was specified")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(EMSCRIPTEN_BUILD TRUE)
  message(STATUS "Configuring for Emscripten")
  set(PLATFORM Web CACHE STRING "Platform to build for")
else()
  set(EMSCRIPTEN_BUILD FALSE)
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

if (EMSCRIPTEN_BUILD)
  include(FetchContent)

  set(RAYLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "Disable examples")
  set(RAYLIB_GLFW_BUILD_X11 OFF CACHE INTERNAL "Disable GLFW X11 build")

  FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib
    GIT_TAG 5.5
  )
  FetchContent_MakeAvailable(raylib)
else()
  find_package(Boost 1.88.0 REQUIRED COMPONENTS system)
endif()

add_subdirectory(src)
