/* ************************************************************************** */
/*                                                                            */
/*   irc_utils.cpp                                       SHARED - small tools */
/*                                                                            */
/* ************************************************************************** */

#include "irc_utils.hpp"

#include <cctype>
#include <sstream>

#include "Client.hpp"

std::string	ircLower(const std::string& s)
{
	std::string	out(s);

	for (std::string::size_type i = 0; i < out.size(); ++i)
	{
		char	ch = out[i];

		if (ch == '[')
			out[i] = '{';
		else if (ch == ']')
			out[i] = '}';
		else if (ch == '\\')
			out[i] = '|';
		else
			out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}
	return (out);
}

bool	ircEqual(const std::string& a, const std::string& b)
{
	return (ircLower(a) == ircLower(b));
}

static bool	isNickSpecial(char c)
{
	return (c == '[' || c == ']' || c == '\\' || c == '`'
		|| c == '_' || c == '^' || c == '{' || c == '|' || c == '}');
}

bool	isValidNick(const std::string& nick)
{
	if (nick.empty() || nick.size() > 30)
		return (false);
	if (!std::isalpha(static_cast<unsigned char>(nick[0])) && !isNickSpecial(nick[0]))
		return (false);

	for (std::string::size_type i = 1; i < nick.size(); ++i)
	{
		char	c = nick[i];

		if (!std::isalnum(static_cast<unsigned char>(c)) && !isNickSpecial(c) && c != '-')
			return (false);
	}
	return (true);
}

bool	isValidChannelName(const std::string& name)
{
	if (name.size() < 2 || name.size() > 50)
		return (false);
	if (name[0] != '#' && name[0] != '&')
		return (false);

	for (std::string::size_type i = 1; i < name.size(); ++i)
	{
		char	c = name[i];

		if (c == ' ' || c == ',' || c == '\a' || c == '\r' || c == '\n')
			return (false);
	}
	return (true);
}

std::vector<std::string>	splitComma(const std::string& s)
{
	std::vector<std::string>	out;
	std::string::size_type		start = 0;

	while (start <= s.size())
	{
		std::string::size_type	pos = s.find(',', start);

		if (pos == std::string::npos)
		{
			if (start < s.size())
				out.push_back(s.substr(start));
			break ;
		}
		if (pos > start)
			out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	return (out);
}

std::string	toStr(std::size_t n)
{
	std::ostringstream	oss;

	oss << n;
	return (oss.str());
}

std::string	replyTarget(const Client& c)
{
	if (c.getNick().empty())
		return (std::string("*"));
	return (c.getNick());
}

std::string	clientPrefix(const Client& c)
{
	std::string	nick = c.getNick().empty() ? std::string("*") : c.getNick();
	std::string	user = c.getUser().empty() ? std::string("unknown") : c.getUser();
	std::string	host = c.getHost().empty() ? std::string("localhost") : c.getHost();

	return (nick + "!" + user + "@" + host);
}
