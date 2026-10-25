#pragma once

#include <cstdint>

inline constexpr int BACKLOG = 5;
inline constexpr char SV_SOCK_PATH[] = "/tmp/uds_file";
inline constexpr uint32_t MAX_MESSAGE_SIZE = 4096;
inline constexpr char EVENTFD_REGISTRATION = 'E';

enum class Command : uint32_t { Text = 1, Timer = 2 };

struct Request
{
    uint32_t priority;
    Command command;
    int32_t value;
    uint32_t text_size;
};

struct Response
{
    int32_t status;
    uint32_t text_size;
};