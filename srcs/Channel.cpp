#include "Channel.hpp"

#include <cstdlib>

#include "irc_utils.hpp"

Channel::Channel(const std::string &name): _name(name), _topic(""), _key(""), _limit(0), _inviteOnly(false)
, _topicLocked(false), _hasKey(false), _hasLimit(false) {};

Channel::~Channel() {}

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
    /* An invite is spent the moment it is used. */
    _invited.erase(c.getNick());
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
    if (!isOp(op))
        return ;
    if (_members.find(&target) == _members.end())
        return ;
    _members.erase(&target);
    _operators.erase(&target);
}

void                    Channel::invite(Client &op, const std::string &nick)
{
    if (!isOp(op))
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
        if (!isOp(c))
            return ;
    }
    this->_topic = topic;
}

/* Records one applied mode char, opening a new '+'/'-' run only when the
   direction actually flips -- "+it" stays "+it", never "+i+t". */
static void     noteMode(ModeApply &out, bool addingMode, char mode)
{
    const char  sign = addingMode ? '+' : '-';

    if (out.modes.empty() || out.modes.find_last_of("+-") == std::string::npos
        || out.modes[out.modes.find_last_of("+-")] != sign)
        out.modes += sign;
    out.modes += mode;
}

void Channel::applyModes(Client &c, const std::string &str, const std::vector<std::string> &args,
                         ModeApply &out)
{
    if (!isOp(c))
        return;

    bool addingMode = true;
    std::vector<std::string>::const_iterator it = args.begin();

    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '+')
        {
            addingMode = true;
            continue;
        }
        if (str[i] == '-')
        {
            addingMode = false;
            continue;
        }
        handleMode(addingMode, str[i], it, args, out);
    }
}

void Channel::handleMode(bool addingMode, char mode, std::vector<std::string>::const_iterator &it,
                         const std::vector<std::string> &args, ModeApply &out)
{
    if (mode == 'i')
        handleModeI(addingMode, out);
    else if (mode == 't')
        handleModeT(addingMode, out);
    else if (mode == 'k')
        handleModeK(addingMode, it, args, out);
    else if (mode == 'o')
        handleModeO(addingMode, it, args, out);
    else if (mode == 'l')
        handleModeL(addingMode, it, args, out);
    else
        out.unknown += mode;
}

void Channel::handleModeI(bool addingMode, ModeApply &out)
{
    _inviteOnly = addingMode;
    noteMode(out, addingMode, 'i');
}

void Channel::handleModeT(bool addingMode, ModeApply &out)
{
    _topicLocked = addingMode;
    noteMode(out, addingMode, 't');
}

void Channel::handleModeK(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args, ModeApply &out)
{
    if (!addingMode)
    {
        _hasKey = false, _key.clear();
        noteMode(out, addingMode, 'k');
        return;
    }
    if (it == args.end())
        return;
    _hasKey = true, _key = *it;
    noteMode(out, addingMode, 'k');
    out.args.push_back(*it);
    ++it;
}

void Channel::handleModeO(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args, ModeApply &out)
{
    if (it == args.end())
        return;

    Client  *target = findMember(*it);

    if (target == NULL)
        out.notInChannel.push_back(*it);
    else
    {
        if (addingMode)
            _operators.insert(target);
        else
            _operators.erase(target);
        noteMode(out, addingMode, 'o');
        out.args.push_back(target->getNick());
    }
    ++it;
}

void Channel::handleModeL(bool addingMode, std::vector<std::string>::const_iterator &it,
                          const std::vector<std::string> &args, ModeApply &out)
{
    if (!addingMode)
    {
        _hasLimit = false, _limit = 0;
        noteMode(out, addingMode, 'l');
        return;
    }
    if (it == args.end())
        return;

    const long  wanted = std::atol(it->c_str());

    /* "+l 0" and "+l -5" say nothing useful; ignore rather than lock the
       channel out at zero members. */
    if (wanted > 0)
    {
        _hasLimit = true, _limit = static_cast<size_t>(wanted);
        noteMode(out, addingMode, 'l');
        out.args.push_back(*it);
    }
    ++it;
}

void Channel::broadcast(const std::string &msg, Client *except)
{
    std::set<Client *>::iterator it;

    for (it = _members.begin(); it != _members.end(); ++it)
    {
        if (*it != except)
            (*it)->queue(msg);
    }
}

bool                    Channel::isOp(Client &c) const
{
    if (_operators.find(&c) == _operators.end())
        return false;
    return true;
}

bool Channel::isEmpty() const
{
    return _members.empty();
}

const std::string       &Channel::getName() const
{
    return _name;
}

const std::string       &Channel::getTopic() const
{
    return _topic;
}

bool                    Channel::hasTopic() const
{
    return !_topic.empty();
}

const std::set<Client *> &Channel::getMembers() const
{
    return _members;
}

bool                    Channel::isMember(Client &c) const
{
    return _members.find(&c) != _members.end();
}

Client                  *Channel::findMember(const std::string &nick)
{
    std::set<Client *>::iterator it;

    for (it = _members.begin(); it != _members.end(); ++it)
    {
        if (ircEqual((*it)->getNick(), nick))
            return *it;
    }
    return NULL;
}

void                    Channel::addInvite(const std::string &nick)
{
    _invited.insert(nick);
}

bool                    Channel::isInviteOnly() const
{
    return _inviteOnly;
}

bool                    Channel::isTopicLocked() const
{
    return _topicLocked;
}

std::string             Channel::modeString() const
{
    std::string modes = "+";
    std::string args;

    if (_inviteOnly)
        modes += "i";
    if (_topicLocked)
        modes += "t";
    if (_hasKey)
        modes += "k", args += " " + _key;
    if (_hasLimit)
        modes += "l", args += " " + toStr(_limit);

    return modes + args;
}