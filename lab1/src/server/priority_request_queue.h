#pragma once

#include "defines.h"
#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <queue>

struct RequestReply
{
    int32_t status;
    std::string text;
};

struct QueuedRequest
{
    uint64_t sequence;
    int client_fd;
    Request request;
    std::string text;
    std::promise<RequestReply> completion;
};

class PriorityRequestQueue
{
public:
    std::shared_ptr<QueuedRequest> enqueue(int client_fd, const Request &request, std::string text);
    std::shared_ptr<QueuedRequest> waitAndPop();
    void stop();

private:
    struct Comparator
    {
        bool operator()(
            const std::shared_ptr<QueuedRequest> &left,
            const std::shared_ptr<QueuedRequest> &right) const;
    };

    uint64_t m_next_sequence = 0;
    bool m_stopping = false;

    std::priority_queue<
        std::shared_ptr<QueuedRequest>,
        std::vector<std::shared_ptr<QueuedRequest>>,
        Comparator>
        m_requests;

    std::mutex m_mutex;
    std::condition_variable m_condvar;
};