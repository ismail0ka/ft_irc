/* ************************************************************************** */
/*                                                                            */
/*   CommandDispatcher.hpp                                OWNER B - protocol  */
/*                                                                            */
/*   The one and only place that turns a command name into code. Server owns   */
/*   an instance by value and calls dispatch() once per complete line read     */
/*   from a client. The table is built once, in the constructor.               */
/*                                                                            */
/*   It holds no state about clients and never owns anything -- Server owns    */
/*   every Client and Channel.                                                 */
/*                                                                            */
/* ************************************************************************** */

#ifndef COMMANDDISPATCHER_HPP
# define COMMANDDISPATCHER_HPP

# include <map>
# include <string>

class Server;
class Client;
struct Message;

class CommandDispatcher
{
	public:

		CommandDispatcher();
		~CommandDispatcher();

		/* Routes one message. Guaranteed to return without throwing and
		   without ever calling send(): anything it wants to say to the
		   client goes through Client::queue().

		   - empty line               -> ignored silently (RFC 1459 2.3)
		   - unknown command          -> ERR_UNKNOWNCOMMAND (421)
		   - not registered yet       -> ERR_NOTREGISTERED  (451),
		                                 except for the handshake commands
		   - otherwise                -> the handler runs */
		void	dispatch(Server& srv, Client& c, const Message& m);

	private:

		typedef void (*Handler)(Server&, Client&, const Message&);

		void	bind(const std::string& name, Handler h);

		/* PASS / NICK / USER build the registration, QUIT and PING / PONG must
		   stay reachable so a half-registered client can leave or answer a
		   liveness check. Everything else needs a registered client. */
		static bool	allowedBeforeRegistration(const std::string& command);

		std::map<std::string, Handler>	_table;

		/* Copying the table would be pointless and is never needed: Server
		   holds exactly one dispatcher for its whole lifetime. */
		CommandDispatcher(const CommandDispatcher& other);
		CommandDispatcher&	operator=(const CommandDispatcher& other);
};

#endif
