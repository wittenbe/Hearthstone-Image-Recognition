#include "StreamManager.h"
#include "Config.h"
#include "SystemInterface.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/vector.hpp>

#include "opencv2/highgui/highgui.hpp"

using namespace boost::assign;

namespace hs {

#define CMD_DECK_FORMAT "%s's current decklist: %s"

#define DECK_PICK_FORMAT "Pick %02i: (%s, %s, %s)"
#define DECK_PICKED_FORMAT " --> %s picked %s"

#define MSG_CLASS_POLL "Which class should %s pick next?"
#define MSG_CLASS_POLL_VOTE "Vote for %s's next class: %s"
#define MSG_CLASS_POLL_VOTE_REPEAT "relink: %s"

#define MSG_WINS_POLL "How many wins do you think %s will be able to achieve with this deck?"
#define MSG_WINS_POLL_VOTE "How many wins do you think %s will get? "
#define MSG_WINS_POLL_VOTE_REPEAT "relink: %s"

#define MSG_GAME_START "!score -as %s -vs %s -%s"
#define MSG_GAME_END "!score -%s"


StreamManager::StreamManager(StreamPtr stream, clever_bot::botPtr bot) {
	this->stream = stream;
	this->bot = bot;
	param_strawpolling = true;
	param_backupscoring = false;
	param_debug_level = 0;
	currentDeck.clear();

	currentDeck.state = 0;
	currentGame.state = 0;
	enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
	enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
	enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
	enable(currentGame.state, RECOGNIZER_GAME_END);

	sName = Config::getConfig().get<std::string>("config.stream.streamer_name");
	numThreads = Config::getConfig().get<int>("config.image_recognition.threads");
}

void StreamManager::setStream(StreamPtr stream) {
	this->stream = stream;
}

void StreamManager::startAsyn() {
	for (int i = 0; i < numThreads; i++) {
		processingThreads.create_thread(boost::bind(&StreamManager::run, this));
	}
}

void StreamManager::wait() {
	processingThreads.join_all();
}

void StreamManager::run() {
	cv::Mat image;
//	stream->setStream(4);
//	stream->setFramePos(37228);
	RecognizerPtr recognizer = RecognizerPtr(new Recognizer());

	bool running = true;
	while (running) {

		const bool validImage = stream->read(image);
		if (!validImage) break;

		if (param_debug_level & 2) {
			cv::imshow("Debug", image);
//			cv::waitKey(10);
			cv::waitKey();
		}

		boost::timer t;
		std::vector<Recognizer::RecognitionResult> results = recognizer->recognize(image, currentDeck.state | currentGame.state);

		commandMutex.lock();

		for (auto& result : results) {
			if (RECOGNIZER_DRAFT_CLASS_PICK == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CLASS_PICK)) {
				currentDeck.clear();
				disable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				std::cout << "new draft: " << result.results[0] << ", " << result.results[1] << ", " << result.results[2] << std::endl;

				if (param_strawpolling) {
					bot->message("!subon");
					std::string strawpoll = SystemInterface::createStrawpoll((boost::format(MSG_CLASS_POLL) % sName).str(), result.results);
					bot->message((boost::format(MSG_CLASS_POLL_VOTE) % sName % strawpoll).str(), 1);
					bot->repeat_message((boost::format(MSG_CLASS_POLL_VOTE_REPEAT) % strawpoll).str(),
							6, 25, 7);
					bot->message("!suboff", 120);
				}
			}
			else if (RECOGNIZER_DRAFT_CARD_PICK == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CARD_PICK)) {
				bool isNew = currentDeck.cards.size() == 0;
				const size_t last = currentDeck.picks.size() - 1;
				for (size_t i = 0; i < result.results.size() && !isNew; i++) {
					//is there at least one new card in the current recognized pick?
					isNew |= (result.results[i] != currentDeck.picks[last][i]);
				}

				if (isNew) {
					enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
					enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_CHOSEN);
					disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
					currentDeck.picks.push_back(result.results);
					std::cout << "pick " << currentDeck.cards.size() + 1 << ": " + result.results[0] + ", " + result.results[1] + ", " << result.results[2] << std::endl;
				}
			}
			else if (RECOGNIZER_DRAFT_CARD_CHOSEN  == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CARD_CHOSEN)) {
				enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_CHOSEN);

				size_t index = boost::lexical_cast<int>(result.results[0]);
				std::cout << "picked " << currentDeck.picks.back()[index] << std::endl;
				currentDeck.cards.push_back(currentDeck.picks.back()[index]);

				if (currentDeck.cards.size() == 30) {
					disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
					std::string deckString = createDeckString(currentDeck);
					currentDeck.url = SystemInterface::createHastebin(deckString);

					bot->message((boost::format(CMD_DECK_FORMAT) % sName % currentDeck.url).str());
					if (!param_strawpolling) {
//						std::vector<std::string> choices = list_of("9")("8")("7")("6")("4-5")("0-3");
//						std::string strawpoll = SystemInterface::createStrawpoll(MSG_WINS_POLL, choices);
//						bot->message((boost::format(MSG_WINS_POLL_VOTE) % sName % strawpoll).str());
//						bot->repeat_message((boost::format(MSG_WINS_POLL_VOTE_REPEAT) % strawpoll).str(),
//								7, 5, 0);
					}
				}
			}
			else if (RECOGNIZER_GAME_CLASS_SHOW  == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_CLASS_SHOW)) {
				std::cout << "new game: " << result.results[0] << " " << result.results[1] << std::endl;
				enable(currentGame.state, RECOGNIZER_GAME_COIN);
				disable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				currentGame.player = result.results[0];
				currentGame.opponent = result.results[1];

			}
			else if (RECOGNIZER_GAME_COIN == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_COIN)) {
				std::cout << "coin: " << result.results[0] << std::endl;
				enable(currentGame.state, RECOGNIZER_GAME_END);
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_COIN);
				currentGame.fs = result.results[0];
				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_START) % currentGame.player % currentGame.opponent % currentGame.fs).str());
				}
			}
			else if (RECOGNIZER_GAME_END == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_END)) {
				std::cout << "end: " << result.results[0] << std::endl;
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_END);
				currentGame.end = result.results[0];
				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_END) % currentGame.end).str());
				}
			}
		}

		commandMutex.unlock();

		if (param_debug_level & 1) {
			if (stream->isLivestream()) {
				std::cout << "Processed frame in " << t.elapsed() << "s" << std::endl;
			} else {
				std::cout << "Processed frame " << stream->getFramePos() << " in " << t.elapsed() << "s" << std::endl;
			}
		}
	}

	std::cout << "an error while reading a frame occured" << std::endl;
}

