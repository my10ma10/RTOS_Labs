#include "notification_scheduler.h"
#include <cerrno>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

NotificationScheduler::NotificationScheduler(CompletionCallback callback)
    : m_callback(std::move(callback))
    , m_worker(&NotificationScheduler::run, this)
{}

NotificationScheduler::~NotificationScheduler()
{
    stop();
}

bool NotificationScheduler::Compare::operator()(
    const ScheduledNotification &left, const ScheduledNotification &right) const
{
    if (left.deadline != right.deadline) {
        return left.deadline > right.deadline;
    }
    return left.sequence > right.sequence;
}

bool NotificationScheduler::schedule(int event_fd, int client_fd, std::chrono::seconds delay)
{
    int timer_event_fd = ::dup(event_fd);
    if (timer_event_fd == -1) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) {
            ::close(timer_event_fd);
            return false;
        }
        m_notifications.push(
            {std::chrono::steady_clock::now() + delay, m_next_sequence++, timer_event_fd, client_fd});
    }
    m_condition.notify_one();
    return true;
}

void NotificationScheduler::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_condition.notify_all();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void NotificationScheduler::run()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (true) {
        if (m_stopping) {
            while (!m_notifications.empty()) {
                ::close(m_notifications.top().event_fd);
                m_notifications.pop();
            }
            return;
        }

        if (m_notifications.empty()) {
            m_condition.wait(lock, [this] { return m_stopping || !m_notifications.empty(); });
            continue;
        }

        const auto deadline = m_notifications.top().deadline;
        if (m_condition.wait_until(lock, deadline) != std::cv_status::timeout) {
            continue;
        }
        if (m_stopping || m_notifications.empty()
            || m_notifications.top().deadline > std::chrono::steady_clock::now()) {
            continue;
        }

        ScheduledNotification notification = m_notifications.top();
        m_notifications.pop();
        lock.unlock();

        const uint64_t value = 1;
        ssize_t written;
        do {
            written = ::write(notification.event_fd, &value, sizeof(value));
        } while (written == -1 && errno == EINTR);

        bool success = written == static_cast<ssize_t>(sizeof(value));
        ::close(notification.event_fd);

        m_callback(notification.client_fd, success);

        lock.lock();
    }
}