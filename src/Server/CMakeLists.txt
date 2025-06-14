set(SERVER_SOURCES
  src/main.cpp
)

add_executable(Server ${SERVER_SOURCES})

target_link_libraries(Server PRIVATE boost_system)
