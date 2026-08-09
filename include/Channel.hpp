/* ************************************************************************** */
/*                                                                            */
/*   Channel.hpp                                          OWNER C - channels  */
/*                                                                            */
/*   Holds raw Client* it does NOT own -- Server owns every Client. A leaving  */
/*   client must therefore be erased from _members, _operators and _invited    */
/*   before it is deleted; Server::partAllChannels() does that on QUIT.        */
/*                                                                            */
/*   Division of labour with the handlers: Channel decides and performs, the   */
/*   handler validates and speaks. Channel never queues a numeric itself, it   */
/*   reports back (JoinResult, ModeApply) and lets the caller phrase it.       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <cstddef>
# include <set>
# include <string>
# include <vector>

class Client;

enum JoinResult
{
	JOIN_OK = 0,
	JOIN_ALREADY_MEMBER,	/* re-JOIN, not an error: stay silent	*/
	JOIN_CHANNEL_FULL,		/* +l reached		-> 471				*/
	JOIN_INVITE_ONLY,		/* +i, no invite	-> 473				*/
	JOIN_NOT_INVITED,		/* same, kept distinct for clarity		*/
	JOIN_BAD_KEY			/* +k mismatch		-> 475				*/
};

/* What applyModes() actually did, so the caller can announce exactly that and
   nothing more. `modes` is empty when the request was a no-op. */
struct ModeApply
{
	std::string					modes;			/* "+it-k"					*/
	std::vector<std::string>	args;			/* in the order they appear	*/
	std::string					unknown;		/* mode chars we do not know */
	std::vector<std::string>	notInChannel;	/* "+o ghost" targets		*/
};

class Channel
{
	public:

		explicit Channel(const std::string& name);
		~Channel();

		/* ---- membership -------------------------------------------------- */

		int		join(Client& c, const std::string& key);

		/* Silent removals: the handler has already broadcast the PART / KICK,
		   because the leaver must receive it too. */
		void	part(Client& c, const std::string& reason);
		void	kick(Client& op, Client& target, const std::string& reason);

		void	addInvite(const std::string& nick);

		void	setTopic(Client& c, const std::string& topic);

		/* Applies what it can and records the outcome in `out`. Assumes the
		   caller already checked that `c` is an operator here. */
		void	applyModes(Client& c, const std::string& str,
						   const std::vector<std::string>& args, ModeApply& out);

		void	broadcast(const std::string& msg, Client* except);

		/* ---- queries ------------------------------------------------------ */

		Client*	findMember(const std::string& nick) const;

		const std::set<Client*>&	getMembers() const;
		const std::string&			getName() const;
		const std::string&			getTopic() const;

		bool	hasTopic() const;
		bool	isEmpty() const;
		bool	isMember(Client& c) const;
		bool	isOp(Client& c) const;
		bool	isInviteOnly() const;
		bool	isTopicLocked() const;
		bool	isInvited(const std::string& nick) const;

		/* "+itl 20" for RPL_CHANNELMODEIS. A bare `MODE #chan` is answerable
		   by anybody, so the key is flagged as set but never spelled out. */
		std::string	modeString() const;

	private:

		typedef std::set<Client*>::const_iterator	MemberCIt;

		std::string				_name;
		std::string				_topic;
		std::string				_key;			/* mode k */
		std::size_t				_limit;			/* mode l */
		bool					_hasKey;
		bool					_hasLimit;
		bool					_inviteOnly;	/* mode i */
		bool					_topicLocked;	/* mode t */
		std::set<Client*>		_members;
		std::set<Client*>		_operators;		/* mode o */
		std::set<std::string>	_invited;		/* ircLower'd nicknames */

		Channel(const Channel& other);
		Channel&	operator=(const Channel& other);
};

#endif
