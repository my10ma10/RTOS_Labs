#include "client.h"
#include <stdexcept>
#include <cstring>

#include <sys/socket.h>

Client::Client()
{
    m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd == -1)
    {
        throw std::runtime_error("client: socket creation failed");
    }

}

void Client::connect()
{
    std::memset(&m_client_addr, 0, sizeof(m_client_addr));
    m_client_addr.sun_family = AF_UNIX;
    std::strncpy(m_client_addr.sun_path, SV_SOCK_PATH, sizeof(m_client_addr.sun_path) - 1);

    if (::connect(m_socket_fd, (sockaddr *)(&m_client_addr), sizeof(m_client_addr)) < 0) {
        throw std::runtime_error(std::string("client: cannot connect ") + SV_SOCK_PATH);
    }
}

int Client::socketFd() const
{
    return m_socket_fd;
}
