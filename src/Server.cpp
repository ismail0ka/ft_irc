#include "Server.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <exception>
#include <stdexcept>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <iostream>
#include <cerrno>
#include <csignal>

#include "Message.hpp"
#include "MessageParser.hpp"
#include "irc_utils.hpp"
#include "replies.hpp"

static volatile sig_atomic_t g_stop = 0;

static void handleSignal(int)
{
    g_stop = 1;
}





Server::Server(int port , std::string password) :  _port(port), _password(password)
{
    _socketfd = -1;
    _epfd = -1;
}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
        delete it->second;

    for (std::map<std::string, Channel*>::iterator it = _channels.begin();
         it != _channels.end(); ++it)
        delete it->second;

    if (_socketfd >= 0)
        close(_socketfd);

    if (_epfd >= 0)
    {
        close(_epfd);
    }
}

void Server::setupListener()
{
   _socketfd = socket(AF_INET,SOCK_STREAM,0);
   if(_socketfd < 0)
        throw std::runtime_error("cant open the socket");

    int reuse = 1;
    int keepalive = 1;
    if (setsockopt(_socketfd, SOL_SOCKET,SO_REUSEADDR,&reuse, sizeof(reuse)) < 0)
        throw std::runtime_error("setsockopt failed");
    if (setsockopt(_socketfd, SOL_SOCKET,SO_KEEPALIVE,&keepalive, sizeof(keepalive)) < 0)
        throw std::runtime_error("setsockopt failed");
    if (fcntl(_socketfd, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("cannot set nonblock flag");

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_int16_t>(getPort()));

    if (bind(_socketfd, reinterpret_cast<sockaddr* > (&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed (port already in use?)");


    if (listen(_socketfd,SOMAXCONN) < 0)

        throw std::runtime_error("server cannot listen");

    if ((_epfd = epoll_create1(0)) < 0 )
        throw std::runtime_error("epoll failed at creation epollfd");

    struct epoll_event ev;
    std::memset(&ev,0,sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = _socketfd;

    if (epoll_ctl(_epfd,EPOLL_CTL_ADD, _socketfd,&ev) < 0 )
        throw std::runtime_error("epoll failed at monitiring the epoll fd");

}


int Server::getFd() const
{
    return _socketfd;
}

int Server::getPort() const
{
    return _port;
}

const std::string &Server::getPassword() const
{
    return _password;
}

std::map<int, Client*>& Server::clients()
{
    return _clients;
}

std::map<std::string, Channel*>& Server::channels()
{
    return _channels;
}

/* Presentation form of the peer address, for the host half of nick!user@host. */
static std::string hostOf(const sockaddr_storage& addr)
{
    char text[INET6_ADDRSTRLEN];

    std::memset(text, 0, sizeof(text));
    if (addr.ss_family == AF_INET)
    {
        const sockaddr_in* v4 = reinterpret_cast<const sockaddr_in*>(&addr);
        if (inet_ntop(AF_INET, &v4->sin_addr, text, sizeof(text)) != NULL)
            return std::string(text);
    }
    else if (addr.ss_family == AF_INET6)
    {
        const sockaddr_in6* v6 = reinterpret_cast<const sockaddr_in6*>(&addr);
        if (inet_ntop(AF_INET6, &v6->sin6_addr, text, sizeof(text)) != NULL)
            return std::string(text);
    }
    return std::string("localhost");
}

void Server::acceptClient()
{
    sockaddr_storage clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    std::memset(&clientAddr, 0, sizeof(clientAddr));

    int clientfd = accept(
        _socketfd,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &clientAddrLen
    );

    if (clientfd < 0)
        return;

    if (fcntl(clientfd, F_SETFL, O_NONBLOCK) < 0)
    {
        close(clientfd);
        return;
    }


    Client* client = NULL;
    try
    {
        client = new Client(clientfd);
        client->setHost(hostOf(clientAddr));

        struct epoll_event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.events  = EPOLLIN;
        ev.data.fd = clientfd;
        if (epoll_ctl(_epfd, EPOLL_CTL_ADD, clientfd, &ev) < 0)
        {
            delete client;
            return;
        }

        _clients.insert(std::make_pair(clientfd, client));
    }
    catch (const std::exception&)
    {
        epoll_ctl(_epfd, EPOLL_CTL_DEL, clientfd, NULL);
        if (client == NULL)
            close(clientfd);
        else
            delete client;
        return;
    }
}


void Server::removeClient(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    /* Safety net: disconnect() normally did this already, but a client torn
       down straight from an epoll error never went through it. */
    partAllChannels(*it->second, false);

    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
    delete it->second;
    _clients.erase(it);

    std::cout << "[-] client " << fd << " disconnected" << std::endl;
}

void Server::readFrom(Client& c)
{
    char      buf[4096];
    const int fd = c.getFd();
    ssize_t   got = recv(fd, buf, sizeof(buf), 0);

    if (got < 0)
    {
        /* Level-triggered epoll means this is all but impossible, but a
           spurious wakeup must not be read as a hangup. */
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        disconnect(fd, "Connection reset by peer");
        return;
    }
    if (got == 0)
    {
        disconnect(fd, "Connection closed");
        return;
    }

    if (!c.appendIn(buf, static_cast<std::size_t>(got)))
    {
        disconnect(fd, "Input line too long");
        std::cout << "[!] flood from " << fd << std::endl;
        return;
    }

    std::string line;

    /* A handler may have asked for this client to go (QUIT, bad password):
       stop feeding it lines the moment that happens. */
    while (!_pendingClose.count(fd) && c.takeLine(line))
    {
        Message m = MessageParser::parse(line);
        _dispatcher.dispatch(*this, c, m);
    }
}

void Server::sendTo(Client& c)
{
    if (!c.hasOut())
        return;

    ssize_t sent = send(c.getFd(), c.outData().c_str(), c.outData().size(), MSG_NOSIGNAL);

    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        disconnect(c.getFd(), "Write error");
        return;
    }

    c.consumeOut(static_cast<std::size_t>(sent));
}


/* ---- clients, channels, departures ---------------------------------------- */

Client* Server::findClientByNick(const std::string& nick)
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        /* A client on its way out no longer answers to its nick, so the name
           is free again immediately. */
        if (_pendingClose.count(it->first))
            continue;
        if (!it->second->getNick().empty() && ircEqual(it->second->getNick(), nick))
            return it->second;
    }
    return NULL;
}

Channel* Server::findChannel(const std::string& name)
{
    std::map<std::string, Channel*>::iterator it = _channels.find(ircLower(name));

    if (it == _channels.end())
        return NULL;
    return it->second;
}

Channel* Server::createChannel(const std::string& name)
{
    Channel* ch = new Channel(name);

    _channels.insert(std::make_pair(ircLower(name), ch));
    return ch;
}

void Server::dropChannelIfEmpty(Channel* ch)
{
    if (ch == NULL || !ch->isEmpty())
        return;

    std::map<std::string, Channel*>::iterator it = _channels.find(ircLower(ch->getName()));

    if (it != _channels.end() && it->second == ch)
        _channels.erase(it);
    delete ch;
}

void Server::partAllChannels(Client& c, bool announce)
{
    std::map<std::string, Channel*>::iterator it = _channels.begin();

    while (it != _channels.end())
    {
        Channel* ch = it->second;

        if (ch->isMember(c))
        {
            if (announce)
                ch->broadcast(":" + c.getPrefix() + " PART " + ch->getName() + " :Leaving" + CRLF, NULL);
            ch->part(c, "");
        }

        if (ch->isEmpty())
        {
            std::map<std::string, Channel*>::iterator dead = it++;
            _channels.erase(dead);
            delete ch;
        }
        else
            ++it;
    }
}

void Server::broadcastToPeers(Client& c, const std::string& line, bool includeSelf)
{
    std::set<Client*> peers;

    for (std::map<std::string, Channel*>::iterator it = _channels.begin();
         it != _channels.end(); ++it)
    {
        Channel* ch = it->second;

        if (!ch->isMember(c))
            continue;

        const std::set<Client*>& members = ch->getMembers();
        for (std::set<Client*>::const_iterator m = members.begin(); m != members.end(); ++m)
        {
            if (*m != &c)
                peers.insert(*m);
        }
    }

    for (std::set<Client*>::iterator it = peers.begin(); it != peers.end(); ++it)
        (*it)->queue(line);

    if (includeSelf)
        c.queue(line);
}

void Server::disconnect(int fd, const std::string& reason)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);

    if (it == _clients.end() || _pendingClose.count(fd))
        return;

    Client& c = *it->second;

    /* Only a client the network ever saw is worth announcing. */
    if (c.isWelcomed())
        broadcastToPeers(c, ":" + c.getPrefix() + " QUIT :" + reason + CRLF, false);

    partAllChannels(c, false);
    c.queue(ERROR_CLOSING(reason));

    /* The socket stays alive until run() has flushed the buffer. */
    _pendingClose.insert(fd);
}


