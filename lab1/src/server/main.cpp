#include "server.h"
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int main(int argc, char *argv[])
{
    try {
        if (argc > 2) {
            throw std::invalid_argument("usage: server [processing-delay-ms]");
        }

        unsigned long delay_ms = 0;
        if (argc == 2) {
            size_t parsed = 0;
            delay_ms = std::stoul(argv[1], &parsed);
            if (parsed != std::string(argv[1]).size() || delay_ms > 60000) {
                throw std::invalid_argument("processing delay must be between 0 and 60000 ms");
            }
        }

        Server server{std::chrono::milliseconds(delay_ms)};
        server.bind();
        server.listen();
        server.run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}