set(SERVER_SOURCES
  src/main.cpp
  src/ConfigurableServer.cpp
  src/Router.cpp
)

add_executable(Server ${SERVER_SOURCES})

target_link_libraries(Server PRIVATE boost_system)
target_include_directories(Server PUBLIC include)
