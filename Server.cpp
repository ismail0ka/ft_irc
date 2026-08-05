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



Server::Server(int port , std::string password) :  _port(port), _password(password) 
{
    _socketfd = -1;
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
        throw std::runtime_error("cannot set nonblock flag");


    if (listen(_socketfd,SOMAXCONN) < 0)

        throw std::runtime_error("server cannot listen");

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

    Client* client = new Client(clientfd);

    

    _clients.insert(std::make_pair(clientfd, client));
}


void Server::readFrom(Client& c)
{

}
void Server::sendTo(Client& c)
{

}