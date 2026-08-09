/* ************************************************************************** */
/*                                                                            */
/*   messaging.cpp                                 OWNER B - PRIVMSG NOTICE    */
/*                                                                            */
/*   Same delivery path for both. The one real difference is that NOTICE must  */
/*   never generate an automatic reply (RFC 1459 4.4.2) -- otherwise two bots  */
/*   notice-ing each other loop forever -- so every error is swallowed.        */
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

static void	deliver(Server& srv, Client& c, const Message& m, bool notice)
{
	const std::string	me   = replyTarget(c);
	const char* const	verb = notice ? " NOTICE " : " PRIVMSG ";

	if (m.argc() < 1 || m.arg(0).empty())
	{
		if (!notice)
			c.queue(ERR_NORECIPIENT(me, notice ? "NOTICE" : "PRIVMSG"));
		return ;
	}

	const std::string	text = m.hasTrailing ? m.trailing : m.arg(1);

	if (text.empty())
	{
		if (!notice)
			c.queue(ERR_NOTEXTTOSEND(me));
		return ;
	}

	std::vector<std::string>	targets = splitComma(m.arg(0));

	for (std::size_t i = 0; i < targets.size(); ++i)
	{
		const std::string&	t = targets[i];

		if (t[0] == '#' || t[0] == '&')
		{
			Channel*	ch = srv.findChannel(t);

			if (ch == NULL)
			{
				if (!notice)
					c.queue(ERR_NOSUCHCHANNEL(me, t));
				continue ;
			}
			/* No external messages: you talk to a channel you are on. */
			if (!ch->isMember(c))
			{
				if (!notice)
					c.queue(ERR_CANNOTSENDTOCHAN(me, ch->getName()));
				continue ;
			}
			ch->broadcast(":" + c.getPrefix() + verb + ch->getName() + " :" + text + CRLF, &c);
		}
		else
		{
			Client*	dst = srv.findClientByNick(t);

			if (dst == NULL)
			{
				if (!notice)
					c.queue(ERR_NOSUCHNICK(me, t));
				continue ;
			}
			dst->queue(":" + c.getPrefix() + verb + dst->getNick() + " :" + text + CRLF);
		}
	}
}

void	cmdPrivmsg(Server& srv, Client& c, const Message& m)
{
	deliver(srv, c, m, false);
}

void	cmdNotice(Server& srv, Client& c, const Message& m)
{
	deliver(srv, c, m, true);
}
