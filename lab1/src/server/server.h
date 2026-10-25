#pragma once

#include "defines.h"
#include "priority_request_queue.h"
#include <mutex>
#include <string>
#include <sys/un.h>
#include <thread>

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
    void processQueue();
    void logEvent(const std::string &message);
    bool sendResponse(int client_fd, int32_t status, const char *text, uint32_t text_size);

    struct sockaddr_un m_server_addr
    {};
    int m_socket_fd = -1;
    bool m_bound = false;
    PriorityRequestQueue m_request_queue;
    std::mutex m_log_mutex;
    std::thread m_worker;
};