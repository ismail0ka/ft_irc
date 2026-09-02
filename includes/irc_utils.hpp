/* ************************************************************************** */
/*                                                                            */
/*   irc_utils.hpp                                       SHARED - small tools */
/*                                                                            */
/*   Free functions the handlers, Server and Channel all need. No state, no    */
/*   ownership, nothing IRC-protocol-heavy: just the string chores.            */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRC_UTILS_HPP
# define IRC_UTILS_HPP

# include <cstddef>
# include <string>
# include <vector>

class Client;

/* RFC 1459 casemapping: ASCII lowercase, and {}| are the lowercase forms of
   []\ . Nicks and channel names compare through this, so "#Foo" and "#foo"
   are the same channel. */
std::string	ircLower(const std::string& s);
bool		ircEqual(const std::string& a, const std::string& b);

/* First char a letter or one of []\`_^{|} , then those plus digits and '-'.
   Capped at 30 so a nick always fits in a 512-byte line with room to spare. */
bool		isValidNick(const std::string& nick);

/* '#' or '&' followed by 1..49 chars, none of them space, comma, or bell. */
bool		isValidChannelName(const std::string& name);

/* "a,b,,c" -> ["a", "b", "c"]. Empty fields are dropped, which is what the
   comma-separated target lists of JOIN / PART / PRIVMSG want. */
std::vector<std::string>	splitComma(const std::string& s);

/* C++98 has no std::to_string. */
std::string	toStr(std::size_t n);

/* The nick to put in a numeric reply, or "*" for a client that has not sent
   NICK yet -- a numeric always needs a target field. */
std::string	replyTarget(const Client& c);

/* "nick!user@host" -- the source every message a client originates is stamped
   with. Fields still unset fall back to placeholders so the line stays
   well-formed even mid-registration. */
std::string	clientPrefix(const Client& c);

#endif
