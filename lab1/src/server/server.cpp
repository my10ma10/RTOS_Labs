#include "server.h"
#include "rwoperations.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <utility>

Server::Server()
{
    m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        throw std::runtime_error("server: socket creation failed");
    }

    if ((::unlink(SV_SOCK_PATH) == -1) && errno != ENOENT) {
        ::close(m_socket_fd);
        m_socket_fd = -1;
        throw std::runtime_error(std::string("server: cannot remove ") + SV_SOCK_PATH);
    }
}

Server::~Server()
{
    m_request_queue.stop();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        for (const auto &entry : m_event_fds) {
            ::close(entry.second);
        }
        m_event_fds.clear();
    }
    if (m_socket_fd != -1) {
        ::close(m_socket_fd);
    }
    if (m_bound) {
        ::unlink(SV_SOCK_PATH);
    }
}

void Server::bind()
{
    std::memset(&m_server_addr, 0, sizeof(m_server_addr));
    m_server_addr.sun_family = AF_UNIX;
    std::strncpy(m_server_addr.sun_path, SV_SOCK_PATH, sizeof(m_server_addr.sun_path) - 1);

    if (::bind(m_socket_fd, reinterpret_cast<sockaddr *>(&m_server_addr), sizeof(m_server_addr))
        == -1) {
        throw std::runtime_error(std::string("server: cannot bind ") + SV_SOCK_PATH);
    }
    m_bound = true;
}

void Server::listen()
{
    if (::listen(m_socket_fd, BACKLOG) == -1) {
        throw std::runtime_error(std::string("server: cannot listen ") + SV_SOCK_PATH);
    }
}

int Server::accept()
{
    int client_fd = -1;
    do {
        client_fd = ::accept(m_socket_fd, nullptr, nullptr);
    } while (client_fd == -1 && errno == EINTR);

    return client_fd;
}

int Server::receiveEventFd(int client_fd)
{
    char marker = 0;
    iovec data{};
    data.iov_base = &marker;
    data.iov_len = sizeof(marker);

    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &data;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);

    ssize_t received;
    do {
        received = ::recvmsg(client_fd, &message, MSG_CMSG_CLOEXEC);
    } while (received == -1 && errno == EINTR);

    int event_fd = -1;
    for (cmsghdr *header = CMSG_FIRSTHDR(&message); header != nullptr;
         header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS
            && header->cmsg_len >= CMSG_LEN(sizeof(int))) {
            std::memcpy(&event_fd, CMSG_DATA(header), sizeof(event_fd));
            break;
        }
    }

    if (received != 1 || marker != EVENTFD_REGISTRATION || (message.msg_flags & MSG_CTRUNC)
        || event_fd == -1) {
        if (event_fd != -1) {
            ::close(event_fd);
        }
        return -1;
    }

    return event_fd;
}

void Server::registerEventFd(int client_fd, int event_fd)
{
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        m_event_fds.emplace(client_fd, event_fd);
    }
    logEvent("eventfd registered fd=" + std::to_string(client_fd));
}

void Server::unregisterEventFd(int client_fd)
{
    int event_fd = -1;
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        auto it = m_event_fds.find(client_fd);
        if (it != m_event_fds.end()) {
            event_fd = it->second;
            m_event_fds.erase(it);
        }
    }
    if (event_fd != -1) {
        ::close(event_fd);
    }
}

size_t Server::notifyClients()
{
    const uint64_t value = 1;
    size_t notified = 0;
    std::lock_guard<std::mutex> lock(m_clients_mutex);

    for (const auto &entry : m_event_fds) {
        ssize_t written;
        do {
            written = ::write(entry.second, &value, sizeof(value));
        } while (written == -1 && errno == EINTR);

        if (written == static_cast<ssize_t>(sizeof(value))) {
            ++notified;
        } else {
            logEvent("eventfd notification failed fd=" + std::to_string(entry.first));
        }
    }

    return notified;
}

bool Server::sendResponse(int client_fd, int32_t status, const char *text, uint32_t text_size)
{
    Response response{status, text_size};
    return rwoperations::writeExact(client_fd, &response, sizeof(response))
           && rwoperations::writeExact(client_fd, text, text_size);
}

void Server::logEvent(const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_log_mutex);
    std::cout << message << std::endl;
}

void Server::handleClient(int client_fd)
{
    int event_fd = receiveEventFd(client_fd);
    if (event_fd == -1) {
        logEvent("eventfd registration failed fd=" + std::to_string(client_fd));
        ::close(client_fd);
        return;
    }
    registerEventFd(client_fd, event_fd);
    logEvent("client connected fd=" + std::to_string(client_fd));

    while (true) {
        Request request{};
        if (!rwoperations::readExact(client_fd, &request, sizeof(request))) {
            break;
        }

        if (request.text_size > MAX_MESSAGE_SIZE) {
            logEvent("invalid request from fd=" + std::to_string(client_fd));
            break;
        }

        std::string text(request.text_size, '\0');
        if (!rwoperations::readExact(client_fd, text.data(), text.size())) {
            logEvent("incomplete request from fd=" + std::to_string(client_fd));
            break;
        }

        auto queued_request = m_request_queue.enqueue(client_fd, request, std::move(text));
        auto result = queued_request->completion.get_future();
        logEvent(
            "queued request fd=" + std::to_string(client_fd)
            + " priority=" + std::to_string(request.priority)
            + " sequence=" + std::to_string(queued_request->sequence));

        RequestReply reply = result.get();
        if (!sendResponse(
                client_fd,
                reply.status,
                reply.text.data(),
                static_cast<uint32_t>(reply.text.size()))) {
            logEvent("response failed fd=" + std::to_string(client_fd));
            break;
        }
        logEvent(
            "response sent fd=" + std::to_string(client_fd)
            + " sequence=" + std::to_string(queued_request->sequence));
    }

    unregisterEventFd(client_fd);
    ::close(client_fd);
    logEvent("client disconnected fd=" + std::to_string(client_fd));
}

void Server::processQueue()
{
    while (true) {
        auto queued_request = m_request_queue.waitAndPop();
        if (!queued_request) {
            return;
        }

        logEvent(
            "processing request fd=" + std::to_string(queued_request->client_fd)
            + " priority=" + std::to_string(queued_request->request.priority)
            + " sequence=" + std::to_string(queued_request->sequence));

        RequestReply reply{};
        if (queued_request->request.command == Command::Text) {
            reply.status = 0;
            reply.text = "Confirm: server received: " + queued_request->text;
            size_t notified = notifyClients();
            logEvent("async notification sent to " + std::to_string(notified) + " client(s)");
        } else {
            reply.status = -1;
            reply.text = "Unsupported command";
        }
        queued_request->completion.set_value(std::move(reply));
    }
}

void Server::run()
{
    m_worker = std::thread(&Server::processQueue, this);

    while (true) {
        int client_fd = accept();
        if (client_fd == -1) {
            throw std::runtime_error("server: accept failed");
        }

        std::thread(&Server::handleClient, this, client_fd).detach();
    }
}