#include "server.h"
#include "rwoperations.h"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
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

bool Server::sendResponse(int client_fd, int32_t status, const char *text, uint32_t text_size)
{
    Response response{status, text_size};
    return rwoperations::writeExact(client_fd, &response, sizeof(response))
           && rwoperations::writeExact(client_fd, text, text_size);
}

void Server::logEvent(const std::string &message)
{
    std::lock_guard<std::mutex> lock(m_log_mutex);
    std::cout << "[SERVER]: " << message << std::endl;
}

void Server::handleClient(int client_fd)
{
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