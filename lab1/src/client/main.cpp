#include "client.h"
#include <cerrno>
#include <exception>
#include <iostream>
#include <limits>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace {

void sendLine(Client &client, std::string line)
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    std::cout << client.transact(line) << std::endl;
}

void readNotifications(int event_fd)
{
    uint64_t count = 0;
    ssize_t result;
    do {
        result = ::read(event_fd, &count, sizeof(count));
    } while (result == -1 && errno == EINTR);

    if (result != static_cast<ssize_t>(sizeof(count))) {
        throw std::runtime_error("client: cannot read eventfd");
    }

    std::cout << "Async notifications: " << count << std::endl;
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        if (argc > 2) {
            throw std::invalid_argument("usage: client [priority]");
        }

        uint32_t priority = 0;
        if (argc == 2) {
            size_t parsed = 0;
            unsigned long value = std::stoul(argv[1], &parsed);
            if (parsed != std::string(argv[1]).size()
                || value > std::numeric_limits<uint32_t>::max()) {
                throw std::invalid_argument("priority must be an unsigned 32-bit integer");
            }
            priority = static_cast<uint32_t>(value);
        }

        Client client(priority);
        client.connect();

        std::cout << "Enter message to server (priority " << priority << ")" << std::endl;
        std::string pending;
        bool input_open = true;
        while (input_open) {
            pollfd descriptors[2]{};
            descriptors[0].fd = STDIN_FILENO;
            descriptors[0].events = POLLIN | POLLHUP;
            descriptors[1].fd = client.eventFd();
            descriptors[1].events = POLLIN;

            int result;
            do {
                result = ::poll(descriptors, 2, -1);
            } while (result == -1 && errno == EINTR);
            if (result == -1) {
                throw std::runtime_error("client: poll failed");
            }

            if (descriptors[1].revents & POLLIN) {
                readNotifications(client.eventFd());
            }
            if (descriptors[1].revents & (POLLERR | POLLNVAL)) {
                throw std::runtime_error("client: eventfd polling failed");
            }

            if (descriptors[0].revents & (POLLIN | POLLHUP)) {
                char buffer[512];
                ssize_t bytes_read;
                do {
                    bytes_read = ::read(STDIN_FILENO, buffer, sizeof(buffer));
                } while (bytes_read == -1 && errno == EINTR);

                if (bytes_read == -1) {
                    throw std::runtime_error("client: cannot read stdin");
                }
                if (bytes_read == 0) {
                    input_open = false;
                    if (!pending.empty()) {
                        sendLine(client, std::move(pending));
                        pending.clear();
                    }
                    continue;
                }

                pending.append(buffer, static_cast<size_t>(bytes_read));
                size_t newline;
                while ((newline = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    sendLine(client, std::move(line));
                }
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}