#include "Channel.hpp"

Channel::Channel(const std::string &name): _name(name), _topic(""), _key(""), _limit(0), _inviteOnly(false)
, _topicLocked(false), _hasKey(false), _hasLimit(false), _addingMode(false) {};

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
    if (_operators.find(&op) == _operators.end())
        return ;

    std::set<Client *>::iterator it;
    for (it = _members.begin(); it != _members.end(); ++it)
    {
        if ((*it)->getNick() == nick)
            return ;
    }

    if (_invited.find(nick) != _invited.end())
        return ;
    _invited.insert(nick);
}

void                    Channel::setTopic(Client &c, const std::string &topic)
{
    if (_topicLocked)
    {
        if (_operators.find(&c) == _operators.end())
            return ;
    }
    this->_topic = topic;
}

void Channel::applyModes(Client &c, const std::string &str, const std::vector<std::string> &args)
{
    if (_operators.find(&c) == _operators.end())
        return;

    bool addingMode = true;
    std::vector<std::string>::const_iterator it = args.begin();

    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '+')
            addingMode = true, continue;

        if (str[i] == '-')
            addingMode = false, continue;
        handleMode(addingMode, str[i], it, args);
    }
}

void Channel::handleMode(bool addingMode, char mode, std::vector<std::string>::const_iterator &it,
                         const std::vector<std::string> &args)
{
    if (mode == 'i')
        handleModeI(addingMode);
    else if (mode == 't')
        handleModeT(addingMode);
    else if (mode == 'k')
        handleModeK(addingMode, it, args);
    else if (mode == 'o')
        handleModeO(addingMode, it, args);
    else if (mode == 'l')
        handleModeL(addingMode, it, args);
}

void Channel::handleModeI(bool addingMode)
{
    _inviteOnly = addingMode;
}

void Channel::handleModeT(bool addingMode)
{
    _topicLocked = addingMode;
}

void Channel::handleModeK(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args)
{
    if (!addingMode)
    {
        _hasKey = false, _key.clear();
        return;
    }
    if (it == args.end())
        return;
    _hasKey = true, _key = *it;
    ++it;
}

void Channel::handleModeO(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args)
{
    if (it == args.end())
        return;

    std::set<Client *>::iterator iter;
    for (iter = _members.begin(); iter != _members.end(); ++iter)
    {
        if ((*iter)->getNick() == *it)
        {
            if (addingMode)
                _operators.insert(*iter);
            else
                _operators.erase(*iter);
            break;
        }
    }
    ++it;
}

void Channel::handleModeL(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args)
{
    if (!addingMode)
    {
        _hasLimit = false, _limit = 0;
        return;
    }
    if (it == args.end())
        return;
    _hasLimit = true, _limit = std::atol(it->c_str());
    ++it;
}