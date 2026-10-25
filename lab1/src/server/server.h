#pragma once

#include "defines.h"
#include "notification_scheduler.h"
#include "priority_request_queue.h"
#include <cstddef>
#include <mutex>
#include <string>
#include <sys/un.h>
#include <thread>
#include <unordered_map>

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
    int receiveEventFd(int client_fd);
    void registerEventFd(int client_fd, int event_fd);
    void unregisterEventFd(int client_fd);
    size_t notifyClients();

    bool scheduleTimer(int client_fd, int32_t seconds);
    void handleClient(int client_fd);
    void processQueue();
    void logEvent(const std::string &message);
    bool sendResponse(int client_fd, int32_t status, const char *text, uint32_t text_size);

    struct sockaddr_un m_server_addr{};
    int m_socket_fd = -1;
    bool m_bound = false;
    PriorityRequestQueue m_request_queue;

    std::unordered_map<int, int> m_event_fds;
    std::mutex m_clients_mutex;

    std::mutex m_log_mutex;
    std::thread m_worker;
    NotificationScheduler m_notification_scheduler;
};