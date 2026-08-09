/* ************************************************************************** */
/*                                                                            */
/*   Channel.cpp                                          OWNER C - channels  */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>

#include "Channel.hpp"

#include "Client.hpp"
#include "irc_utils.hpp"

Channel::Channel(const std::string& name)
	: _name(name), _topic(), _key(), _limit(0), _hasKey(false),
	  _hasLimit(false), _inviteOnly(false), _topicLocked(false), _members(),
	  _operators(), _invited()
{
}

Channel::~Channel()
{
}

/* -------------------------------------------------------------------------- */
/*  membership                                                                */
/* -------------------------------------------------------------------------- */

int	Channel::join(Client& c, const std::string& key)
{
	if (isMember(c))
		return (JOIN_ALREADY_MEMBER);
	/* Same order as the classic ircds: key, then limit, then invitation. */
	if (_hasKey && key != _key)
		return (JOIN_BAD_KEY);
	if (_hasLimit && _members.size() >= _limit)
		return (JOIN_CHANNEL_FULL);
	if (_inviteOnly && !isInvited(c.getNick()))
		return (JOIN_NOT_INVITED);

	/* Whoever opens the channel runs it. */
	if (_members.empty())
		_operators.insert(&c);
	_members.insert(&c);
	/* An invitation is spent once it is used. */
	_invited.erase(ircLower(c.getNick()));
	return (JOIN_OK);
}

void	Channel::part(Client& c, const std::string&)
{
	_members.erase(&c);
	_operators.erase(&c);
}

void	Channel::kick(Client&, Client& target, const std::string&)
{
	_members.erase(&target);
	_operators.erase(&target);
}

void	Channel::addInvite(const std::string& nick)
{
	_invited.insert(ircLower(nick));
}

void	Channel::setTopic(Client&, const std::string& topic)
{
	_topic = topic;
}

/* -------------------------------------------------------------------------- */
/*  modes                                                                     */
/* -------------------------------------------------------------------------- */

/* Appends one change that really happened. The sign is only written when it
   flips, so "+i +t -k" reads back as "+it-k". */
static void	record(ModeApply& out, char& lastSign, char sign, char mode,
			   const std::string& arg)
{
	if (lastSign != sign)
	{
		out.modes += sign;
		lastSign = sign;
	}
	out.modes += mode;
	if (!arg.empty())
		out.args.push_back(arg);
}

void	Channel::applyModes(Client&, const std::string& str,
					const std::vector<std::string>& args, ModeApply& out)
{
	char					sign = '+';
	char					lastSign = 0;
	std::size_t				argi = 0;
	std::string::size_type	i = 0;

	while (i < str.size())
	{
		const char	mode = str[i];
		const bool	adding = (sign == '+');

		++i;
		if (mode == '+' || mode == '-')
		{
			sign = mode;
			continue ;
		}
		if (mode == 'i')
		{
			if (_inviteOnly == adding)
				continue ;			/* already in that state: no-op */
			_inviteOnly = adding;
			record(out, lastSign, sign, 'i', "");
		}
		else if (mode == 't')
		{
			if (_topicLocked == adding)
				continue ;
			_topicLocked = adding;
			record(out, lastSign, sign, 't', "");
		}
		else if (mode == 'k')
		{
			/* Both "+k key" and "-k key" carry an argument; the removal
			   ignores its value but must still consume it so the following
			   arguments keep lining up with their mode letters. */
			if (argi >= args.size())
				continue ;

			const std::string	given = args[argi++];

			if (adding)
			{
				if (given.empty() || (_hasKey && _key == given))
					continue ;
				_key = given;
				_hasKey = true;
				record(out, lastSign, sign, 'k', _key);
			}
			else
			{
				if (!_hasKey)
					continue ;
				_key.clear();
				_hasKey = false;
				record(out, lastSign, sign, 'k', "");
			}
		}
		else if (mode == 'l')
		{
			if (adding)
			{
				if (argi >= args.size())
					continue ;

				const std::string	given = args[argi++];
				const long			value = std::atol(given.c_str());

				/* A limit that is not a positive number is not a limit. */
				if (value <= 0)
					continue ;
				if (_hasLimit && _limit == static_cast<std::size_t>(value))
					continue ;
				_limit = static_cast<std::size_t>(value);
				_hasLimit = true;
				record(out, lastSign, sign, 'l', given);
			}
			else
			{
				if (!_hasLimit)
					continue ;
				_limit = 0;
				_hasLimit = false;
				record(out, lastSign, sign, 'l', "");
			}
		}
		else if (mode == 'o')
		{
			if (argi >= args.size())
				continue ;

			const std::string	who = args[argi++];
			Client*				target = findMember(who);

			if (target == NULL)
			{
				out.notInChannel.push_back(who);
				continue ;
			}
			if (adding)
			{
				if (_operators.find(target) != _operators.end())
					continue ;
				_operators.insert(target);
			}
			else
			{
				if (_operators.find(target) == _operators.end())
					continue ;
				_operators.erase(target);
			}
			record(out, lastSign, sign, 'o', who);
		}
		else
			out.unknown += mode;
	}
}

std::string	Channel::modeString() const
{
	std::string	modes = "+";
	std::string	params;

	if (_inviteOnly)
		modes += "i";
	if (_topicLocked)
		modes += "t";
	/* Advertise that a key exists without handing it out: anyone may query
	   the modes of a channel they are not even on. */
	if (_hasKey)
		modes += "k";
	if (_hasLimit)
	{
		modes += "l";
		params += " " + toStr(_limit);
	}
	return (modes + params);
}

/* -------------------------------------------------------------------------- */
/*  queries                                                                   */
/* -------------------------------------------------------------------------- */

void	Channel::broadcast(const std::string& msg, Client* except)
{
	std::set<Client*>::iterator	it;

	for (it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it != except)
			(*it)->queue(msg);
	}
}

Client*	Channel::findMember(const std::string& nick) const
{
	MemberCIt	it;

	for (it = _members.begin(); it != _members.end(); ++it)
	{
		if (ircEqual((*it)->getNick(), nick))
			return (*it);
	}
	return (NULL);
}

const std::set<Client*>&	Channel::getMembers() const
{
	return (_members);
}

const std::string&	Channel::getName() const
{
	return (_name);
}

const std::string&	Channel::getTopic() const
{
	return (_topic);
}

bool	Channel::hasTopic() const
{
	return (!_topic.empty());
}

bool	Channel::isEmpty() const
{
	return (_members.empty());
}

bool	Channel::isMember(Client& c) const
{
	return (_members.find(&c) != _members.end());
}

bool	Channel::isOp(Client& c) const
{
	return (_operators.find(&c) != _operators.end());
}

bool	Channel::isInviteOnly() const
{
	return (_inviteOnly);
}

bool	Channel::isTopicLocked() const
{
	return (_topicLocked);
}

bool	Channel::isInvited(const std::string& nick) const
{
	return (_invited.find(ircLower(nick)) != _invited.end());
}
