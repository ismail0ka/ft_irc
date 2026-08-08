#include "Channel.hpp"

Channel::Channel(const std::string &name): _name(name), _topic(""), _key(""), _limit(0), _inviteOnly(false)
, _topicLocked(false), _hasKey(false), _hasLimit(false) {};

int                     Channel::join(Client &c, const std::string &key)
{
    if (_members.find(&c) != _members.end())
        return JOIN_ALREADY_MEMBER;

    if (_hasLimit && _members.size() >= _limit)
        return JOIN_CHANNEL_FULL;
    if (_inviteOnly && _invited.find(c.getNick()) == _invited.end())
        return JOIN_NOT_INVITED;
    if (_hasKey && _key != key)
        return JOIN_BAD_KEY;

    if (isEmpty())
        _operators.insert(&c);
    _members.insert(&c);
    return JOIN_OK;
}

void                    Channel::part(Client &c, const std::string &reason)
{
    (void)reason;
    if (_members.find(&c) == _members.end())
        return ;
    _members.erase(&c);
    _operators.erase(&c);
}

void                    Channel::kick(Client &op, Client &target, const std::string &reason)
{
    (void)reason;
    if (_operators.find(&op) == _operators.end())
        return ;
    if (_members.find(&target) == _members.end())
        return ;
    _members.erase(&target);
    _operators.erase(&target);
}

void                    Channel::invite(Client &op, const std::string &nick)
{
    
}