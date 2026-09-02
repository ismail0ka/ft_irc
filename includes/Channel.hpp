#ifndef CHANNEL_HPP
# define CHANNEL_HPP

#include <string>
#include <set>
#include <vector>

#include "Client.hpp"

enum JoinResult
{
    JOIN_OK = 0,
    JOIN_ALREADY_MEMBER,
    JOIN_CHANNEL_FULL,
    JOIN_INVITE_ONLY,
    JOIN_NOT_INVITED,
    JOIN_BAD_KEY
};

/* What applyModes() actually did, so MODE can announce exactly the changes that
   took effect and nothing else. */
struct ModeApply
{
    std::string                 modes;        // "+it-k" as really applied
    std::vector<std::string>    args;         // the args to echo with them
    std::string                 unknown;      // unrecognised mode chars  -> 472
    std::vector<std::string>    notInChannel; // +o/-o on a non-member    -> 441
};

class Channel
{
    private:
        std::string           _name;
        std::string           _topic;
        std::string           _key;
        size_t                _limit;

        bool                  _inviteOnly;
        bool                  _topicLocked;

        std::set<Client *>    _members;
        std::set<Client *>    _operators;
        std::set<std::string> _invited;

        bool                  _hasKey;
        bool                  _hasLimit;

        Channel(const Channel &other);
        Channel &operator=(const Channel &other);
    
    public:
        Channel(const std::string &name);
        ~Channel();
    
        int                     join(Client &c, const std::string &key);
        void                    part(Client &c, const std::string &reason); //af apply this meth uu need to check isEmpty() 
        void                    kick(Client &op, Client &target, const std::string &reason);
        void                    invite(Client &op, const std::string &nick);
        void                    setTopic(Client &c, const std::string &topic);
    
        void                    applyModes(Client &c, const std::string &str, const std::vector<std::string> &args,
                                ModeApply &out);
        void                    handleMode(bool addingMode, char mode, std::vector<std::string>::const_iterator &it,
                                const std::vector<std::string> &args, ModeApply &out);
        void                    handleModeI(bool addingMode, ModeApply &out);
        void                    handleModeT(bool addingMode, ModeApply &out);
        void                    handleModeK(bool addingMode, std::vector<std::string>::const_iterator &it,
                                const std::vector<std::string> &args, ModeApply &out);
        void                    handleModeO(bool addingMode, std::vector<std::string>::const_iterator &it,
                                const std::vector<std::string> &args, ModeApply &out);
        void                    handleModeL(bool addingMode, std::vector<std::string>::const_iterator &it,
                                const std::vector<std::string> &args, ModeApply &out);

        void                    broadcast(const std::string &msg, Client *except);

        bool                    isOp(Client &c) const;
        bool                    isEmpty() const;

        const std::string       &getName() const;
        const std::string       &getTopic() const;
        bool                    hasTopic() const;

        const std::set<Client *> &getMembers() const;
        bool                    isMember(Client &c) const;
        Client                  *findMember(const std::string &nick);

        void                    addInvite(const std::string &nick);

        bool                    isInviteOnly() const;
        bool                    isTopicLocked() const;

        /* The channel's modes as MODE #chan reports them (324). Only an
           operator ever sees it, so the key is safe to include. */
        std::string             modeString() const;
};

#endif