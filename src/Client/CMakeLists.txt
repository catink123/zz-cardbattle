set(CLIENT_SOURCES
  src/main.cpp
)

add_executable(Client ${CLIENT_SOURCES})

set(CMAKE_EXECUTABLE_SUFFIX ".html")
set_target_properties(Client PROPERTIES LINK_FLAGS "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/public/index.html")
target_link_libraries(Client PRIVATE raylib)
