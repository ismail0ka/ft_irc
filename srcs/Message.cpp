/* ************************************************************************** */
/*                                                                            */
/*   Message.cpp                                          OWNER B - protocol  */
/*                                                                            */
/* ************************************************************************** */

#include "Message.hpp"

Message::Message() : prefix(), command(), params(), trailing(), hasTrailing(false)
{
}

const std::string&	Message::arg(std::size_t i) const
{
	static const std::string	none;

	if (i >= params.size())
		return (none);
	return (params[i]);
}

std::size_t	Message::argc() const
{
	return (params.size());
}

bool	Message::empty() const
{
	return (command.empty());
}
