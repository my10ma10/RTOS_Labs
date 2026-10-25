#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

class NotificationScheduler
{
public:
    using CompletionCallback = std::function<void(int, bool)>;

    explicit NotificationScheduler(CompletionCallback callback);
    ~NotificationScheduler();

    NotificationScheduler(const NotificationScheduler &) = delete;
    NotificationScheduler &operator=(const NotificationScheduler &) = delete;

    bool schedule(int event_fd, int client_fd, std::chrono::seconds delay);
    void stop();

private:
    struct ScheduledNotification
    {
        std::chrono::steady_clock::time_point deadline;
        uint64_t sequence;
        int event_fd;
        int client_fd;
    };

    struct Compare
    {
        bool operator()(const ScheduledNotification &left, const ScheduledNotification &right) const;
    };

    void run();

    CompletionCallback m_callback;
    uint64_t m_next_sequence = 0;
    bool m_stopping = false;
    std::priority_queue<ScheduledNotification, std::vector<ScheduledNotification>, Compare>
        m_notifications;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_worker;
};