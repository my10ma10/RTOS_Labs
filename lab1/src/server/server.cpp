#include "server.h"
#include <stdexcept>
#include <array>
#include <cstring>

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

Server::Server()
{
    m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd == -1)
    {
        throw std::runtime_error("server: socket creation failed");
    }

    if ((remove(SV_SOCK_PATH) == -1) && errno != ENOENT)
    {
        throw std::runtime_error(std::string("server: cannot remove ") + SV_SOCK_PATH);
    }
}

void Server::bind()
{
    std::memset(&m_server_addr, 0, sizeof(m_server_addr));
    m_server_addr.sun_family = AF_UNIX;
    std::strncpy(m_server_addr.sun_path, SV_SOCK_PATH, sizeof(m_server_addr.sun_path) - 1);

    int status = ::bind(m_socket_fd, (sockaddr *)(&m_server_addr), sizeof(m_server_addr));
    if (status == -1)
    {
        throw std::runtime_error(std::string("server: cannot bind ") + SV_SOCK_PATH);
    }
}

void Server::listen()
{
    if (::listen(m_socket_fd, BACKLOG))
    {
        throw std::runtime_error(std::string("server: cannot listen ") + SV_SOCK_PATH);
    }
}

int Server::accept()
{
    int listen_fd = ::accept(m_socket_fd, NULL, NULL);
    return listen_fd;
}

void Server::process()
{
}
