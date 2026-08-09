/* ************************************************************************** */
/*                                                                            */
/*   MessageParser.hpp                                    OWNER B - protocol  */
/*                                                                            */
/*   RFC 1459 / 2812 line grammar:                                            */
/*                                                                            */
/*     message  =  [ ':' prefix SPACE ] command [ params ] crlf               */
/*     params   =  *14( SPACE middle ) [ SPACE ':' trailing ]                 */
/*              =/ 14( SPACE middle ) [ SPACE [ ':' ] trailing ]              */
/*     middle   =  nospcrlfcl *( ':' / nospcrlfcl )                           */
/*     trailing =  *( ':' / ' ' / nospcrlfcl )                                */
/*                                                                            */
/*   Stateless and allocation-light: one line in, one Message out. It never    */
/*   touches Client, Server or the socket -- so it is unit-testable alone.     */
/*                                                                            */
/* ************************************************************************** */

#ifndef MESSAGEPARSER_HPP
# define MESSAGEPARSER_HPP

# include <string>

# include "Message.hpp"

class MessageParser
{
	public:

		/* Parses one line. The line may or may not still carry its CR/LF:
		   Client::takeLine() strips the LF, some clients send a bare LF, so
		   every trailing CR and LF is removed here as well.

		   The parser is deliberately total: it never throws and never fails.
		   A malformed line simply yields a Message whose command is empty
		   (Message::empty() == true), which CommandDispatcher ignores. */
		static Message	parse(const std::string& line);

	private:

		typedef std::string::const_iterator	It;

		/* Consumes the prefix, ':' already eaten, stops on the first space. */
		static std::string	readPrefix(It& it, It end);

		/* Consumes the command token and upper-cases it, so that "privmsg",
		   "PrivMsg" and "PRIVMSG" all hit the same dispatcher entry. */
		static std::string	readCommand(It& it, It end);

		/* Consumes every remaining parameter into out.params / out.trailing. */
		static void			readParams(It& it, It end, Message& out);

		static void			skipSpaces(It& it, It end);
		static std::string	readToken(It& it, It end);

		/* Static-only class: never instantiated, never copied. */
		MessageParser();
		MessageParser(const MessageParser& other);
		MessageParser&	operator=(const MessageParser& other);
		~MessageParser();
};

#endif
