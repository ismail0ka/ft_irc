/* ************************************************************************** */
/*                                                                            */
/*   replies.hpp                                     SHARED - numeric replies */
/*                                                                            */
/*   Every macro expands to ONE complete line, CRLF included, ready to hand    */
/*   straight to Client::queue(). Nothing here allocates on its own: they are  */
/*   plain std::string expressions.                                            */
/*                                                                            */
/*     c.queue(ERR_NEEDMOREPARAMS(nick, "JOIN"));                              */
/*                                                                            */
/*   `target` is always the receiving client's nick, or "*" when it has not    */
/*   sent NICK yet -- see replyTarget() in irc_utils.hpp.                       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REPLIES_HPP
# define REPLIES_HPP

# include <string>

# define SERVER_NAME    "ircserv"
# define NETWORK_NAME   "42Net"
# define SERVER_VERSION "ircserv-1.0"
# define CRLF           "\r\n"

/* ":ircserv <code> <target> " -- the head every numeric shares. */
# define NUMERIC(code, target) (std::string(":" SERVER_NAME " " code " ") + (target) + " ")

/* ---- registration burst -------------------------------------------------- */

# define RPL_WELCOME(n, full)   (NUMERIC("001", n) + ":Welcome to the " NETWORK_NAME " Network, " + (full) + CRLF)
# define RPL_YOURHOST(n)        (NUMERIC("002", n) + ":Your host is " SERVER_NAME ", running version " SERVER_VERSION CRLF)
# define RPL_CREATED(n)         (NUMERIC("003", n) + ":This server was created at start-up" CRLF)
# define RPL_MYINFO(n)          (NUMERIC("004", n) + SERVER_NAME " " SERVER_VERSION " o itkol" CRLF)
# define RPL_ISUPPORT(n)        (NUMERIC("005", n) + "CHANTYPES=#& CHANMODES=,k,l,it PREFIX=(o)@ NICKLEN=30 :are supported by this server" CRLF)

# define RPL_MOTDSTART(n)       (NUMERIC("375", n) + ":- " SERVER_NAME " Message of the Day -" CRLF)
# define RPL_MOTD(n, text)      (NUMERIC("372", n) + ":- " + (text) + CRLF)
# define RPL_ENDOFMOTD(n)       (NUMERIC("376", n) + ":End of /MOTD command." CRLF)

/* ---- state queries ------------------------------------------------------- */

# define RPL_UMODEIS(n, modes)          (NUMERIC("221", n) + (modes) + CRLF)
# define RPL_CHANNELMODEIS(n, ch, m)    (NUMERIC("324", n) + (ch) + " " + (m) + CRLF)
# define RPL_NOTOPIC(n, ch)             (NUMERIC("331", n) + (ch) + " :No topic is set" CRLF)
# define RPL_TOPIC(n, ch, topic)        (NUMERIC("332", n) + (ch) + " :" + (topic) + CRLF)
# define RPL_INVITING(n, ch, target)    (NUMERIC("341", n) + (target) + " " + (ch) + CRLF)
# define RPL_NAMREPLY(n, ch, names)     (NUMERIC("353", n) + "= " + (ch) + " :" + (names) + CRLF)
# define RPL_ENDOFNAMES(n, ch)          (NUMERIC("366", n) + (ch) + " :End of /NAMES list" CRLF)

/* ---- errors -------------------------------------------------------------- */

# define ERR_NOSUCHNICK(n, t)           (NUMERIC("401", n) + (t) + " :No such nick/channel" CRLF)
# define ERR_NOSUCHCHANNEL(n, ch)       (NUMERIC("403", n) + (ch) + " :No such channel" CRLF)
# define ERR_CANNOTSENDTOCHAN(n, ch)    (NUMERIC("404", n) + (ch) + " :Cannot send to channel" CRLF)
# define ERR_NOORIGIN(n)                (NUMERIC("409", n) + ":No origin specified" CRLF)
# define ERR_NORECIPIENT(n, cmd)        (NUMERIC("411", n) + ":No recipient given (" + (cmd) + ")" CRLF)
# define ERR_NOTEXTTOSEND(n)            (NUMERIC("412", n) + ":No text to send" CRLF)
# define ERR_UNKNOWNCOMMAND(n, cmd)     (NUMERIC("421", n) + (cmd) + " :Unknown command" CRLF)
# define ERR_NONICKNAMEGIVEN(n)         (NUMERIC("431", n) + ":No nickname given" CRLF)
# define ERR_ERRONEUSNICKNAME(n, bad)   (NUMERIC("432", n) + (bad) + " :Erroneous nickname" CRLF)
# define ERR_NICKNAMEINUSE(n, bad)      (NUMERIC("433", n) + (bad) + " :Nickname is already in use" CRLF)
# define ERR_USERNOTINCHANNEL(n, u, ch) (NUMERIC("441", n) + (u) + " " + (ch) + " :They aren't on that channel" CRLF)
# define ERR_NOTONCHANNEL(n, ch)        (NUMERIC("442", n) + (ch) + " :You're not on that channel" CRLF)
# define ERR_USERONCHANNEL(n, u, ch)    (NUMERIC("443", n) + (u) + " " + (ch) + " :is already on channel" CRLF)
# define ERR_NOTREGISTERED(n)           (NUMERIC("451", n) + ":You have not registered" CRLF)
# define ERR_NEEDMOREPARAMS(n, cmd)     (NUMERIC("461", n) + (cmd) + " :Not enough parameters" CRLF)
# define ERR_ALREADYREGISTERED(n)       (NUMERIC("462", n) + ":You may not reregister" CRLF)
# define ERR_PASSWDMISMATCH(n)          (NUMERIC("464", n) + ":Password incorrect" CRLF)
# define ERR_CHANNELISFULL(n, ch)       (NUMERIC("471", n) + (ch) + " :Cannot join channel (+l)" CRLF)
# define ERR_UNKNOWNMODE(n, ch)         (NUMERIC("472", n) + (ch) + " :is unknown mode char to me" CRLF)
# define ERR_INVITEONLYCHAN(n, ch)      (NUMERIC("473", n) + (ch) + " :Cannot join channel (+i)" CRLF)
# define ERR_BADCHANNELKEY(n, ch)       (NUMERIC("475", n) + (ch) + " :Cannot join channel (+k)" CRLF)
# define ERR_CHANOPRIVSNEEDED(n, ch)    (NUMERIC("482", n) + (ch) + " :You're not channel operator" CRLF)
# define ERR_USERSDONTMATCH(n)          (NUMERIC("502", n) + ":Cannot change mode for other users" CRLF)

/* Last thing a dying connection is told. Not a numeric: it carries no target. */
# define ERROR_CLOSING(reason)          (std::string("ERROR :Closing link: ") + (reason) + CRLF)

#endif
