/* ************************************************************************** */
/*                                                                            */
/*   registration.cpp                        OWNER B - PASS NICK USER QUIT     */
/*                                                                    PING PONG*/
/*                                                                            */
/*   Registration completes only once PASS, NICK and USER have all landed,     */
/*   in whatever order the client chose to send them. The welcome burst goes   */
/*   out exactly once, guarded by Client::isWelcomed().                        */
/*                                                                            */
/* ************************************************************************** */

#include <string>

#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "commands.hpp"
#include "irc_utils.hpp"
#include "replies.hpp"

/* Fires the 001-004 burst the moment the last of PASS/NICK/USER arrives. */
static void	tryWelcome(Client& c)
{
	if (c.isWelcomed() || !c.isRegistered())
		return ;

	const std::string	me = c.getNick();

	c.setWelcomed(true);
	c.queue(RPL_WELCOME(me, clientPrefix(c)));
	c.queue(RPL_YOURHOST(me));
	c.queue(RPL_CREATED(me));
	c.queue(RPL_MYINFO(me));
	c.queue(RPL_ISUPPORT(me));
	c.queue(RPL_MOTDSTART(me));
	c.queue(RPL_MOTD(me, std::string("Welcome to ") + NETWORK_NAME + ", " + me));
	c.queue(RPL_ENDOFMOTD(me));
}

void	cmdPass(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (c.isRegistered() || c.isPassOk())
	{
		c.queue(ERR_ALREADYREGISTERED(me));
		return ;
	}
	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "PASS"));
		return ;
	}
	if (m.arg(0) != srv.getPassword())
	{
		/* Wrong password is terminal: the client is told why and dropped,
		   rather than left hanging in a half-registered state forever. */
		c.queue(ERR_PASSWDMISMATCH(me));
		srv.disconnect(c.getFd(), "Access denied: incorrect password");
		return ;
	}
	c.setPassOk(true);
	tryWelcome(c);
}

void	cmdNick(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NONICKNAMEGIVEN(me));
		return ;
	}

	const std::string&	want = m.arg(0);

	if (!isValidNick(want))
	{
		c.queue(ERR_ERRONEUSNICKNAME(me, want));
		return ;
	}

	Client*	holder = srv.findClientByNick(want);

	if (holder != NULL && holder != &c)
	{
		c.queue(ERR_NICKNAMEINUSE(me, want));
		return ;
	}
	/* Same client, same spelling: nothing happened. A pure case change does
	   go through, since that is a real rename to everyone watching. */
	if (holder == &c && c.getNick() == want)
		return ;

	const std::string	oldPrefix = clientPrefix(c);
	const bool			renaming  = c.isNickSet();

	c.setNick(want);
	c.setNickSet(true);

	if (renaming && c.isWelcomed())
		srv.broadcastToPeers(c, ":" + oldPrefix + " NICK :" + want + CRLF, true);

	tryWelcome(c);
}

void	cmdUser(Server& srv, Client& c, const Message& m)
{
	const std::string	me = replyTarget(c);

	(void)srv;
	if (c.isRegistered() || c.isUserSet())
	{
		c.queue(ERR_ALREADYREGISTERED(me));
		return ;
	}
	if (m.argc() < 4)
	{
		c.queue(ERR_NEEDMOREPARAMS(me, "USER"));
		return ;
	}

	c.setUser(m.arg(0));
	c.setRealname(m.arg(3));
	c.setUserSet(true);
	tryWelcome(c);
}

void	cmdQuit(Server& srv, Client& c, const Message& m)
{
	std::string	reason = m.hasTrailing ? m.trailing : m.arg(0);

	if (reason.empty())
		reason = "Client Quit";
	srv.disconnect(c.getFd(), reason);
}

void	cmdPing(Server& srv, Client& c, const Message& m)
{
	(void)srv;
	if (m.argc() < 1 || m.arg(0).empty())
	{
		c.queue(ERR_NOORIGIN(replyTarget(c)));
		return ;
	}
	c.queue(std::string(":" SERVER_NAME " PONG " SERVER_NAME " :") + m.arg(0) + CRLF);
}

void	cmdPong(Server& srv, Client& c, const Message& m)
{
	/* Nothing to do: we never send PING, so nothing is waiting on this. */
	(void)srv;
	(void)c;
	(void)m;
}
