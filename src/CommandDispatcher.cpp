/* ************************************************************************** */
/*                                                                            */
/*   CommandDispatcher.cpp                                OWNER B - protocol  */
/*                                                                            */
/*   Needs from OWNER A:                                                       */
/*     Client::isRegistered() const  -> bool                                   */
/*     Client::nick() const          -> std::string (empty before NICK)        */
/*     Client::queue(std::string)    -> void                                   */
/*     replies.hpp : ERR_UNKNOWNCOMMAND(nick, command)                         */
/*                   ERR_NOTREGISTERED(nick)                                   */
/*   Both macros must expand to a full line, CRLF included.                    */
/*   If A names them differently, only replyUnknown()/replyNotRegistered()     */
/*   below have to change -- nothing else in B's code touches replies.hpp.     */
/*                                                                            */
/* ************************************************************************** */

#include "CommandDispatcher.hpp"

#include "Client.hpp"
#include "Message.hpp"
#include "Server.hpp"
#include "commands.hpp"
#include "replies.hpp"

/* A client that has not sent NICK yet has no name to put in a numeric, and
   the RFC uses '*' as the placeholder target in that case. */
static std::string	targetOf(Client& c)
{
	std::string	nick = c.nick();

	if (nick.empty())
		return (std::string("*"));
	return (nick);
}

static void	replyUnknown(Client& c, const std::string& command)
{
	c.queue(ERR_UNKNOWNCOMMAND(targetOf(c), command));
}

static void	replyNotRegistered(Client& c)
{
	c.queue(ERR_NOTREGISTERED(targetOf(c)));
}

CommandDispatcher::CommandDispatcher() : _table()
{
	/* OWNER B */
	bind("PASS", &cmdPass);
	bind("NICK", &cmdNick);
	bind("USER", &cmdUser);
	bind("QUIT", &cmdQuit);
	bind("PING", &cmdPing);
	bind("PONG", &cmdPong);
	bind("PRIVMSG", &cmdPrivmsg);
	bind("NOTICE", &cmdNotice);

	/* OWNER C */
	bind("JOIN", &cmdJoin);
	bind("PART", &cmdPart);
	bind("TOPIC", &cmdTopic);
	bind("MODE", &cmdMode);
	bind("KICK", &cmdKick);
	bind("INVITE", &cmdInvite);
	bind("NAMES", &cmdNames);
}

CommandDispatcher::~CommandDispatcher()
{
}

void	CommandDispatcher::bind(const std::string& name, Handler h)
{
	_table[name] = h;
}

bool	CommandDispatcher::allowedBeforeRegistration(const std::string& command)
{
	static const char* const	allowed[] = { "PASS", "NICK", "USER",
											  "QUIT", "PING", "PONG", 0 };
	std::size_t					i = 0;

	while (allowed[i] != 0)
	{
		if (command == allowed[i])
			return (true);
		++i;
	}
	return (false);
}

void	CommandDispatcher::dispatch(Server& srv, Client& c, const Message& m)
{
	/* An empty line is legal on the wire and must produce no error at all. */
	if (m.empty())
		return ;

	std::map<std::string, Handler>::const_iterator	it = _table.find(m.command);

	if (it == _table.end())
	{
		replyUnknown(c, m.command);
		return ;
	}
	if (!c.isRegistered() && !allowedBeforeRegistration(m.command))
	{
		replyNotRegistered(c);
		return ;
	}
	/* The handler may end up calling Server::disconnect(): after this line the
	   Client reference can be dead, so nothing must touch `c` below it. */
	it->second(srv, c, m);
}
