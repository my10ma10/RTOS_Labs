#pragma once
#include "defines.h"

namespace rwoperations
{
    int read(int fd, Buffer &buf, size_t size);
    int write(int fd, Buffer &buf, size_t size);
} // namespace rwoperations
