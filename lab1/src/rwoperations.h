#pragma once

#include <cstddef>

namespace rwoperations {
bool readExact(int fd, void *data, size_t size);
bool writeExact(int fd, const void *data, size_t size);
} // namespace rwoperations