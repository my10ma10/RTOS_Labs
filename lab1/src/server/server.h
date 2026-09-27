#pragma once
#include <queue>
#include <sys/un.h>
#include "defines.h"

class Server
{
public:
    Server();

    void bind();
    void listen();
    int accept();

    Request readRequest() const;
    void writeResponse(const Response &response);

private:
    void process();

private:
    struct sockaddr_un m_server_addr{};
    int m_socket_fd = -1;

    std::priority_queue<Request> m_queue;
};