#include "server.h"
#include "rwoperations.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

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

void Server::handleClient(int client_fd)
{
    while (true) {
        Request request{};
        if (!rwoperations::readExact(client_fd, &request, sizeof(request))) {
            break;
        }

        if (request.text_size > MAX_MESSAGE_SIZE) {
            break;
        }

        std::string text(request.text_size, '\0');
        if (!rwoperations::readExact(client_fd, text.data(), text.size())) {
            break;
        }

        if (request.command != Command::Text) {
            const std::string error = "Unsupported command";
            if (!sendResponse(client_fd, -1, error.data(), static_cast<uint32_t>(error.size()))) {
                break;
            }
            continue;
        }

        const std::string reply = "Server received: " + text;
        if (!sendResponse(client_fd, 0, reply.data(), static_cast<uint32_t>(reply.size()))) {
            break;
        }
    }
}

void Server::run()
{
    while (true) {
        int client_fd = accept();
        if (client_fd == -1) {
            throw std::runtime_error("server: accept failed");
        }

        handleClient(client_fd);
        ::close(client_fd);
    }
}