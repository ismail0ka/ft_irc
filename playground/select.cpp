#include "server.hpp"


#include <sys/select.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>



int server::select_m()
{
    fd_set master;
    FD_ZERO(&master);
    FD_SET(_serverfd, &master);
    int maxfd = _serverfd;
    while (true)
    {
        fd_set readfds = master;          
        int ready = select(maxfd + 1, &readfds, NULL, NULL,NULL);
        if (ready < 0)
        {
            throw std::runtime_error("select failed");
        }
        if (FD_ISSET(_serverfd, &readfds))
        {
            int new_client = acceptclient();
            if (new_client >= 0)
            {
                FD_SET(new_client, &master);
                if (new_client > maxfd)
                    maxfd = new_client;
            }
        }
        for (std::map<int, client*>::iterator it = _clients.begin(); it != _clients.end(); )
        {
            std::map<int, client*>::iterator current = it++;

            if (FD_ISSET(current->first, &readfds))
            {
                if (!recievefrom(current->second))
                {
                    FD_CLR(current->first, &master);
                    close(current->first);

                    delete current->second;
                    _clients.erase(current);
                }
            }
        }
        }

}  


