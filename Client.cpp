#include "Client.hpp"

#include <unistd.h>

Client::Client(int fd)
    : _clientfd(fd),
      _nick(),
      _user(),
      _realname(),
      _host(),
      _passOk(false),
      _nickSet(false),
      _userSet(false),
      _welcomed(false),
      _inBuffer(),
      _outBuffer()
{
}


Client::~Client()
{
    if (_clientfd >= 0)
        close(_clientfd);
}

int Client::getFd() const
{
    return _clientfd;
}

const std::string& Client::getNick() const
{
    return _nick;
}

const std::string& Client::getUser() const
{
    return _user;
}

const std::string& Client::getRealname() const
{
    return _realname;
}

const std::string& Client::getHost() const
{
    return _host;
}

void Client::setNick(const std::string& nick)
{
    _nick = nick;
}

void Client::setUser(const std::string& user)
{
    _user = user;
}

void Client::setRealname(const std::string& realname)
{
    _realname = realname;
}

void Client::setHost(const std::string& host)
{
    _host = host;
}

bool Client::isPassOk() const
{
    return _passOk;
}

bool Client::isNickSet() const
{
    return _nickSet;
}

bool Client::isUserSet() const
{
    return _userSet;
}

bool Client::isWelcomed() const
{
    return _welcomed;
}


bool Client::isRegistered() const
{
    return _passOk && _nickSet && _userSet;
}

void Client::setPassOk(bool value)
{
    _passOk = value;
}

void Client::setNickSet(bool value)
{
    _nickSet = value;
}

void Client::setUserSet(bool value)
{
    _userSet = value;
}

void Client::setWelcomed(bool value)
{
    _welcomed = value;
}


bool Client::appendIn(const char* data, std::size_t n)
{
    _inBuffer.append(data, n);

<<<<<<< Updated upstream
    // irc messages are capped at 512char!
=======
    
>>>>>>> Stashed changes
    if (_inBuffer.size() > 512 && _inBuffer.find('\n') == std::string::npos)
        return false;

    return true;
}


bool Client::takeLine(std::string& out)
{
    std::string::size_type pos = _inBuffer.find('\n');

    if (pos == std::string::npos)
        return false;

    
    std::size_t len = pos;
    if (len > 0 && _inBuffer[len - 1] == '\r')
        --len;

    out = _inBuffer.substr(0, len);
    _inBuffer.erase(0, pos + 1); 

    return true;
}


void Client::queue(const std::string& msg)
{
    _outBuffer.append(msg);
}

bool Client::hasOut() const
{
    return !_outBuffer.empty();
}

const std::string& Client::outData() const
{
    return _outBuffer;
}


void Client::consumeOut(std::size_t n)
{
    _outBuffer.erase(0, n);
}