#include "CommandProcessor.h"
#include "SystemInterface.h"
#include "Logger.h"

#include <signal.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

namespace hs {

CommandProcessor::CommandProcessor(boost::shared_ptr<StreamManager> smPtr) {
	this->sm = smPtr;
	userCooldownTimer.restart();

	//set up all commands
	cmdMap["!deck"] = CCP(new CommandCallback(0, 0, UL_USER, false, [this](const CommandInfo& ci, std::string& response){
		response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.textUrl).str();
	}));
	alias("!deck", "!decklist");

	cmdMap["!deckprogress"] = CCP(new CommandCallback(0, 0, UL_USER, false, [this](const CommandInfo& ci, std::string& response){
		if (!sm->deck.isComplete()) {
			response = "Arena draft progress: " + boost::lexical_cast<std::string>(sm->deck.pickHistory.size()) + "/30";
			if (sm->deck.pickHistory.size() != 0) {
				response += ", latest pick: " + sm->deck.pickHistory.back().name;
			}
		} else {
			response = "Deck complete";
		}
	}));

	cmdMap["!publishdeck"] = CCP(new CommandCallback(0, 0, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.allArgs == "image") {
			auto deckImage = sm->deck.createImageRepresentation();
			sm->currentDeck.imageUrl = SystemInterface::createImgur(deckImage);
			response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.imageUrl).str();
		} else {
			std::string deckString = sm->deck.createTextRepresentation();
			sm->currentDeck.textUrl = SystemInterface::createHastebin(deckString);
			response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.textUrl).str();
		}

	}));

	cmdMap["!setdeck"] = CCP(new CommandCallback(1, -1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		sm->currentDeck.textUrl = ci.allArgs;
		response = (boost::format(CMD_DECK_FORMAT) % sm->sName % sm->currentDeck.textUrl).str();
	}));

	cmdMap["!strawpolling"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_strawpolling = ci.toggleEnable;
		}
		response = "Automated strawpolling is: ";
		response += (sm->param_strawpolling)? "on" : "off";
	}));

	cmdMap["!backupscoring"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_backupscoring = ci.toggleEnable;
		}
		response = "Backup scoring is: ";
		response += (sm->param_backupscoring)? "on" : "off";
	}));

	cmdMap["!drawhandling"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_drawhandling = ci.toggleEnable;
			if (sm->param_drawhandling) {
				sm->currentDraw.state = RECOGNIZER_GAME_DRAW;
			} else {
				sm->currentDraw.state = 0;
			}
		}
		response = "Draw handling is: ";
		response += (sm->param_drawhandling)? "on" : "off";
	}));

	cmdMap["!apicalling"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->param_apicalling = ci.toggleEnable;
		}
		response = "API calling is: ";
		response += (sm->param_apicalling)? "on" : "off";
	}));

	cmdMap["!deckfromdraws"] = CCP(new CommandCallback(0, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.toggle) {
			sm->currentDraw.buildFromDraws = ci.toggleEnable;
		}
		response = "Building decklist from observed draws is: ";
		response += (sm->currentDraw.buildFromDraws)? "on" : "off";
	}));

	cmdMap["!info"] = CCP(new CommandCallback(1, 1, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		if (ci.allArgs != "fortebot") return;
		response = "ForteBot uses OpenCV and perceptual hashing to very quickly compare all card images against the stream image to find a match. "
				"Additionally, SURF feature detection is used for automated scoring. "
				"The Bot is written in C++ by ZeForte. "
				"Check out the (poorly commented) source on GitHub: http://bit.ly/1eGgN5g";
	}));

	cmdMap["!fb_debuglevel"] = CCP(new CommandCallback(1, 1, UL_SUPER, true, [this](const CommandInfo& ci, std::string& response){
		sm->param_debug_level = boost::lexical_cast<unsigned int>(ci.allArgs);
	}));

	cmdMap["!fb_score"] = CCP(new CommandCallback(0, 0, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		response = (boost::format("[SCORE] Current score is: %d - %d") % sm->winsLosses.first % sm->winsLosses.second).str();
	}));

	cmdMap["!fb_cleardeck"] = CCP(new CommandCallback(0, 0, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		sm->deck.clear();
		response = "Deck cleared";
	}));

	cmdMap["!fb_state"] = CCP(new CommandCallback(0, 0, UL_SUPER, false, [this](const CommandInfo& ci, std::string& response){
		response = boost::lexical_cast<std::string>(sm->currentDeck.state);
		response += " ";
		response += boost::lexical_cast<std::string>(sm->currentGame.state);
		response += " ";
		response += boost::lexical_cast<std::string>(sm->currentDraw.state);
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
		if (userCooldownTimer.elapsed() < TIME_BETWEEN_COMMANDS) {
			ci.userlevel = UL_SUBUSER;
		} else {
			userCooldownTimer.restart();
		}
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
