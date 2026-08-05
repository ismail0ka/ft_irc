#include <iostream>
#include "server.hpp"

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
#include <cstdlib>

server::server()
{
    _serverfd = -1;
}

client::client(int fd, int i)
{
    _clientfd = fd;
    id = i;
}

void server::tolisten()
{
    _serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverfd < 0)
        throw std::runtime_error("can't open the socket");

    int reuse = 1;
    int keepalive = 1;

    if (setsockopt(_serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        throw std::runtime_error("setsockopt failed");

    if (setsockopt(_serverfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0)
        throw std::runtime_error("setsockopt failed");

    if (fcntl(_serverfd, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("cannot set nonblock flag");

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_int16_t>(8080));

    if (bind(_serverfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(_serverfd, SOMAXCONN) < 0)
        throw std::runtime_error("server cannot listen");
}

int server::acceptclient()
{
    sockaddr_storage clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    int clientfd = accept(
        _serverfd,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &clientAddrLen
    );

    if (clientfd < 0)
        return -1;

    if (fcntl(clientfd, F_SETFL, O_NONBLOCK) < 0)
    {
        close(clientfd);
        return -1;
    }

    client* c = new client(clientfd, rand());
    _clients.insert(std::make_pair(clientfd, c));

    return clientfd;
}

bool server::recievefrom(client* c)
{
    char buffer[1024];

    ssize_t received = recv(c->getfd(), buffer, sizeof(buffer), 0);

    if (received < 0)
    {
        perror("recv");
        return false;
    }

    if (received == 0)
    {
        std::cout << "Client disconnected." << std::endl;
        return false;
    }
    std::cout << "Received " << received << " bytes: ";
    std::cout.write(buffer, received);
    std::cout << std::endl;
    c->appendInBuffer(buffer, static_cast<std::size_t>(received));

    return true;
}

bool server::sendto(client* c)
{
    if (c->getOutBuffer().empty())
        return true;

    ssize_t bytes = send(
        c->getfd(),
        c->getOutBuffer().data(),
        c->getOutBuffer().size(),
        0
    );

    if (bytes < 0)
    {
        perror("send");
        return false;
    }

    c->eraseFromOutBuffer(static_cast<std::size_t>(bytes));
    return true;
}

int client::getfd() const
{
    return _clientfd;
}

void client::appendInBuffer(const char* data, std::size_t n)
{
    in_buffer.append(data, n);
}

const std::string& client::getOutBuffer() const
{
    return out_buffer;
}

void client::eraseFromOutBuffer(std::size_t n)
{
    out_buffer.erase(0, n);
}

server::~server()
{
    for (std::map<int, client*>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        close(it->first);
        delete it->second;
    }

    if (_serverfd != -1)
        close(_serverfd);
}

client::~client()
{
}