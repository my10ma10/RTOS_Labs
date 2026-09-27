#pragma once
#include <sys/un.h>
#include "defines.h"

class Client
{
public:
    Client();
    void connect();

    int socketFd() const;

private:
    struct sockaddr_un m_client_addr{};
    int m_socket_fd = -1;
};