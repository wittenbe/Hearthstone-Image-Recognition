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

	toggleMap["strawpolling"] = INTERNAL_STRAWPOLLING;
	toggleMap["scoring"] = INTERNAL_SCORING;
	toggleMap["drawhandling"] = INTERNAL_DRAWHANDLING;
	toggleMap["buildfromdraws"] = INTERNAL_BUILDFROMDRAWS;
	toggleMap["apicalling"] = INTERNAL_APICALLING;

	//set up all commands
	cmdMap["!deck"] = CCP(new CommandCallback(0, 0, UL_USER, false, [this](const CommandInfo& ci, std::string& response){
		response = sm->deckInfo.msg;
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
		if (ci.allArgs == "remaining") {
			auto deckImage = sm->deck.createImageRemainingRepresentation();
			std::string remaining = SystemInterface::createImgur(deckImage);
			response = (boost::format(CMD_REMAINING_FORMAT) % remaining).str();
		} else {
			std::string deckString = sm->deck.createTextRepresentation();
			sm->deckInfo.msg = sm->createDeckURLs();
			response = sm->deckInfo.msg;
		}
	}));

	cmdMap["!setdeck"] = CCP(new CommandCallback(1, -1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		sm->deckInfo.msg = ci.allArgs;
		response = sm->deckInfo.msg;
	}));

	cmdMap["!fb"] = CCP(new CommandCallback(1, 2, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		const unsigned int internal = toggleMap[ci.args[0]];
		if (!internal) return;

		if (ci.args.size() >= 2) {
			if (getToggle(ci.args[1])) {
				sm->enable(sm->internalState, internal);
			} else {
				sm->disable(sm->internalState, internal);
			}
		}
		const bool isOn = sm->internalState & internal;
		response = (boost::format(COMMAND_FEEDBACK_TOGGLE) % ci.args[0] % ((isOn)? "on" : "off")).str();
	}));

	cmdMap["!info"] = CCP(new CommandCallback(1, 1, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		if (ci.allArgs != "fortebot") return;
		response = "ForteBot uses OpenCV and perceptual hashing to very quickly compare all card/class images against the stream image to find a match. "
				"Additionally, SURF feature detection is used for automated victory/defeat detection. "
				"The Bot is written in C++ by ZeForte. "
				"Check out the (poorly commented) source on GitHub: http://bit.ly/1eGgN5g";
	}));

	cmdMap["!fb_debuglevel"] = CCP(new CommandCallback(1, 1, UL_SUPER, true, [this](const CommandInfo& ci, std::string& response){
		sm->param_debug_level = boost::lexical_cast<unsigned int>(ci.allArgs);
	}));

	cmdMap["!fb_score"] = CCP(new CommandCallback(0, 0, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		response = (boost::format("[SCORE] Current score is: %d - %d") % sm->winsLosses.first % sm->winsLosses.second).str();
	}));

	cmdMap["!fb_internaldeck"] = CCP(new CommandCallback(1, 1, UL_MOD, true, [this](const CommandInfo& ci, std::string& response){
		if (ci.allArgs == "clear") {
			sm->deck.clear();
			response = "Deck cleared";
		} else if (ci.allArgs == "get") {
			response = sm->deck.createInternalRepresentation();
		} else if (ci.allArgs == "send") {
			std::vector<std::string> args(2);
			args[0] = sm->deck.heroClass;
			args[1] = sm->deck.createInternalRepresentation();
			SystemInterface::callAPI(sm->api.submitDeckFormat, args);
			response = "Deck sent";
		}
	}));

	cmdMap["!fb_state"] = CCP(new CommandCallback(0, 0, UL_SUPER, false, [this](const CommandInfo& ci, std::string& response){
		response = (boost::format(COMMAND_FEEDBACK_STATE) % sm->deckInfo.state % sm->gameInfo.state % sm->drawInfo.state % sm->internalState).str();
	}));

	cmdMap["!fb_quit"] = CCP(new CommandCallback(0, 0, UL_MOD, false, [this](const CommandInfo& ci, std::string& response){
		raise(SIGINT);
	}));
}

std::string CommandProcessor::process(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser) {
	std::string response;

	//command exists?
	std::vector<std::string> cmdParams;
	boost::split(cmdParams, cmd, boost::is_any_of(" "), boost::token_compress_on);
	auto cmdMapEntry = cmdMap.find(cmdParams[0]);
	if (cmdMapEntry == cmdMap.end()) return response;

	//setup parameters
	CommandInfo ci;
	ci.user = user;
	for (size_t i = 1; i < cmdParams.size(); i++) {ci.args.push_back(cmdParams[i]);}
	ci.allArgs = boost::algorithm::join(ci.args, " ");

	//get userlevel
	if (isSuperUser) {
		ci.userlevel = UL_SUPER;
	} else if (isMod) {
		ci.userlevel = UL_MOD;
	} else {
		ci.userlevel = UL_USER;
		if (userCooldownTimer.elapsed() < COMMAND_COOLDOWN) {
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
