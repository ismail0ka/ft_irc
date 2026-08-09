#ifndef SERVER_HPP
#define SERVER_HPP


#include <string>
#include <map>
<<<<<<< Updated upstream
#include "Client.hpp"
=======
#include <set>
#include "Client.hpp"
#define MAX_EVENTS 64
>>>>>>> Stashed changes

class Server
{
    private:
        int _port;
        std::string _password;
        int _socketfd;
<<<<<<< Updated upstream
        std::map<int, Client*> _clients;
=======
        int _epfd;
        std::map<int, Client*> _clients;
        std::set<int> _pendingClose;
>>>>>>> Stashed changes
        Server(const Server& other);
        Server& operator=(const Server& other);

    public:
        explicit Server(int port , std::string password);
        ~Server();
<<<<<<< Updated upstream

        void                setupListener();
        void                acceptClient();
=======
        
        void                setupListener();
        void                acceptClient();
        void                removeClient(int fd);
>>>>>>> Stashed changes
        void                readFrom(Client& c);
        void                sendTo(Client& c);
        int                 getFd() const;
        int                 getPort() const;
        const std::string   &getPassword() const;
<<<<<<< Updated upstream
=======
        void                run();
>>>>>>> Stashed changes
};


#endif