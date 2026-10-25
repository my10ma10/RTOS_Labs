#include "server.h"
#include <exception>
#include <iostream>

int main()
{
    try {
        Server server;
        server.bind();
        server.listen();
        server.run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}