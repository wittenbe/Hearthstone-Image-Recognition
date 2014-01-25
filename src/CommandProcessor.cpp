#include "CommandProcessor.h"
#include "SystemInterface.h"
#include "Logger.h"

#include <signal.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

namespace hs {

CommandProcessor::CommandProcessor(StreamManager* smPtr) {
	this->sm = smPtr;

	//set up all commands
	cmdMap["!deck"] = CCP(new CommandCallback(0, 0, UL_USER, false, [this](const CommandInfo& ci, std::string& response){
		response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.url).str();
	}));
	alias("!deck", "!decklist");

	cmdMap["!deckprogress"] = CCP(new CommandCallback(0, 0, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		if (sm->currentDeck.cards.size() < 30) {
			response = "Arena draft progress: " + boost::lexical_cast<std::string>(sm->currentDeck.cards.size()) + "/30";
			if (sm->currentDeck.cards.size() != 0) {
				response += ", latest pick: " + sm->currentDeck.cards.back();
			}
		} else {
			response = "Deck complete";
		}
	}));

	cmdMap["!deckforcepublish"] = CCP(new CommandCallback(0, 0, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		while (sm->currentDeck.picks.size() < 30) {
			std::vector<std::string> pick;
			pick.push_back("?"); pick.push_back("?"); pick.push_back("?");
			sm->currentDeck.picks.push_back(pick);
		}
		while (sm->currentDeck.cards.size() < 30) sm->currentDeck.cards.push_back("?");
		std::string deckString = sm->createDeckString(sm->currentDeck);
		sm->currentDeck.url = SystemInterface::createHastebin(deckString);
		response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.url).str();
	}));

	cmdMap["!setdeck"] = CCP(new CommandCallback(1, -1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		sm->currentDeck.url = ci.allArgs;
		response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.url).str();
	}));

	cmdMap["!backupscoring"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_backupscoring = ci.toggleEnable;
			if (sm->param_backupscoring) {
				sm->currentDeck.state = RECOGNIZER_GAME_CLASS_SHOW | RECOGNIZER_GAME_END;
			} else {
				sm->currentDeck.state = 0;
			}
		}
		response = "Backup scoring is: ";
		response += (sm->param_backupscoring)? "on" : "off";
	}));

	cmdMap["!strawpolling"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_strawpolling = ci.toggleEnable;
		}
		response = "Automated strawpolling is: ";
		response += (sm->param_strawpolling)? "on" : "off";
	}));

	cmdMap["!info"] = CCP(new CommandCallback(1, 1, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		if (ci.allArgs != "fortebot") return;
		response = "ForteBot uses OpenCV and perceptual hashing to very quickly compare all card images against the stream image to find a match. "
				"Additionally, SIFT feature detection is used for automated (backup) scoring. "
				"The Bot is written in C++ by ZeForte. "
				"Check out the (poorly commented) source on GitHub: http://bit.ly/1eGgN5g";
	}));

	cmdMap["!fb_debuglevel"] = CCP(new CommandCallback(1, 1, UL_SUPER, true, [this](const CommandInfo& ci, std::string& response){
		sm->param_debug_level = boost::lexical_cast<unsigned int>(ci.allArgs);
	}));

	cmdMap["!fb_quit"] = CCP(new CommandCallback(0, 0, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		raise(SIGINT);
	}));
}

std::string CommandProcessor::process(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser) {
	std::string response;

	//command exists
	std::vector<std::string> cmdParams;
	boost::split(cmdParams, cmd, boost::is_any_of(" "), boost::token_compress_on);
	auto cmdMapEntry = cmdMap.find(cmdParams[0]);
	if (cmdMapEntry == cmdMap.end()) return response;

	//setup parameters
	CommandInfo ci;
	ci.user = user;
	for (size_t i = 1; i < cmdParams.size(); i++) {ci.args.push_back(cmdParams[i]);}
	ci.allArgs = boost::algorithm::join(ci.args, " ");
	ci.toggle = cmdParams.size() >= 2;
	ci.toggleEnable = ci.toggle && (ci.args[0] == "1" || ci.args[0] == "on" || ci.args[0] == "true");

	//get userlevel
	if (isSuperUser) {
		ci.userlevel = UL_SUPER;
	} else if (isMod) {
		ci.userlevel = UL_MOD;
	} else {
		ci.userlevel = UL_USER;
	}

	//process command
	auto command = cmdMapEntry->second;
	if (ci.args.size() >= command->minargs && ci.userlevel >= command->minuserlevel) {
		if (command->changesState) {
			sm->stateMutex.lock();
			command->f(ci, response);
			sm->stateMutex.unlock();
		} else {
			command->f(ci, response);
		}
	}

	return response;
}

void CommandProcessor::alias(std::string original, std::string alias) {
	auto cmd = cmdMap.find(original);
	if (cmd == cmdMap.end()) return;
	cmdMap[alias] = cmd->second;
}

}
