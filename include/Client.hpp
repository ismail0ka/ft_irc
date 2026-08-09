#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <cstddef>
#include <string>

class Client
{
private:
    int             _clientfd;
    std::string     _nick;
    std::string     _user;
    std::string     _realname;
    std::string     _host;

    bool            _passOk;
    bool            _nickSet;
    bool            _userSet;
    bool            _welcomed;

    std::string     _inBuffer;
    std::string     _outBuffer;

    Client(const Client& other);
    Client& operator=(const Client& other);

public:
    explicit Client(int fd);
    ~Client();

    int getFd() const;

    const std::string& getNick() const;
    const std::string& getUser() const;
    const std::string& getRealname() const;
    const std::string& getHost() const;

    /* "nick!user@host" -- the source every message this client originates is
       stamped with. Fields still unset fall back to placeholders so the line
       stays well-formed even mid-registration. */
    std::string getPrefix() const;

    void setNick(const std::string& nick);
    void setUser(const std::string& user);
    void setRealname(const std::string& realname);
    void setHost(const std::string& host);

    bool isPassOk() const;
    bool isNickSet() const;
    bool isUserSet() const;
    bool isWelcomed() const;
     bool isRegistered() const;

    void setPassOk(bool value);
    void setNickSet(bool value);
    void setUserSet(bool value);
    void setWelcomed(bool value);

    bool appendIn(const char* data, std::size_t n);
    bool takeLine(std::string& out);

    void queue(const std::string& msg);
    bool hasOut() const;
    const std::string& outData() const;
    void consumeOut(std::size_t n);
};

#endif