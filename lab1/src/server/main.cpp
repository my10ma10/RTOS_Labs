#include "server.h"
#include <iostream>
#include <unistd.h>
#include "rwoperations.h"

int main()
{
    Server server;

    server.bind();
    server.listen();
    int listen_fd = server.accept();

    Buffer buf;
    int read_n;
    while ((read_n = rwoperations::read(listen_fd, buf, BUF_SIZE)) > 0)
    {
        if (rwoperations::write(STDOUT_FILENO, buf, read_n) != read_n)
        {
            std::clog << "server: cannot print to stdout" << std::endl;
        }
    }
    return 0;
}