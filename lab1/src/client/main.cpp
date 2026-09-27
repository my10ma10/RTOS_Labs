#include "client.h"
#include "rwoperations.h"
#include <iostream>
#include <unistd.h>

int main()
{
    Client client;
    client.connect();

    Buffer buf;
    int read_n;
    while ((read_n = rwoperations::read(STDIN_FILENO, buf, BUF_SIZE)) > 0)
    {
        if (rwoperations::write(client.socketFd(), buf, read_n) != read_n)
        {
            std::clog << "client: cannot print to stdout" << std::endl;
        }
    }
    return 0;
}