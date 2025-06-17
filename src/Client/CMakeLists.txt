set(CLIENT_SOURCES
  src/main.cpp
)

add_executable(Client ${CLIENT_SOURCES})

set(CMAKE_EXECUTABLE_SUFFIX ".html")
target_compile_options(Client PUBLIC "-std=c++20")
target_link_options(Client PUBLIC "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/public/index.html")
target_link_libraries(Client PRIVATE raylib)
