#pragma once

#include "defines.h"
#include <sys/un.h>

class Server
{
public:
    Server();
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    void bind();
    void listen();
    int accept();
    void run();

private:
    void handleClient(int client_fd);
    bool sendResponse(int client_fd, int32_t status, const char *text, uint32_t text_size);

    struct sockaddr_un m_server_addr
    {};
    int m_socket_fd = -1;
    bool m_bound = false;
};