#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <poll.h>

#include "Client.hpp"
#include "Channel.hpp"
#include "CommandDispatcher.hpp"

class Server
{
    private:
        int                             _port;
        std::string                     _password;
        int                             _socketfd;
        std::vector<struct pollfd>      _pfds;
        std::map<int, Client*>          _clients;
        std::map<std::string, Channel*> _channels;
        CommandDispatcher               _dispatcher;

        Server(const Server& other);
        Server& operator=(const Server& other);

        void    acceptClient();
        void    readFrom(Client& c);
        void    sendTo(Client& c);
        void    removeClient(int fd);
        void    closeQuitClients();

    public:
        Server(int port, const std::string& password);
        ~Server();

        void    setupListener();
        void    run();

        const std::string&  getPassword() const;

        void        disconnect(int fd, const std::string& reason);

        Client*     findClientByNick(const std::string& nick);
        Channel*    findChannel(const std::string& name);
        Channel*    createChannel(const std::string& name);

        void        dropChannelIfEmpty(Channel* ch);

        void        partAllChannels(Client& c, bool announce);

        void        broadcastToPeers(Client& c, const std::string& line, bool includeSelf);

        std::map<std::string, Channel*>& channels();
};

#endif
