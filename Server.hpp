#ifndef SERVER_HPP
#define SERVER_HPP


#include <string>
#include <map>
#include "Client.hpp"

class Server
{
    private:
        int _port;
        std::string _password;
        int _socketfd;
        std::map<int, Client*> _clients;
        Server(const Server& other);
        Server& operator=(const Server& other);

    public:
        explicit Server(int port , std::string password);
        ~Server();

        void                setupListener();
        void                acceptClient();
        void                readFrom(Client& c);
        void                sendTo(Client& c);
        int                 getFd() const;
        int                 getPort() const;
        const std::string   &getPassword() const;
};


#endif