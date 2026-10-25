#include "rwoperations.h"
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace rwoperations {

bool readExact(int fd, void *data, size_t size)
{
    auto *bytes = static_cast<char *>(data);
    size_t total = 0;

    while (total < size) {
        ssize_t result = ::read(fd, bytes + total, size - total);
        if (result == -1 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        total += static_cast<size_t>(result);
    }

    return true;
}

bool writeExact(int fd, const void *data, size_t size)
{
    const auto *bytes = static_cast<const char *>(data);
    size_t total = 0;

    while (total < size) {
        ssize_t result = ::write(fd, bytes + total, size - total);
        if (result == -1 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        total += static_cast<size_t>(result);
    }

    return true;
}

} // namespace rwoperations