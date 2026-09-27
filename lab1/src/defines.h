#pragma once

#include <optional>
#include <any>
#include <array>

#define BACKLOG 5
#define SV_SOCK_PATH "/tmp/uds_file"

#define BUF_SIZE 1024

using Buffer = std::array<char, BUF_SIZE>;

struct Request
{
    int priority; // приоритет клиента
    int command;  // тип команды
    int value;    // данные
};

struct Response
{
    int status;
    std::optional<std::any> result = std::nullopt;
};