#include "priority_request_queue.h"
#include <stdexcept>
#include <utility>

bool PriorityRequestQueue::Comparator::operator()(
    const std::shared_ptr<QueuedRequest> &left, const std::shared_ptr<QueuedRequest> &right) const
{
    if (left->request.priority != right->request.priority) {
        return left->request.priority < right->request.priority;
    }
    return left->sequence > right->sequence;
}

std::shared_ptr<QueuedRequest> PriorityRequestQueue::enqueue(
    int client_fd, const Request &request, std::string text)
{
    auto queued_request = std::make_shared<QueuedRequest>();
    queued_request->client_fd = client_fd;
    queued_request->request = request;
    queued_request->text = std::move(text);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping) {
            throw std::runtime_error("request queue is stopping");
        }
        queued_request->sequence = m_next_sequence++;
        m_requests.push(queued_request);
    }

    m_condvar.notify_one();
    return queued_request;
}

std::shared_ptr<QueuedRequest> PriorityRequestQueue::waitAndPop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condvar.wait(lock, [this] { return m_stopping || !m_requests.empty(); });

    if (m_requests.empty()) {
        return nullptr;
    }

    auto queued_request = m_requests.top();
    m_requests.pop();
    return queued_request;
}

void PriorityRequestQueue::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_condvar.notify_all();
}