add_library(rwoperations STATIC
    rwoperations.h
    rwoperations.cpp
)

add_executable(server
    server/server.h
    server/server.cpp
    server/priority_request_queue.h
    server/priority_request_queue.cpp
    server/notification_scheduler.h
    server/notification_scheduler.cpp
    server/main.cpp
)
target_link_libraries(server
    PRIVATE
    rwoperations
    Threads::Threads
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