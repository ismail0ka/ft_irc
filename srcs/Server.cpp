#include "Server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <stdexcept>
#include <iostream>

#include "Message.hpp"
#include "MessageParser.hpp"
#include "irc_utils.hpp"
#include "replies.hpp"

static volatile sig_atomic_t g_stop = 0;

static void handleSignal(int)
{
    g_stop = 1;
}

Server::Server(int port, const std::string& password)
    : _port(port), _password(password), _socketfd(-1)
{
}

Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        delete it->second;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
    if (_socketfd >= 0)
        close(_socketfd);
}

const std::string& Server::getPassword() const
{
    return _password;
}

std::map<std::string, Channel*>& Server::channels()
{
    return _channels;
}

void Server::setupListener()
{
    _socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_socketfd < 0)
        throw std::runtime_error("socket failed");

    int reuse = 1;
    if (setsockopt(_socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        throw std::runtime_error("setsockopt failed");
    if (fcntl(_socketfd, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl failed");

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<unsigned short>(_port));

    if (bind(_socketfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed (port already in use?)");
    if (listen(_socketfd, SOMAXCONN) < 0)
        throw std::runtime_error("listen failed");

    struct pollfd pfd;
    pfd.fd = _socketfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pfds.push_back(pfd);

    std::cout << "server listening on port " << _port << std::endl;
}

void Server::acceptClient()
{
    struct sockaddr_in  addr;
    socklen_t           len = sizeof(addr);

    std::memset(&addr, 0, sizeof(addr));
    int fd = accept(_socketfd, reinterpret_cast<struct sockaddr*>(&addr), &len);
    if (fd < 0)
        return;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
    {
        close(fd);
        return;
    }

    Client* c;
    try
    {
        c = new Client(fd);
    }
    catch (const std::exception&)
    {
        close(fd);
        return;
    }
    c->setHost(inet_ntoa(addr.sin_addr));
    _clients[fd] = c;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    _pfds.push_back(pfd);

    std::cout << "[+] client " << fd << " connected" << std::endl;
}

void Server::removeClient(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    partAllChannels(*it->second, false);

    for (std::size_t i = 0; i < _pfds.size(); i++)
    {
        if (_pfds[i].fd == fd)
        {
            _pfds.erase(_pfds.begin() + i);
            break;
        }
    }

    delete it->second;
    _clients.erase(it);

    std::cout << "[-] client " << fd << " disconnected" << std::endl;
}

void Server::readFrom(Client& c)
{
    char    buf[4096];
    int     fd = c.getFd();
    ssize_t got = recv(fd, buf, sizeof(buf), 0);

    if (got < 0)
    {
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
        return;
    }

    std::string line;

    while (!c.isQuit() && c.takeLine(line))
    {
        Message m = MessageParser::parse(line);
        _dispatcher.dispatch(*this, c, m);
    }
}

void Server::sendTo(Client& c)
{
    if (!c.hasOut())
        return;

    ssize_t sent = send(c.getFd(), c.outData().c_str(), c.outData().size(), 0);
    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        disconnect(c.getFd(), "Write error");
        return;
    }
    c.consumeOut(static_cast<std::size_t>(sent));
}

void Server::disconnect(int fd, const std::string& reason)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client& c = *it->second;
    if (c.isQuit())
        return;

    if (c.isWelcomed())
        broadcastToPeers(c, ":" + clientPrefix(c) + " QUIT :" + reason + CRLF, false);

    partAllChannels(c, false);
    c.queue(ERROR_CLOSING(reason));
    c.setQuit(true);
}

void Server::closeQuitClients()
{
    std::vector<int> gone;

    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second->isQuit())
            gone.push_back(it->first);
    }

    for (std::size_t i = 0; i < gone.size(); i++)
    {
        Client* c = _clients[gone[i]];
        if (c->hasOut())
            send(c->getFd(), c->outData().c_str(), c->outData().size(), 0);
        removeClient(gone[i]);
    }
}

Client* Server::findClientByNick(const std::string& nick)
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second->isQuit())
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

    _channels[ircLower(name)] = ch;
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
                ch->broadcast(":" + clientPrefix(c) + " PART " + ch->getName() + " :Leaving" + CRLF, NULL);
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

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
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

void Server::run()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    while (!g_stop)
    {
        for (std::size_t i = 0; i < _pfds.size(); i++)
        {
            std::map<int, Client*>::iterator it = _clients.find(_pfds[i].fd);

            _pfds[i].events = POLLIN;
            if (it != _clients.end() && it->second->hasOut())
                _pfds[i].events |= POLLOUT;
            _pfds[i].revents = 0;
        }

        if (poll(&_pfds[0], _pfds.size(), -1) < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        std::size_t count = _pfds.size();
        for (std::size_t i = 0; i < count; i++)
        {
            int   fd = _pfds[i].fd;
            short re = _pfds[i].revents;

            if (re == 0)
                continue;
            if (fd == _socketfd)
            {
                if (re & POLLIN)
                    acceptClient();
                continue;
            }

            std::map<int, Client*>::iterator it = _clients.find(fd);
            if (it == _clients.end())
                continue;

            Client& c = *it->second;
            if (re & (POLLERR | POLLHUP | POLLNVAL))
            {
                disconnect(fd, "Connection error");
                continue;
            }
            if (re & POLLIN)
                readFrom(c);
            if (re & POLLOUT)
                sendTo(c);
        }

        closeQuitClients();
    }

    std::cout << "\nshutting down" << std::endl;
}
