/* ************************************************************************** */
/*                                                                            */
/*   test_client.cpp                                    NETWORK LAYER - unit  */
/*                                                                            */
/*   Standalone unit test for Client. Covers the part of the network layer     */
/*   that can be tested without a socket: the input buffer that turns a byte   */
/*   stream into lines, the output buffer that absorbs partial sends, and the  */
/*   registration flags.                                                       */
/*                                                                            */
/*   Nothing here speaks IRC. It never parses a command, never touches a       */
/*   channel: it only checks that bytes in equal lines out.                    */
/*                                                                            */
/*     c++ -Wall -Wextra -Werror -std=c++98 -I. \                              */
/*         Client.cpp tests/test_client.cpp -o test_client && ./test_client    */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "Client.hpp"

static int	g_failed = 0;
static int	g_total = 0;

static std::string	visible(const std::string& s)
{
	std::string	out;

	for (std::size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '\r')
			out += "\\r";
		else if (s[i] == '\n')
			out += "\\n";
		else if (s[i] == '\0')
			out += "\\0";
		else
			out += s[i];
	}
	return out;
}

static void	check(const std::string& what, const std::string& got,
				  const std::string& want)
{
	++g_total;
	if (got == want)
		return ;
	++g_failed;
	std::cout << "FAIL  " << what << "\n      got  [" << visible(got)
			  << "]\n      want [" << visible(want) << "]" << std::endl;
}

static void	checkBool(const std::string& what, bool got, bool want)
{
	++g_total;
	if (got == want)
		return ;
	++g_failed;
	std::cout << "FAIL  " << what << "\n      got  " << (got ? "true" : "false")
			  << ", want " << (want ? "true" : "false") << std::endl;
}

static void	checkSize(const std::string& what, std::size_t got, std::size_t want)
{
	std::ostringstream	a;
	std::ostringstream	b;

	a << got;
	b << want;
	check(what, a.str(), b.str());
}

/* Feeding a std::string through the raw (const char*, size_t) interface the
   server uses, so embedded NULs are exercised too. */
static bool	feed(Client& c, const std::string& data)
{
	return c.appendIn(data.data(), data.size());
}

/* -------------------------------------------------------------------------- */
/*   the input buffer: a byte stream becomes lines                             */
/* -------------------------------------------------------------------------- */

static void	testOneCompleteLine(void)
{
	Client		c(-1);
	std::string	line;

	feed(c, "NICK alice\r\n");
	checkBool("one line: takeLine succeeds", c.takeLine(line), true);
	check("one line: CRLF stripped", line, "NICK alice");
	checkBool("one line: buffer now empty", c.takeLine(line), false);
}

static void	testBareLineFeed(void)
{
	Client		c(-1);
	std::string	line;

	/* nc without -C sends bare LF; it has to work as well as CRLF. */
	feed(c, "NICK bob\n");
	checkBool("bare LF: takeLine succeeds", c.takeLine(line), true);
	check("bare LF: line intact", line, "NICK bob");
}

static void	testEmptyLine(void)
{
	Client		c(-1);
	std::string	line;

	/* A blank line is a real line: the dispatcher ignores it, the buffer
	   must still consume it rather than stalling. */
	feed(c, "\r\n");
	checkBool("empty line: takeLine succeeds", c.takeLine(line), true);
	check("empty line: is empty", line, "");
	checkBool("empty line: buffer drained", c.takeLine(line), false);
}

static void	testSplitAcrossReads(void)
{
	Client		c(-1);
	std::string	line;

	/* The subject's own test: one command delivered in three packets. */
	feed(c, "JO");
	checkBool("split: incomplete after 'JO'", c.takeLine(line), false);
	feed(c, "IN #a");
	checkBool("split: incomplete after 'IN #a'", c.takeLine(line), false);
	feed(c, "\r\n");
	checkBool("split: complete after CRLF", c.takeLine(line), true);
	check("split: reassembled", line, "JOIN #a");
}

static void	testSplitInsideCrlf(void)
{
	Client		c(-1);
	std::string	line;

	/* The nastiest split of all: the CR and the LF arrive separately. */
	feed(c, "PING x\r");
	checkBool("CRLF split: incomplete on CR alone", c.takeLine(line), false);
	feed(c, "\n");
	checkBool("CRLF split: complete on LF", c.takeLine(line), true);
	check("CRLF split: CR still stripped", line, "PING x");
}

static void	testSeveralLinesInOnePacket(void)
{
	Client		c(-1);
	std::string	line;

	feed(c, "PASS x\r\nNICK y\r\nUSER y 0 * :Y\r\n");
	checkBool("batch: first", c.takeLine(line), true);
	check("batch: first line", line, "PASS x");
	checkBool("batch: second", c.takeLine(line), true);
	check("batch: second line", line, "NICK y");
	checkBool("batch: third", c.takeLine(line), true);
	check("batch: third line", line, "USER y 0 * :Y");
	checkBool("batch: drained", c.takeLine(line), false);
}

static void	testTrailingPartialAfterCompleteLine(void)
{
	Client		c(-1);
	std::string	line;

	/* One whole command plus the start of the next, in a single packet. */
	feed(c, "NICK a\r\nNIC");
	checkBool("partial tail: complete line first", c.takeLine(line), true);
	check("partial tail: line", line, "NICK a");
	checkBool("partial tail: remainder held back", c.takeLine(line), false);
	feed(c, "K b\r\n");
	checkBool("partial tail: completes later", c.takeLine(line), true);
	check("partial tail: second line", line, "NICK b");
}

