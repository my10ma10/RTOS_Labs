#include "rwoperations.h"
#include <unistd.h>

namespace rwoperations
{

    int read(int fd, Buffer &buf, size_t size)
    {
        return ::read(fd, buf.data(), size);
    }

    int write(int fd, Buffer &buf, size_t size)
    {
        return ::write(fd, buf.data(), size);
    }

} // namespace rwoperations
