#include "client.h"
#include <exception>
#include <iostream>
#include <string>

int main()
{
    try {
        Client client;
        client.connect();

        std::string text;
        while (std::getline(std::cin, text)) {
            std::cout << client.transact(text) << std::endl;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}