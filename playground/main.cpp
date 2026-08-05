#include "server.hpp"

#include <iostream>
#include <exception>

int main()
{
    try
    {
        server serv;

        serv.tolisten();

        std::cout << "Server listening on port 8080..." << std::endl;

        serv.select_m();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}