add_library(rwoperations STATIC
    rwoperations.h
    rwoperations.cpp
)

add_executable(server
    server/server.h
    server/server.cpp
    server/main.cpp
)
target_link_libraries(server
    PRIVATE
    rwoperations
)

add_executable(client
    client/client.h
    client/client.cpp
    client/main.cpp
)
target_link_libraries(client
    PRIVATE
    rwoperations
)