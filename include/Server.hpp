#ifndef SERVER_HPP
#define SERVER_HPP


#include <string>
#include <map>
#include <set>
#include "Client.hpp"
#include "Channel.hpp"
#include "CommandDispatcher.hpp"
#define MAX_EVENTS 64

class Server
{
    private:
        int _port;
        std::string _password;
        int _socketfd;
        int _epfd;
        std::map<int, Client*> _clients;
        /* Keyed by ircLower(name), so "#Foo" and "#foo" are one channel. The
           Channel keeps the spelling it was created with, for display. */
        std::map<std::string, Channel*> _channels;
        std::set<int> _pendingClose;
        CommandDispatcher _dispatcher;
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

        /* ---- the surface the command handlers are allowed to touch ------- */

        /* Announces the departure, drops the client out of every channel and
           schedules the socket for closing. It never deletes the Client: the
           poll loop does that once the outgoing buffer has been flushed, so
           the caller may keep using `c` until it returns to run(). */
        void                disconnect(int fd, const std::string& reason);

        Client*             findClientByNick(const std::string& nick);
        Channel*            findChannel(const std::string& name);
        Channel*            createChannel(const std::string& name);

        /* Channels are born on JOIN and die with their last member. Call this
           after anything that can empty one; the pointer is dangling after. */
        void                dropChannelIfEmpty(Channel* ch);

        void                partAllChannels(Client& c, bool announce);

        /* Everyone sharing at least one channel with `c`, each reached once
           however many channels they share. */
        void                broadcastToPeers(Client& c, const std::string& line, bool includeSelf);

        std::map<int, Client*>&          clients();
        std::map<std::string, Channel*>& channels();
};


#endif