void Server::run()
{
    struct epoll_event events[MAX_EVENTS];

    std::signal(SIGINT,  handleSignal);
    std::signal(SIGQUIT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    while (!g_stop)
    {
        int n = epoll_wait(_epfd, events, MAX_EVENTS, -1);

        if (n < 0)
        {

            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;

            if (fd == _socketfd)
            {
                acceptClient();
                continue;
            }
            else
            {
                std::map<int, Client*>::iterator it = _clients.find(fd);
                if (it == _clients.end())
                    continue;

                Client&      c  = *it->second;
                unsigned int ev = events[i].events;

                if (ev & (EPOLLERR | EPOLLHUP))
                {
                    disconnect(fd, "Connection error");
                    continue;
                }

                if (ev & EPOLLIN)
                    readFrom(c);

                if (ev & EPOLLOUT)
                    sendTo(c);
            }
        }

        /* Flush what a leaving client is still owed -- the QUIT echo and the
           ERROR line -- then let go of it. Best effort: the peer may already
           be gone, and there is nothing left to report if it is. */
        if (!_pendingClose.empty())
        {
            std::set<int> closing;
            closing.swap(_pendingClose);

            for (std::set<int>::iterator it = closing.begin(); it != closing.end(); ++it)
            {
                std::map<int, Client*>::iterator ci = _clients.find(*it);

                if (ci != _clients.end() && ci->second->hasOut())
                {
                    ssize_t ignored = send(ci->second->getFd(),
                                           ci->second->outData().c_str(),
                                           ci->second->outData().size(),
                                           MSG_NOSIGNAL);
                    (void)ignored;
                }
                removeClient(*it);
            }
            /* Anything the loop above re-marked refers to an fd already gone. */
            _pendingClose.clear();
        }


        for (std::map<int , Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        {
            struct epoll_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.data.fd = it->second->getFd();
            if (it->second->hasOut())
                ev.events = EPOLLIN | EPOLLOUT;
            else
                ev.events = EPOLLIN;
            epoll_ctl(_epfd, EPOLL_CTL_MOD, it->second->getFd(), &ev);
        }
    }

    std::cout << "\nshutting down" << std::endl;
}
