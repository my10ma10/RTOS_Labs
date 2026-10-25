#include "client.h"
#include "rwoperations.h"
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

Client::Client(uint32_t priority)
    : m_priority(priority)
{
    m_socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket_fd == -1) {
        throw std::runtime_error("client: socket creation failed");
    }

    m_event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_event_fd == -1) {
        ::close(m_socket_fd);
        m_socket_fd = -1;
        throw std::runtime_error("client: eventfd creation failed");
    }
}

Client::~Client()
{
    if (m_event_fd != -1) {
        ::close(m_event_fd);
    }
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

    char marker = EVENTFD_REGISTRATION;
    iovec data{};
    data.iov_base = &marker;
    data.iov_len = sizeof(marker);

    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &data;
    message.msg_iovlen = 1;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);

    cmsghdr *header = CMSG_FIRSTHDR(&message);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(header), &m_event_fd, sizeof(m_event_fd));

    ssize_t sent;
    do {
        sent = ::sendmsg(m_socket_fd, &message, 0);
    } while (sent == -1 && errno == EINTR);

    if (sent != static_cast<ssize_t>(sizeof(marker))) {
        throw std::runtime_error("client: cannot send eventfd to server");
    }
}

std::string Client::transact(std::string_view text)
{
    return transact(Command::Text, 0, text);
}

std::string Client::scheduleTimer(uint32_t seconds)
{
    if (seconds > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        throw std::runtime_error("client: timer delay is too large");
    }
    return transact(Command::Timer, static_cast<int32_t>(seconds), {});
}

std::string Client::transact(Command command, int32_t value, std::string_view text)
{
    if (text.size() > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("client: message is too long");
    }

    Request request{m_priority, command, value, static_cast<uint32_t>(text.size())};
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

int Client::eventFd() const
{
    return m_event_fd;
}