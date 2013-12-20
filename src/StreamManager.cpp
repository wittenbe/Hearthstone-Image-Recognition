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

#define CMD_DECK_FORMAT "Trump's current deck: %s"
#define MSG_CLASS_POLL "Which class should Trump pick next?"
#define MSG_CLASS_POLL_VOTE "Vote for Trump's next class: "
#define MSG_CLASS_POLL_VOTE_REPEAT "relink: "
#define MSG_WINS_POLL "How many wins do you think Trump will be able to achieve with this deck?"
#define MSG_WINS_POLL_VOTE "How many wins do you think Trump will get? "
#define MSG_WINS_POLL_VOTE_REPEAT "relink: %s"

#define DECK_PICK_FORMAT "Pick %02i: (%s, %s, %s)"
#define DECK_PICKED_FORMAT " --> Trump picked %s"

StreamManager::StreamManager(StreamPtr stream, clever_bot::botPtr bot) {
	this->stream = stream;
	this->bot = bot;
	recognizer = RecognizerPtr(new Recognizer());
	allowedRecognizers = RECOGNIZER_ALLOW_NONE;
	param_silent = true;
	param_overthrow_hidbot = false;
	param_debug_level = 0;
	enableRecognizer(RECOGNIZER_DRAFT_CLASS_PICK);
	enableRecognizer(RECOGNIZER_DRAFT_CARD_PICK);
	currentDeck.clear();

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

	bool running = true;
	while (running) {

		const bool validImage = stream->read(image);
		if (!validImage) break;

		if (param_debug_level >= 2) {
			cv::imshow("Debug", image);
			cv::waitKey(10);
		}

		boost::timer t;
		std::vector<Recognizer::RecognitionResult> results = recognizer->recognize(image, allowedRecognizers);

		commandMutex.lock();

		for (auto& result : results) {
			switch (result.sourceRecognizer) {
			case RECOGNIZER_DRAFT_CLASS_PICK: {
				enableRecognizer(RECOGNIZER_DRAFT_CARD_PICK);
				disableRecognizer(RECOGNIZER_DRAFT_CLASS_PICK);
				currentDeck.clear();
				std::cout << "new draft" << std::endl;

				if (!param_silent) {
					bot->message("!subon");
					std::string strawpoll = SystemInterface::createStrawpoll(MSG_CLASS_POLL, result.results);
					bot->message(MSG_CLASS_POLL_VOTE + strawpoll, 1);
					bot->repeat_message(MSG_CLASS_POLL_VOTE_REPEAT + strawpoll, 5, 5, 2);
					bot->message("!suboff", 60);
				}
			}break;

			case RECOGNIZER_DRAFT_CARD_PICK: {
				bool isNew = currentDeck.cards.size() == 0;
				const size_t last = currentDeck.picks.size() - 1;
				for (size_t i = 0; i < result.results.size() && !isNew; i++) {
					//is there at least one new card in the current recognized pick?
					isNew |= (result.results[i] != currentDeck.picks[last][i]);
				}

				if (isNew) {
					disableRecognizer(RECOGNIZER_DRAFT_CARD_PICK);
					enableRecognizer(RECOGNIZER_DRAFT_CARD_CHOSEN);
					enableRecognizer(RECOGNIZER_DRAFT_CLASS_PICK);
					currentDeck.picks.push_back(result.results);
					std::cout << "pick " << currentDeck.cards.size() << ": " + result.results[0] + ", " + result.results[1] + ", " << result.results[2] << std::endl;
				}
			}break;

			case RECOGNIZER_DRAFT_CARD_CHOSEN: {
				enableRecognizer(RECOGNIZER_DRAFT_CARD_PICK);
				disableRecognizer(RECOGNIZER_DRAFT_CARD_CHOSEN);

				size_t index = boost::lexical_cast<int>(result.results[0]);
				std::cout << "picked " << currentDeck.picks.back()[index] << std::endl;
				currentDeck.cards.push_back(currentDeck.picks.back()[index]);

				if (currentDeck.cards.size() == 30) {
					disableRecognizer(RECOGNIZER_DRAFT_CARD_PICK);
					enableRecognizer(RECOGNIZER_DRAFT_CLASS_PICK);
					std::string deckString = createDeckString(currentDeck);
					currentDeck.url = SystemInterface::createHastebin(deckString);

					bot->message((boost::format(CMD_DECK_FORMAT) % currentDeck.url).str());
					if (!param_silent) {
						std::vector<std::string> choices = list_of("9")("8")("7")("6")("4-5")("0-3");
						std::string strawpoll = SystemInterface::createStrawpoll(MSG_WINS_POLL, choices);
						bot->message(MSG_WINS_POLL_VOTE + strawpoll);
						bot->repeat_message((boost::format(MSG_WINS_POLL_VOTE_REPEAT) % strawpoll).str(),
								7, 5, 0);
					}
				}

			}break;
			}
		}

		commandMutex.unlock();

		if (param_debug_level >= 1) {
			std::cout << t.elapsed() << std::endl;
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
		pickString += (boost::format(DECK_PICKED_FORMAT) % deck.cards[i]).str();
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
	bool toggleEnable = cmdParams.size() == 1 || cmdParams[1] == "1";

	if ("!deck" == cmdParams[0]) {
		response = (boost::format(CMD_DECK_FORMAT) % currentDeck.url).str();
	}
	else if ("!deckprogress" == cmdParams[0]) {
		if (currentDeck.picks.size() < 30) {
			response = "Arena draft progress: " + boost::lexical_cast<std::string>(currentDeck.cards.size()) + "/30";
			if (currentDeck.cards.size() != 0) {
				response += ", latest pick: " + currentDeck.cards.back();
			}
		} else {
			response = "Deck complete";
		}
	}
	else if ("!setdeck" == cmdParams[0]
	         && isAllowed && cmdParams.size() >= 2) {
		currentDeck.url = cmdParams[1];
	}
	else if ("!silence" == cmdParams[0]
	         && isAllowed) {
		param_silent = toggleEnable;
	}
	else if("!fb_debug" == cmdParams[0]
	         && isSuperUser) {
		param_debug = toggleEnable;
	}
	else if ("!fb_debuglevel" == cmdParams[0]
	         && isSuperUser && cmdParams.size() == 2) {
		param_debug_level = boost::lexical_cast<unsigned int>(cmdParams[1]);
	}

	commandMutex.unlock();
	return response;
}

}
