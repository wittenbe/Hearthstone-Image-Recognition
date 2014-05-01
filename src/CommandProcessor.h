#ifndef COMMANDPROCESSOR_H_
#define COMMANDPROCESSOR_H_

#include "StreamManager.h"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/unordered_map.hpp>
#include <boost/timer.hpp>
#include <string>

namespace hs {

class StreamManager;

const unsigned int UL_SUBUSER = 0;
const unsigned int UL_USER = 0;
const unsigned int UL_MOD = 0;
const unsigned int UL_SUPER = 0;

const unsigned int COMMAND_COOLDOWN = 5;

const std::string COMMAND_FEEDBACK_TOGGLE = "Parameter \"%s\" is: %s";
const std::string COMMAND_FEEDBACK_STATE = "Deck(%d) Game(%d) Draw(%d) Internal(%d)";

class CommandProcessor {
public:
	struct CommandInfo {
		std::string user;
		unsigned int userlevel;
		std::vector<std::string> args;
		std::string allArgs;
	};

	struct CommandCallback {
		CommandCallback(unsigned int minargs, unsigned int maxargs, unsigned int minuserlevel, bool changesState, boost::function<void(const CommandInfo& ci, std::string& response)> f)
			: minargs(minargs), maxargs(maxargs), minuserlevel(minuserlevel), changesState(changesState), f(f) {};
		unsigned int minargs;
		unsigned int maxargs;
		unsigned int minuserlevel;
		bool changesState;
		boost::function<void(const CommandInfo& ci, std::string& response)> f;
	};
	typedef boost::shared_ptr<CommandCallback> CCP;

	CommandProcessor(boost::shared_ptr<StreamManager> sm);
	std::string process(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser);
private:
	boost::shared_ptr<StreamManager> sm;
	boost::timer userCooldownTimer;
	boost::unordered_map<std::string, boost::shared_ptr<CommandCallback> > cmdMap;
	boost::unordered_map<std::string, unsigned int> toggleMap;
	void alias(std::string original, std::string alias);
	inline bool getToggle(const std::string& s) {
		return (s == "1" || s == "on" || s == "true");
	}
};

typedef boost::shared_ptr<CommandProcessor> CommandProcessorPtr;

}


#endif /* COMMANDPROCESSOR_H_ */
