#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <csignal>

#include "Server.hpp"

static int parsePort(const std::string& text)
{
    std::istringstream iss(text);
    int                port = 0;

    if (!(iss >> port) || !iss.eof())
        throw std::runtime_error("port must be a number");
    if (port < 1024 || port > 65535)
        throw std::runtime_error("port must be between 1024 and 65535");

    return port;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        int         port     = parsePort(argv[1]);
        std::string password = argv[2];

        Server s(port, password);

        s.setupListener();
        s.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
