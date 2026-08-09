#include <iostream>

#include <istream>

#include "Server.hpp"
#include <cstring>
#include <sstream>
<<<<<<< Updated upstream

=======
#include <csignal>
>>>>>>> Stashed changes
int parsePort(const std::string &text)
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
        return -1;
    }
    int port = parsePort(argv[1]);
    std::string password = argv[2];

<<<<<<< Updated upstream
=======
    std::signal(SIGPIPE, SIG_IGN);


>>>>>>> Stashed changes
    try
    {
        Server s(port,password);
        s.setupListener();
<<<<<<< Updated upstream
=======
        s.run();
>>>>>>> Stashed changes
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
}