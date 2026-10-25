#pragma once

#include "defines.h"
#include <string>
#include <string_view>
#include <sys/un.h>

class Client
{
public:
    Client();
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    void connect();
    std::string transact(std::string_view text);

private:
    struct sockaddr_un m_client_addr
    {};
    int m_socket_fd = -1;
};