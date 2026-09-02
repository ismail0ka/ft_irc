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
#include "irc_utils.hpp"
#include "replies.hpp"

/* replyTarget() is the shared version of "the nick, or '*' before NICK": a
   numeric always needs a target field. */

static void	replyUnknown(Client& c, const std::string& command)
{
	c.queue(ERR_UNKNOWNCOMMAND(replyTarget(c), command));
}

static void	replyNotRegistered(Client& c)
{
	c.queue(ERR_NOTREGISTERED(replyTarget(c)));
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
