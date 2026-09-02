/* ************************************************************************** */
/*                                                                            */
/*   MessageParser.cpp                                    OWNER B - protocol  */
/*                                                                            */
/* ************************************************************************** */

#include <cctype>

#include "MessageParser.hpp"

Message	MessageParser::parse(const std::string& line)
{
	Message	msg;
	It		begin = line.begin();
	It		end = line.end();

	/* CRLF tolerant: Client::takeLine() hands us the line without its LF, a
	   raw nc user sends a bare LF, irssi sends CRLF. Drop whatever is left. */
	while (end != begin && (*(end - 1) == '\n' || *(end - 1) == '\r'))
		--end;

	It	it = begin;

	skipSpaces(it, end);
	if (it != end && *it == ':')
	{
		++it;
		msg.prefix = readPrefix(it, end);
	}
	skipSpaces(it, end);
	if (it == end)
		return (msg);	/* blank line, or a prefix and nothing else */
	msg.command = readCommand(it, end);
	readParams(it, end, msg);
	return (msg);
}

std::string	MessageParser::readPrefix(It& it, It end)
{
	return (readToken(it, end));
}

std::string	MessageParser::readCommand(It& it, It end)
{
	std::string				cmd = readToken(it, end);
	std::string::size_type	i = 0;

	while (i < cmd.size())
	{
		cmd[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd[i])));
		++i;
	}
	return (cmd);
}

void	MessageParser::readParams(It& it, It end, Message& out)
{
	while (true)
	{
		skipSpaces(it, end);
		if (it == end)
			break ;
		if (*it == ':')
		{
			/* Trailing parameter: everything left, spaces included, ':' eaten. */
			++it;
			out.trailing.assign(it, end);
			out.hasTrailing = true;
			out.params.push_back(out.trailing);
			it = end;
			break ;
		}
		if (out.params.size() == static_cast<std::size_t>(MSG_MAX_PARAMS - 1))
		{
			/* 14 middles already read: the 15th parameter swallows the rest of
			   the line even without a leading ':' (RFC 2812 2.3.1). */
			out.trailing.assign(it, end);
			out.hasTrailing = true;
			out.params.push_back(out.trailing);
			it = end;
			break ;
		}
		out.params.push_back(readToken(it, end));
	}
}

void	MessageParser::skipSpaces(It& it, It end)
{
	while (it != end && *it == ' ')
		++it;
}

std::string	MessageParser::readToken(It& it, It end)
{
	It	start = it;

	while (it != end && *it != ' ')
		++it;
	return (std::string(start, it));
}
