#include "client.h"
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int main(int argc, char *argv[])
{
    try {
        if (argc > 2) {
            throw std::invalid_argument("usage: client [priority]");
        }

        uint32_t priority = 0;
        if (argc == 2) {
            size_t parsed_symbols_count = 0;
            const auto value = std::stoul(argv[1], &parsed_symbols_count);
            if (parsed_symbols_count != std::string(argv[1]).size()
                || value > std::numeric_limits<uint32_t>::max()) {
                throw std::invalid_argument("priority must be an unsigned 32-bit integer");
            }
            priority = static_cast<uint32_t>(value);
        }

        Client client(priority);
        client.connect();

        std::string text;
        std::cout << "Enter message to server (priority " << priority << ")" << std::endl;
        while (std::getline(std::cin, text)) {
            std::cout << client.transact(text) << std::endl;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}