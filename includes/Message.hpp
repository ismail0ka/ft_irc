/* ************************************************************************** */
/*                                                                            */
/*   Message.hpp                                          OWNER B - protocol  */
/*                                                                            */
/*   One parsed IRC line. Pure data, no behaviour, no allocation of its own.   */
/*   Produced by MessageParser, consumed by CommandDispatcher and handlers.    */
/*                                                                            */
/* ************************************************************************** */

#ifndef MESSAGE_HPP
# define MESSAGE_HPP

# include <cstddef>
# include <string>
# include <vector>

/* RFC 1459 / 2812: a message never carries more than 15 parameters and never
   exceeds 512 bytes including the terminating CRLF. */
# define MSG_MAX_PARAMS 15
# define MSG_MAX_LEN    512

struct Message
{
	/* Sender, without the leading ':'. Empty for everything a client sends
	   to us -- clients are not allowed to forge a prefix. */
	std::string					prefix;

	/* Always upper-cased by MessageParser, so table lookups stay exact. */
	std::string					command;

	/* EVERY parameter in order, trailing one included as the last element.
	   `PRIVMSG #42 :hello world` -> params = ["#42", "hello world"]. */
	std::vector<std::string>	params;

	/* Copy of the trailing parameter (the ':' one), "" when there is none.
	   It is the same string as params.back() when hasTrailing is true; it is
	   duplicated so handlers can grab a free-text argument without caring
	   about how many middles came before it. */
	std::string					trailing;

	/* True when the last parameter was a trailing parameter, i.e. it may hold
	   spaces. Also true for the 15th parameter, which swallows the rest of the
	   line even without a ':' (RFC 2812 2.3.1). */
	bool						hasTrailing;

	Message();

	/* params[i], or a static empty string when i is out of range -- so a
	   handler can read a missing argument without checking argc() first. */
	const std::string&	arg(std::size_t i) const;

	std::size_t			argc() const;

	/* True for a line that carried no command at all (blank line, only
	   spaces, or prefix with nothing after it). Such a line must be ignored. */
	bool				empty() const;
};

#endif
