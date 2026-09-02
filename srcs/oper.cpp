/* ************************************************************************** */
/*                                                                            */
/*   oper.cpp                                    OWNER C - MODE KICK INVITE    */
/*                                                                            */
/*   The three operator commands. All of them validate here and produce the    */
/*   numeric themselves; Channel only ever performs the change, which is why   */
/*   its own guards look redundant -- they are the second line of defence.     */
/*                                                                            */
/* ************************************************************************** */

#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "commands.hpp"
#include "irc_utils.hpp"
#include "replies.hpp"

void	cmdMode(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "MODE"));
		return ;
	}

	const std::string&	target = m.arg(0);

	/* User modes: we implement none, but a client asking about itself must
	   still get a well-formed answer instead of an error. */
	if (target[0] != '#' && target[0] != '&')
	{
		if (ircEqual(target, c.getNick()))
			c.queue(RPL_UMODEIS(me, "+"));
		else
			c.queue(ERR_USERSDONTMATCH(me));
		return ;
	}

	Channel*	ch = srv.findChannel(target);

	if (ch == NULL)
	{
		c.queue(ERR_NOSUCHCHANNEL(me, target));
		return ;
	}

	/* A bare `MODE #channel` is a query, and anyone may ask. */
	if (m.argc() < 2)
	{
		c.queue(RPL_CHANNELMODEIS(me, ch->getName(), ch->modeString()));
		return ;
	}

	if (!ch->isMember(c))
	{
		c.queue(ERR_NOTONCHANNEL(me, ch->getName()));
		return ;
	}
	if (!ch->isOp(c))
	{
		c.queue(ERR_CHANOPRIVSNEEDED(me, ch->getName()));
		return ;
	}

	std::vector<std::string>	args(m.params.begin() + 2, m.params.end());
	ModeApply					applied;

	ch->applyModes(c, m.arg(1), args, applied);

	for (std::size_t i = 0; i < applied.unknown.size(); ++i)
		c.queue(ERR_UNKNOWNMODE(me, std::string(1, applied.unknown[i])));
	for (std::size_t i = 0; i < applied.notInChannel.size(); ++i)
		c.queue(ERR_USERNOTINCHANNEL(me, applied.notInChannel[i], ch->getName()));

	/* Only what really changed gets announced. */
	if (!applied.modes.empty())
	{
		std::string	line = ":" + clientPrefix(c) + " MODE " + ch->getName() + " " + applied.modes;

		for (std::size_t i = 0; i < applied.args.size(); ++i)
			line += " " + applied.args[i];
		line += CRLF;
		ch->broadcast(line, NULL);
	}
}

void	cmdKick(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 2)
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "KICK"));
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
	if (!ch->isOp(c))
	{
		c.queue(ERR_CHANOPRIVSNEEDED(me, ch->getName()));
		return ;
	}

	/* No reason given means the kicker's own nick, as every server does. */
	const std::string			reason  = (m.hasTrailing && m.argc() > 2) ? m.trailing : c.getNick();
	std::vector<std::string>	victims = splitComma(m.arg(1));

	for (std::size_t i = 0; i < victims.size(); ++i)
	{
		Client*	victim = ch->findMember(victims[i]);

		if (victim == NULL)
		{
			c.queue(ERR_USERNOTINCHANNEL(me, victims[i], ch->getName()));
			continue ;
		}
		/* Announced before the removal, so the victim is told why. */
		ch->broadcast(":" + clientPrefix(c) + " KICK " + ch->getName()
			+ " " + victim->getNick() + " :" + reason + CRLF, NULL);
		ch->kick(c, *victim, reason);
	}
	srv.dropChannelIfEmpty(ch);
}

void	cmdInvite(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 2)
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "INVITE"));
		return ;
	}

	Client*	guest = srv.findClientByNick(m.arg(0));

	if (guest == NULL)
	{
		c.queue(ERR_NOSUCHNICK(me, m.arg(0)));
		return ;
	}

	Channel*	ch = srv.findChannel(m.arg(1));

	if (ch == NULL)
	{
		c.queue(ERR_NOSUCHCHANNEL(me, m.arg(1)));
		return ;
	}
	if (!ch->isMember(c))
	{
		c.queue(ERR_NOTONCHANNEL(me, ch->getName()));
		return ;
	}
	if (ch->isMember(*guest))
	{
		c.queue(ERR_USERONCHANNEL(me, guest->getNick(), ch->getName()));
		return ;
	}
	/* On an open channel any member may invite; +i reserves it to operators. */
	if (ch->isInviteOnly() && !ch->isOp(c))
	{
		c.queue(ERR_CHANOPRIVSNEEDED(me, ch->getName()));
		return ;
	}

	ch->addInvite(guest->getNick());
	c.queue(RPL_INVITING(me, ch->getName(), guest->getNick()));
	guest->queue(":" + clientPrefix(c) + " INVITE " + guest->getNick() + " :" + ch->getName() + CRLF);
}
