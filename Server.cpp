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

#include <iostream>
<<<<<<< Updated upstream
#include <cerrno>
#include <csignal>

static volatile sig_atomic_t g_stop = 0;

static void handleSignal(int)
{
    g_stop = 1;
}
=======
>>>>>>> Stashed changes





Server::Server(int port , std::string password) :  _port(port), _password(password)
{
    _socketfd = -1;
    _epfd = -1;
}

Server::Server(const Server& other)
{
    _socketfd = other._socketfd;
    _port = other._port;
    _password = other._password;
}
Server& Server::operator=(const Server& other)
{   
    if (this != &other)
    {
        _socketfd = other._socketfd;
        _port = other._port;
        _password = other._password;
    }
    return *this;
}
Server::~Server()
{
    for (std::map<int, Client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
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
<<<<<<< Updated upstream
        throw std::runtime_error("bind failed (port already in use?)");
=======
        throw std::runtime_error("cannot set nonblock flag");
>>>>>>> Stashed changes


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
 
void Server::acceptClient()
{
    sockaddr_storage clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

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

<<<<<<< Updated upstream

    Client* client = NULL;
    try
    {
        client = new Client(clientfd);

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
=======
    Client* client = new Client(clientfd);

    
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
>>>>>>> Stashed changes
}


void Server::removeClient(int fd)
{
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);  
    delete it->second;                            
    _clients.erase(it);                           

    std::cout << "[-] client " << fd << " disconnected" << std::endl;
}

void Server::readFrom(Client& c)
{
    char    buf[4096];
    ssize_t got = recv(c.getFd(), buf, sizeof(buf), 0);

    if (got <= 0)
    {
        _pendingClose.insert(c.getFd());
        std::cout << "[-] client " << c.getFd() << " gone" << std::endl;
        return;
    }

    if (!c.appendIn(buf, static_cast<std::size_t>(got)))
    {
        _pendingClose.insert(c.getFd()); 
        std::cout << "[!] flood from " << c.getFd() << std::endl;
        return;
    }

    std::string line;
    while (c.takeLine(line))
        std::cout << "<< [" << c.getFd() << "] " << line << std::endl;
}
void Server::sendTo(Client& c)
{
    if (!c.hasOut())
        return;

    ssize_t sent = send(c.getFd(), c.outData().c_str(), c.outData().size(), MSG_NOSIGNAL);

    if (sent <= 0)
    {
        _pendingClose.insert(c.getFd());
        return;
    }

    c.consumeOut(static_cast<std::size_t>(sent));
}



void Server::run()
{
    struct epoll_event events[MAX_EVENTS];

<<<<<<< Updated upstream
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

=======
    while (1)
    {
        int n = epoll_wait(_epfd, events, MAX_EVENTS, -1);

>>>>>>> Stashed changes
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
                    _pendingClose.insert(fd);
                    continue;
                }

                if (ev & EPOLLIN)
                    readFrom(c);

                if (_pendingClose.count(fd))
                    continue;

                if (ev & EPOLLOUT)
                    sendTo(c);
            }
        }

<<<<<<< Updated upstream
        
        for (std::set<int>::iterator it = _pendingClose.begin();
             it != _pendingClose.end(); ++it)
        {
            removeClient(*it);
        }
        _pendingClose.clear();

        
=======
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
    }

    std::cout << "\nshutting down" << std::endl;
}
=======
        for (std::set<int>::iterator it = _pendingClose.begin();
             it != _pendingClose.end(); ++it)
        {
            removeClient(*it);
        }
        _pendingClose.clear();
    }
}
>>>>>>> Stashed changes
