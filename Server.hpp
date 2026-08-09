#ifndef SERVER_HPP
#define SERVER_HPP


#include <string>
#include <map>
#include <set>
#include "Client.hpp"
#define MAX_EVENTS 64

class Server
{
    private:
        int _port;
        std::string _password;
        int _socketfd;
        int _epfd;
        std::map<int, Client*> _clients;
        std::set<int> _pendingClose;
        Server(const Server& other);
        Server& operator=(const Server& other);

    public:
        explicit Server(int port , std::string password);
        ~Server();
        
        void                setupListener();
        void                acceptClient();
        void                removeClient(int fd);
        void                readFrom(Client& c);
        void                sendTo(Client& c);
        int                 getFd() const;
        int                 getPort() const;
        const std::string   &getPassword() const;
        void                run();
};


#endif