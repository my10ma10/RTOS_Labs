#include "client.h"
#include "rwoperations.h"
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Client::Client(uint32_t priority)
    : m_priority(priority)
{
    m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        throw std::runtime_error("client: socket creation failed");
    }
}

Client::~Client()
{
    if (m_socket_fd != -1) {
        ::close(m_socket_fd);
    }
}

void Client::connect()
{
    std::memset(&m_client_addr, 0, sizeof(m_client_addr));
    m_client_addr.sun_family = AF_UNIX;
    std::strncpy(m_client_addr.sun_path, SV_SOCK_PATH, sizeof(m_client_addr.sun_path) - 1);

    if (::connect(m_socket_fd, reinterpret_cast<sockaddr *>(&m_client_addr), sizeof(m_client_addr))
        == -1) {
        throw std::runtime_error(std::string("client: cannot connect ") + SV_SOCK_PATH);
    }
}

std::string Client::transact(std::string_view text)
{
    if (text.size() > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("client: message is too long");
    }

    Request request{m_priority, Command::Text, 0, static_cast<uint32_t>(text.size())};
    if (!rwoperations::writeExact(m_socket_fd, &request, sizeof(request))
        || !rwoperations::writeExact(m_socket_fd, text.data(), text.size())) {
        throw std::runtime_error("client: cannot send request");
    }

    Response response{};
    if (!rwoperations::readExact(m_socket_fd, &response, sizeof(response))
        || response.text_size > MAX_MESSAGE_SIZE + 64) {
        throw std::runtime_error("client: invalid response");
    }

    std::string reply(response.text_size, '\0');
    if (!rwoperations::readExact(m_socket_fd, reply.data(), reply.size())) {
        throw std::runtime_error("client: cannot read response");
    }

    if (response.status != 0) {
        throw std::runtime_error("server: " + reply);
    }

    return reply;
}