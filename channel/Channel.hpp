#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>
#include <vector>

// #include "Client.hpp"

enum JoinResult
{
    JOIN_OK = 0,
    JOIN_ALREADY_MEMBER,
    JOIN_CHANNEL_FULL,
    JOIN_INVITE_ONLY,
    JOIN_NOT_INVITED,
    JOIN_BAD_KEY
};

class Channel
{
    private:
        std::string           _name;
        std::string           _topic;
        std::string           _key;
        size_t                _limit;

        bool                  _inviteOnly;
        bool                  _topicLocked;

        std::set<Client *>    _members;
        std::set<Client *>    _operators;
        std::set<std::string> _invited;

        bool                  _hasKey;
        bool                  _hasLimit;

        Channel(const Channel &other);
        Channel &operator=(const Channel &other);
    
    public:
        Channel(const std::string &name);
        ~Channel();
    
        int                     join(Client &c, const std::string &key);
        void                    part(Client &c, const std::string &reason); //af apply this meth uu need to check isEmpty() 
        void                    kick(Client &op, Client &target, const std::string &reason);
        void                    invite(Client &op, const std::string &nick);
        void                    setTopic(Client &c, const std::string &topic);
    
        void                    applyModes(Client &c, const std::string &str, const std::vector<std::string> &args);
    
        void                    broadcast(const std::string &msg, Client *except);
    
        bool                    isOp(Client &c) const;
        bool                    isEmpty() const;
};

#endif