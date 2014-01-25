#ifndef COMMANDPROCESSOR_H_
#define COMMANDPROCESSOR_H_

#include "StreamManager.h"
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/unordered_map.hpp>
#include <string>

namespace hs {

class StreamManager;

#define UL_USER 0
#define UL_MOD 1
#define UL_SUPER 2

class CommandProcessor {
public:
	struct CommandInfo {
		std::string user;
		unsigned int userlevel;
		bool toggle;
		bool toggleEnable;
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


	CommandProcessor(StreamManager* sm);
	std::string process(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser);
private:
	StreamManager* sm;
	boost::unordered_map<std::string, boost::shared_ptr<CommandCallback> > cmdMap;
	void alias(std::string original, std::string alias);
};

typedef boost::shared_ptr<CommandProcessor> CommandProcessorPtr;

}


#endif /* COMMANDPROCESSOR_H_ */
