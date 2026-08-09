/* ************************************************************************** */
/*                                                                            */
/*   channels.cpp                          OWNER C - JOIN PART TOPIC NAMES     */
/*                                                                            */
/*   A channel exists exactly as long as it has members: JOIN creates it on    */
/*   demand and the last PART takes it away, so a stale +i +k channel can      */
/*   never lock a name out.                                                    */
/*                                                                            */
/* ************************************************************************** */

#include <map>
#include <set>
#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "commands.hpp"
#include "irc_utils.hpp"
#include "replies.hpp"

/* 353 + 366. Operators are flagged '@', which is what clients use to draw
   the member list. */
static void	sendNames(Client& c, Channel& ch)
{
	const std::set<Client*>&			members = ch.getMembers();
	std::set<Client*>::const_iterator	it;
	std::string							names;

	for (it = members.begin(); it != members.end(); ++it)
	{
		if (!names.empty())
			names += " ";
		if (ch.isOp(**it))
			names += "@";
		names += (*it)->getNick();
	}
	c.queue(RPL_NAMREPLY(replyTarget(c), ch.getName(), names));
	c.queue(RPL_ENDOFNAMES(replyTarget(c), ch.getName()));
}

static void	sendTopic(Client& c, Channel& ch)
{
	if (ch.hasTopic())
		c.queue(RPL_TOPIC(replyTarget(c), ch.getName(), ch.getTopic()));
	else
		c.queue(RPL_NOTOPIC(replyTarget(c), ch.getName()));
}

void	cmdJoin(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "JOIN"));
		return ;
	}
	/* "JOIN 0" is the RFC's leave-everything shorthand. */
	if (m.arg(0) == "0")
	{
		srv.partAllChannels(c, true);
		return ;
	}

	std::vector<std::string>	names = splitComma(m.arg(0));
	std::vector<std::string>	keys  = splitComma(m.arg(1));

	for (std::size_t i = 0; i < names.size(); ++i)
	{
		const std::string&	name = names[i];
		const std::string	key  = (i < keys.size()) ? keys[i] : std::string();

		if (!isValidChannelName(name))
		{
			c.queue(ERR_NOSUCHCHANNEL(me, name));
			continue ;
		}

		Channel*	ch      = srv.findChannel(name);
		const bool	created = (ch == NULL);

		if (created)
			ch = srv.createChannel(name);

		const int	result = ch->join(c, key);

		if (result != JOIN_OK)
		{
			if (result == JOIN_CHANNEL_FULL)
				c.queue(ERR_CHANNELISFULL(me, ch->getName()));
			else if (result == JOIN_INVITE_ONLY || result == JOIN_NOT_INVITED)
				c.queue(ERR_INVITEONLYCHAN(me, ch->getName()));
			else if (result == JOIN_BAD_KEY)
				c.queue(ERR_BADCHANNELKEY(me, ch->getName()));
			/* JOIN_ALREADY_MEMBER is silent -- re-JOINing is not an error. */

			if (created)
				srv.dropChannelIfEmpty(ch);
			continue ;
		}

		/* The joiner sees its own JOIN too: that is how a client learns the
		   channel window is now open. */
		ch->broadcast(":" + c.getPrefix() + " JOIN :" + ch->getName() + CRLF, NULL);
		sendTopic(c, *ch);
		sendNames(c, *ch);
	}
}

void	cmdPart(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "PART"));
		return ;
	}

	const std::string			reason = m.hasTrailing ? m.trailing : std::string();
	std::vector<std::string>	names  = splitComma(m.arg(0));

	for (std::size_t i = 0; i < names.size(); ++i)
	{
		Channel*	ch = srv.findChannel(names[i]);

		if (ch == NULL)
		{
			c.queue(ERR_NOSUCHCHANNEL(me, names[i]));
			continue ;
		}
		if (!ch->isMember(c))
		{
			c.queue(ERR_NOTONCHANNEL(me, ch->getName()));
			continue ;
		}

		std::string	line = ":" + c.getPrefix() + " PART " + ch->getName();

		if (!reason.empty())
			line += " :" + reason;
		line += CRLF;

		/* Announced before the removal, so the leaver gets its own PART. */
		ch->broadcast(line, NULL);
		ch->part(c, reason);
		srv.dropChannelIfEmpty(ch);
	}
}

void	cmdTopic(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "TOPIC"));
		return ;
	}

	Channel*	ch = srv.findChannel(m.arg(0));

	if (ch == NULL)
	{
		c.queue(ERR_NOSUCHCHANNEL(me, m.arg(0)));
		return ;
	}
	if (!ch->isMember(c))
	{
		c.queue(ERR_NOTONCHANNEL(me, ch->getName()));
		return ;
	}

	/* One argument is a query, two is a change -- including `TOPIC #c :` with
	   an empty trailing, which is how a topic gets cleared. */
	if (m.argc() < 2)
	{
		sendTopic(c, *ch);
		return ;
	}

	if (ch->isTopicLocked() && !ch->isOp(c))
	{
		c.queue(ERR_CHANOPRIVSNEEDED(me, ch->getName()));
		return ;
	}

	const std::string	topic = m.hasTrailing ? m.trailing : m.arg(1);

	ch->setTopic(c, topic);
	ch->broadcast(":" + c.getPrefix() + " TOPIC " + ch->getName() + " :" + topic + CRLF, NULL);
}

void	cmdNames(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		std::map<std::string, Channel*>&			all = srv.channels();
		std::map<std::string, Channel*>::iterator	it;

		for (it = all.begin(); it != all.end(); ++it)
			sendNames(c, *it->second);
		c.queue(RPL_ENDOFNAMES(me, "*"));
		return ;
	}

	std::vector<std::string>	names = splitComma(m.arg(0));

	for (std::size_t i = 0; i < names.size(); ++i)
	{
		Channel*	ch = srv.findChannel(names[i]);

		if (ch == NULL)
			c.queue(RPL_ENDOFNAMES(me, names[i]));
		else
			sendNames(c, *ch);
	}
}