std::string StreamManager::createDeckString(Deck deck) {
	std::string deckString = "";
	std::string pickString = "";

	//pick list
	for (size_t i = 0; i < deck.picks.size(); i++) {
		pickString += (boost::format(DECK_PICK_FORMAT) % (i + 1) % deck.picks[i][0] % deck.picks[i][1] % deck.picks[i][2]).str();
		pickString += "\n";
		pickString += (boost::format(DECK_PICKED_FORMAT) % sName % deck.cards[i]).str();
		pickString += "\n";
	}

	std::sort(deck.cards.begin(), deck.cards.end());
	//card list
	for (size_t i = 0; i < deck.cards.size();) {
		int count = 1;
		for (int j = i + 1; j < deck.cards.size() && (deck.cards[i] == deck.cards[j]); j++) {
			count++;
		}
		deckString += boost::lexical_cast<std::string>(count) + "x " + deck.cards[i] + "\n";
		i += count;
	}
	deckString += "\n\n";
	deckString += pickString;

	return deckString;
}

std::string StreamManager::processCommand(std::string user, std::vector<std::string> cmdParams, bool isAllowed, bool isSuperUser) {
	std::string response;
	if (cmdParams.size() == 0) return response;

	commandMutex.lock();
	bool toggle = cmdParams.size() >= 2;
	bool toggleEnable = toggle && (cmdParams[1] == "1" || cmdParams[1] == "on" || cmdParams[1] == "true");
	std::vector<std::string> params;
	for (size_t i = 1; i < cmdParams.size(); i++) {params.push_back(cmdParams[i]);}
	std::string allParams = boost::algorithm::join(params, " ");

	if ("!deck" == cmdParams[0] || "!decklist" == cmdParams[0]) {
		response = (boost::format(CMD_DECK_FORMAT) % sName % currentDeck.url).str();
	}
	else if ("!deckprogress" == cmdParams[0]) {
		if (currentDeck.cards.size() < 30) {
			response = "Arena draft progress: " + boost::lexical_cast<std::string>(currentDeck.cards.size()) + "/30";
			if (currentDeck.cards.size() != 0) {
				response += ", latest pick: " + currentDeck.cards.back();
			}
		} else {
			response = "Deck complete";
		}
	}
	else if ("!deckforcepublish" == cmdParams[0] && isAllowed) {
		while (currentDeck.picks.size() < 30) {
			std::vector<std::string> pick;
			pick.push_back("?"); pick.push_back("?"); pick.push_back("?");
			currentDeck.picks.push_back(pick);
		}
		while (currentDeck.cards.size() < 30) currentDeck.cards.push_back("?");
		std::string deckString = createDeckString(currentDeck);
		currentDeck.url = SystemInterface::createHastebin(deckString);
		response = (boost::format(CMD_DECK_FORMAT) % sName % currentDeck.url).str();
	}
	else if (cmdParams.size() >= 2 &&  isAllowed &&
			"!setdeck" == cmdParams[0]) {
		currentDeck.url = allParams;
	}
	else if ("!backupscoring" == cmdParams[0] && isAllowed) {
		if (toggle) {
			param_backupscoring = toggleEnable;
		}
		response = "Backup scoring is: ";
		response += (param_backupscoring)? "on" : "off";
	}
	else if ("!strawpolling" == cmdParams[0] && isAllowed) {
		if (toggle) {
			param_strawpolling = toggleEnable;
		}
		response = "Automated strawpolling is: ";
		response += (param_strawpolling)? "on" : "off";
	}
	else if ("!fb_debuglevel" == cmdParams[0] && isSuperUser && cmdParams.size() == 2) {
		param_debug_level = boost::lexical_cast<unsigned int>(cmdParams[1]);
	}
	else if (cmdParams.size() >= 2 &&  isAllowed &&
			"!info" == cmdParams[0] && "fortebot" == cmdParams[1]) {
		response = "ForteBot uses OpenCV and perceptual hashing to very quickly compare all card images against the stream image to find a match. "
				"Additionally, SIFT feature detection is used for automated (backup) scoring. "
				"The Bot is written in C++ by ZeForte. "
				"Check out the (poorly commented) source on GitHub: http://bit.ly/1eGgN5g";
	}

	commandMutex.unlock();
	return response;
}

}
