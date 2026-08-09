/* ************************************************************************** */
/*                                                                            */
/*   test_parser.cpp                                      OWNER B - protocol  */
/*                                                                            */
/*   Standalone unit test for Message + MessageParser. Depends on nothing      */
/*   else, so it runs before Server or Channel exist:                          */
/*                                                                            */
/*     c++ -Wall -Wextra -Werror -std=c++98 -Iinclude \                        */
/*         src/Message.cpp src/MessageParser.cpp tests/test_parser.cpp \       */
/*         -o test_parser && ./test_parser                                     */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "MessageParser.hpp"

static int	g_failed = 0;
static int	g_total = 0;

static void	check(const std::string& what, const std::string& got,
				  const std::string& want)
{
	++g_total;
	if (got == want)
		return ;
	++g_failed;
	std::cout << "FAIL  " << what << "\n      got  [" << got
			  << "]\n      want [" << want << "]" << std::endl;
}

static void	checkSize(const std::string& what, std::size_t got, std::size_t want)
{
	std::ostringstream	a;
	std::ostringstream	b;

	a << got;
	b << want;
	check(what, a.str(), b.str());
}

static void	checkBool(const std::string& what, bool got, bool want)
{
	check(what, got ? "true" : "false", want ? "true" : "false");
}

int	main()
{
	/* ---- plain command, no parameter ------------------------------------ */
	{
		Message	m = MessageParser::parse("PING\r\n");

		check("plain.command", m.command, "PING");
		checkSize("plain.argc", m.argc(), 0);
		checkBool("plain.empty", m.empty(), false);
		checkBool("plain.hasTrailing", m.hasTrailing, false);
	}

	/* ---- middles only ---------------------------------------------------- */
	{
		Message	m = MessageParser::parse("MODE #chan +o bob\r\n");

		check("middles.command", m.command, "MODE");
		checkSize("middles.argc", m.argc(), 3);
		check("middles.arg0", m.arg(0), "#chan");
		check("middles.arg1", m.arg(1), "+o");
		check("middles.arg2", m.arg(2), "bob");
		checkBool("middles.hasTrailing", m.hasTrailing, false);
	}

	/* ---- trailing holds spaces, and is also the last param --------------- */
	{
		Message	m = MessageParser::parse("PRIVMSG #42 :hello there world\r\n");

		check("trail.command", m.command, "PRIVMSG");
		checkSize("trail.argc", m.argc(), 2);
		check("trail.arg0", m.arg(0), "#42");
		check("trail.arg1", m.arg(1), "hello there world");
		check("trail.trailing", m.trailing, "hello there world");
		checkBool("trail.hasTrailing", m.hasTrailing, true);
	}

	/* ---- prefix ---------------------------------------------------------- */
	{
		Message	m = MessageParser::parse(":bob!bob@localhost JOIN :#42\r\n");

		check("prefix.prefix", m.prefix, "bob!bob@localhost");
		check("prefix.command", m.command, "JOIN");
		check("prefix.arg0", m.arg(0), "#42");
	}

	/* ---- command is case insensitive ------------------------------------- */
	{
		Message	m = MessageParser::parse("PrIvMsG bob :yo");

		check("case.command", m.command, "PRIVMSG");
		check("case.arg0", m.arg(0), "bob");
	}

	/* ---- LF only, CR only, nothing at all -------------------------------- */
	{
		check("lf.command", MessageParser::parse("NICK bob\n").command, "NICK");
		check("cr.command", MessageParser::parse("NICK bob\r").command, "NICK");
		check("bare.arg0", MessageParser::parse("NICK bob").arg(0), "bob");
	}

	/* ---- degenerate lines are never errors, just empty ------------------- */
	{
		checkBool("empty.blank", MessageParser::parse("").empty(), true);
		checkBool("empty.crlf", MessageParser::parse("\r\n").empty(), true);
		checkBool("empty.spaces", MessageParser::parse("      ").empty(), true);
		checkBool("empty.prefixonly", MessageParser::parse(":bob").empty(), true);
		checkBool("empty.prefixsp", MessageParser::parse(":bob \r\n").empty(), true);
	}

	/* ---- extra spaces between tokens are folded -------------------------- */
	{
		Message	m = MessageParser::parse("   PRIVMSG    #42    :  padded  ");

		check("spaces.command", m.command, "PRIVMSG");
		checkSize("spaces.argc", m.argc(), 2);
		check("spaces.arg0", m.arg(0), "#42");
		check("spaces.trailing", m.trailing, "  padded  ");
	}

	/* ---- empty trailing is a real, present parameter --------------------- */
	{
		Message	m = MessageParser::parse("QUIT :");

		checkSize("emptytrail.argc", m.argc(), 1);
		check("emptytrail.arg0", m.arg(0), "");
		checkBool("emptytrail.hasTrailing", m.hasTrailing, true);
	}

	/* ---- ':' inside a middle stays part of it ---------------------------- */
	{
		Message	m = MessageParser::parse("USER a:b 0 * :real name");

		check("colon.arg0", m.arg(0), "a:b");
		check("colon.trailing", m.trailing, "real name");
	}

	/* ---- 15 parameter cap: the 15th swallows the rest -------------------- */
	{
		Message	m = MessageParser::parse("CMD p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 "
										 "p11 p12 p13 p14 the rest of it");

		checkSize("cap.argc", m.argc(), 15);
		check("cap.arg13", m.arg(13), "p14");
		check("cap.arg14", m.arg(14), "the rest of it");
		checkBool("cap.hasTrailing", m.hasTrailing, true);
	}

	/* ---- out of range argument is a safe empty string -------------------- */
	{
		Message	m = MessageParser::parse("JOIN");

		check("oob.arg0", m.arg(0), "");
		check("oob.arg99", m.arg(99), "");
	}

	std::cout << (g_total - g_failed) << "/" << g_total << " checks passed"
			  << std::endl;
	return (g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
