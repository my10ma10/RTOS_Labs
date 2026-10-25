#pragma once

#include "defines.h"
#include <string>
#include <string_view>
#include <sys/un.h>

class Client
{
public:
    explicit Client(uint32_t priority = 0);
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    void connect();
    std::string transact(std::string_view text);
    std::string scheduleTimer(uint32_t seconds);
    int eventFd() const;

private:
    std::string transact(Command command, int32_t value, std::string_view text);

    struct sockaddr_un m_client_addr{};
    int m_socket_fd = -1;
    int m_event_fd = -1;
    uint32_t m_priority;
};