static void	testEmbeddedNul(void)
{
	Client		c(-1);
	std::string	line;
	std::string	payload("PRIVMSG a :x", 12);

	/* appendIn takes an explicit length, so a NUL in the middle of a packet
	   must not truncate the line. */
	payload += '\0';
	payload += "y\r\n";
	feed(c, payload);
	checkBool("NUL: line extracted", c.takeLine(line), true);
	checkSize("NUL: full length kept", line.size(), 14);
}

static void	testLongLineGuard(void)
{
	Client		c(-1);
	Client		d(-1);
	std::string	flood(600, 'A');
	std::string	line;

	/* Over 512 bytes with no newline in sight: the peer is filling our
	   memory, so appendIn refuses and the server disconnects it. */
	checkBool("guard: oversize without newline rejected", feed(c, flood), false);

	/* The same volume WITH newlines is legitimate traffic and must pass. */
	std::string	legit;
	for (int i = 0; i < 60; ++i)
		legit += "PRIVMSG #c :0123456789\r\n";
	checkBool("guard: oversize with newlines accepted", feed(d, legit), true);

	int	got = 0;
	while (d.takeLine(line))
		++got;
	checkSize("guard: every line survived", static_cast<std::size_t>(got), 60);
}

static void	testExactlyAtLimit(void)
{
	Client		c(-1);
	std::string	line;
	std::string	max(510, 'B');

	/* 510 payload + CRLF is the largest legal IRC line: it must pass. */
	checkBool("limit: 512-byte line accepted", feed(c, max + "\r\n"), true);
	checkBool("limit: and is extractable", c.takeLine(line), true);
	checkSize("limit: length preserved", line.size(), 510);
}

/* -------------------------------------------------------------------------- */
/*   the output buffer: absorbing partial sends                                */
/* -------------------------------------------------------------------------- */

static void	testQueueAndDrain(void)
{
	Client	c(-1);

	checkBool("out: empty at birth", c.hasOut(), false);
	c.queue("001 hello\r\n");
	checkBool("out: has data after queue", c.hasOut(), true);
	check("out: content", c.outData(), "001 hello\r\n");

	c.queue("002 world\r\n");
	check("out: appends, never replaces", c.outData(), "001 hello\r\n002 world\r\n");

	c.consumeOut(c.outData().size());
	checkBool("out: drained", c.hasOut(), false);
}

static void	testPartialConsume(void)
{
	Client	c(-1);

	/* send() may accept fewer bytes than offered; only what actually left
	   is erased, and the remainder waits for the next POLLOUT. */
	c.queue("ABCDEFGHIJ");
	c.consumeOut(4);
	checkBool("partial: still has data", c.hasOut(), true);
	check("partial: only the sent bytes erased", c.outData(), "EFGHIJ");
	c.consumeOut(6);
	checkBool("partial: fully drained", c.hasOut(), false);
}

static void	testConsumeZero(void)
{
	Client	c(-1);

	/* A send() that wrote nothing must leave the buffer untouched. */
	c.queue("XYZ");
	c.consumeOut(0);
	check("consume 0: unchanged", c.outData(), "XYZ");
}

/* -------------------------------------------------------------------------- */
/*   registration flags and the quit flag                                      */
/* -------------------------------------------------------------------------- */

static void	testRegistrationFlags(void)
{
	Client	c(-1);

	checkBool("reg: nothing set at birth", c.isRegistered(), false);
	c.setPassOk(true);
	checkBool("reg: PASS alone is not enough", c.isRegistered(), false);
	c.setNickSet(true);
	checkBool("reg: PASS+NICK is not enough", c.isRegistered(), false);
	c.setUserSet(true);
	checkBool("reg: PASS+NICK+USER completes it", c.isRegistered(), true);

	checkBool("reg: welcomed is separate", c.isWelcomed(), false);
	c.setWelcomed(true);
	checkBool("reg: welcomed settable", c.isWelcomed(), true);
}

static void	testQuitFlag(void)
{
	Client	c(-1);

	/* The flag the poll loop reads to know a client may be deleted. */
	checkBool("quit: false at birth", c.isQuit(), false);
	c.setQuit(true);
	checkBool("quit: settable", c.isQuit(), true);
}

static void	testIdentity(void)
{
	Client	c(-1);

	check("identity: nick empty at birth", c.getNick(), "");
	c.setNick("alice");
	c.setUser("al");
	c.setRealname("Alice L");
	c.setHost("127.0.0.1");
	check("identity: nick", c.getNick(), "alice");
	check("identity: user", c.getUser(), "al");
	check("identity: realname", c.getRealname(), "Alice L");
	check("identity: host", c.getHost(), "127.0.0.1");
	checkBool("identity: fd stored", c.getFd() == -1, true);
}

int	main(void)
{
	std::cout << "--- Client: input buffer (stream -> lines) ---" << std::endl;
	testOneCompleteLine();
	testBareLineFeed();
	testEmptyLine();
	testSplitAcrossReads();
	testSplitInsideCrlf();
	testSeveralLinesInOnePacket();
	testTrailingPartialAfterCompleteLine();
	testEmbeddedNul();
	testLongLineGuard();
	testExactlyAtLimit();

	std::cout << "--- Client: output buffer (partial sends) ---" << std::endl;
	testQueueAndDrain();
	testPartialConsume();
	testConsumeZero();

	std::cout << "--- Client: state flags ---" << std::endl;
	testRegistrationFlags();
	testQuitFlag();
	testIdentity();

	std::cout << std::endl;
	if (g_failed == 0)
		std::cout << "OK   " << g_total << " checks passed" << std::endl;
	else
		std::cout << "FAIL " << g_failed << " of " << g_total
				  << " checks failed" << std::endl;
	return g_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
