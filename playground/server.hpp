#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <cstddef>
#include <cstdio>
class client;


struct pollfd {
    int   fd;         /* descriptor to watch; negative means "skip me" */
    short events;     /* what you want to know about   — YOU write this */
    short revents;    /* what actually happened        — KERNEL writes this */
};

class server
{
private:
    int _serverfd;
    std::map<int, client*> _clients;

public:
    server();
    ~server();

    void tolisten();
    int acceptclient();

    bool recievefrom(client* c);
    bool sendto(client* c);

    int select_m();
    void poll_m();
    void epoll_m();
};

class client
{
private:
    int _clientfd;
    int id;
    std::string in_buffer;
    std::string out_buffer;

public:
    client(int fd, int i);
    ~client();

    void appendInBuffer(const char* data, std::size_t n);

    int getfd() const;

    const std::string& getOutBuffer() const;

    void eraseFromOutBuffer(std::size_t n);
};

#endif