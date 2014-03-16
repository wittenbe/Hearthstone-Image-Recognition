#include "StreamManager.h"
#include "Config.h"
#include "SystemInterface.h"
#include "Logger.h"

#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "opencv2/highgui/highgui.hpp"

using namespace boost::assign;

namespace hs {

StreamManager::StreamManager(StreamPtr stream, clever_bot::botPtr bot) {
	this->stream = stream;
	this->bot = bot;
	cp = CommandProcessorPtr(new CommandProcessor(this));
	param_strawpolling = true;
	param_backupscoring = true;
	currentDeck.clear();
	param_debug_level = 0;

	recognizer = RecognizerPtr(new Recognizer());

	currentDeck.state = 0;
	currentGame.state = 0;
	currentGame.wins = 0;
	currentGame.losses = 0;
	enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
	enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);

	if (param_backupscoring) {
		enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
		enable(currentGame.state, RECOGNIZER_GAME_END);
	}

	loadState();

	sName = Config::getConfig().get<std::string>("config.stream.streamer_name");
	numThreads = Config::getConfig().get<int>("config.image_recognition.threads");

	if (numThreads <= 0) {
		numThreads = boost::thread::hardware_concurrency();
		if (numThreads <= 0) {
			HS_ERROR << "Unknown amount of cores, setting to 1" << std::endl;
			numThreads = 1;
		}
	}
}

StreamManager::~StreamManager() {
	cp.reset();
}

void StreamManager::loadState() {
	boost::property_tree::ptree state;
    std::ifstream stateFile(std::string(STATE_PATH).c_str());
    if (stateFile.fail()) {
    	HS_INFO << "no state to load, using default values" << std::endl;
    } else {
        boost::property_tree::read_xml(stateFile, state);
        //using default values in case I want to save more later; so this won't break if someone uses an old state format
        currentDeck.url = state.get<decltype(currentDeck.url)>("state.deckURL", currentDeck.url);
        currentDeck.state = state.get<decltype(currentDeck.state)>("state.deckState", currentDeck.state);
        currentGame.state = state.get<decltype(currentGame.state)>("state.gameState", currentGame.state);
        currentGame.wins = state.get<decltype(currentGame.wins)>("state.currentWins", currentGame.wins);
        currentGame.losses = state.get<decltype(currentGame.losses)>("state.currentLosses", currentGame.losses);
        param_backupscoring = state.get<decltype(param_backupscoring)>("state.backupscoring", param_backupscoring);
        param_strawpolling = state.get<decltype(param_strawpolling)>("state.strawpolling", param_strawpolling);
        HS_INFO << "state loaded" << std::endl;
    }
}

void StreamManager::saveState() {
	HS_INFO << "attempting to save state" << std::endl;
	std::ifstream stateFile(STATE_PATH);
	boost::property_tree::ptree state;
//	boost::property_tree::read_xml(stateFile, state, boost::property_tree::xml_parser::trim_whitespace);

    state.put("state.deckURL", currentDeck.url);
//    state.put("state.deckState", currentDeck.state);
//    state.put("state.gameState", currentGame.state);
    state.put("state.backupscoring", param_backupscoring);
    state.put("state.strawpolling", param_strawpolling);
    state.put("state.currentWins", currentGame.wins);
    state.put("state.currentLosses", currentGame.losses);

    boost::property_tree::xml_writer_settings<char> settings('\t', 1);
    write_xml(STATE_PATH, state, std::locale(""), settings);
    HS_INFO << "state saved" << std::endl;
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
//	stream->setStream(1);
//	stream->setFramePos(45364);
//	stream->setFramePos(42364);
//	stream->setFramePos(36927);

	HS_INFO << "Started thread" << std::endl;

	bool running = true;
	while (running) {

		const bool validImage = stream->read(image);
		if (!validImage) break;

		if (param_debug_level & 2) {
			cv::imshow("Debug", image);
//			cv::waitKey(10);
			cv::waitKey();
		}

		auto startTime = boost::posix_time::microsec_clock::local_time();

		std::vector<Recognizer::RecognitionResult> results = recognizer->recognize(image, currentDeck.state | currentGame.state);

		if (param_debug_level & 1) {
			auto endTime = boost::posix_time::microsec_clock::local_time();
			boost::posix_time::time_duration diff = endTime - startTime;
			const auto elapsed = diff.total_milliseconds();
			if (stream->isLivestream()) {
				HS_INFO << "Processed frame in " << elapsed << "ms" << std::endl;
			} else {
				HS_INFO << "Processed frame " << stream->getFramePos() << " in " << elapsed << "ms" << std::endl;
			}
		}

		if (results.empty()) continue;

		stateMutex.lock();
		for (auto& result : results) {
			if (RECOGNIZER_DRAFT_CLASS_PICK == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CLASS_PICK)) {
				currentDeck.clear();
				disable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				HS_INFO << "new draft: " << result.results[0] << ", " << result.results[1] << ", " << result.results[2] << std::endl;
				bot->message("!score -arena");
				currentGame.losses = 0;
				currentGame.wins = 0;

				if (param_strawpolling) {
					bot->message("!subon");
					bool success = false;
					for (int i = 0; i <= MSG_CLASS_POLL_ERROR_RETRY_COUNT && !success; i++) {
						std::string strawpoll = SystemInterface::createStrawpoll((boost::format(MSG_CLASS_POLL) % sName).str(), result.results);
						if (strawpoll.empty()) {
							bot->message((boost::format(MSG_CLASS_POLL_ERROR) % (MSG_CLASS_POLL_ERROR_RETRY_COUNT - i)).str());
						} else {
							bot->message((boost::format(MSG_CLASS_POLL_VOTE) % sName % strawpoll).str());
							bot->repeat_message((boost::format(MSG_CLASS_POLL_VOTE_REPEAT) % strawpoll).str(),
									5, 25, 7);
							bot->message("!suboff", 120);
							success = true;
						}
					}

					if (!success) {
						bot->message(MSG_CLASS_POLL_ERROR_GIVEUP);
					}

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
					HS_INFO << "pick " << currentDeck.cards.size() + 1 << ": " + result.results[0] + ", " + result.results[1] + ", " << result.results[2] << std::endl;
				}
			}
			else if (RECOGNIZER_DRAFT_CARD_CHOSEN  == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CARD_CHOSEN)) {
				enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_CHOSEN);

				size_t index = boost::lexical_cast<int>(result.results[0]);
				HS_INFO << "picked " << currentDeck.picks.back()[index] << std::endl;
				currentDeck.cards.push_back(currentDeck.picks.back()[index]);

				if (currentDeck.cards.size() == 30) {
					disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
					std::string deckString = createDeckString(currentDeck);
					currentDeck.url = SystemInterface::createHastebin(deckString);

					bot->message((boost::format(CMD_DECK_FORMAT) % sName % currentDeck.url).str());
					if (param_strawpolling) {
//						std::vector<std::string> choices = list_of("9")("8")("7")("6")("4-5")("0-3");
//						std::string strawpoll = SystemInterface::createStrawpoll(MSG_WINS_POLL, choices);
//						bot->message((boost::format(MSG_WINS_POLL_VOTE) % sName % strawpoll).str());
//						bot->repeat_message((boost::format(MSG_WINS_POLL_VOTE_REPEAT) % strawpoll).str(),
//								5, 25, 7);
					}
				}
			}
			else if (RECOGNIZER_GAME_CLASS_SHOW  == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_CLASS_SHOW)) {
				enable(currentGame.state, RECOGNIZER_GAME_COIN);
				disable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				currentGame.player = result.results[0];
				currentGame.opponent = result.results[1];
			}
			else if (RECOGNIZER_GAME_COIN == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_COIN)) {
				enable(currentGame.state, RECOGNIZER_GAME_END);
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_COIN);
				currentGame.fs = result.results[0];
				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_START) % currentGame.player % currentGame.opponent % currentGame.fs).str());
				}
				if (param_debug_level & 4) {
					const std::string& time = boost::lexical_cast<std::string>(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
					std::string name = "coin" + currentGame.fs + time + ".png";
					SystemInterface::saveImage(image, name);
				}
			}
			else if (RECOGNIZER_GAME_END == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_END)) {
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_END);
				currentGame.end = result.results[0];
				if (currentGame.end == "w") currentGame.wins++;
				else currentGame.losses++;

				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_END) % currentGame.end).str());
				}

				if (currentGame.wins == 12 || currentGame.losses == 3) {
//					bot->message("!score -constructed", 10);
				}
				if (param_debug_level & 4) {
					const std::string& time = boost::lexical_cast<std::string>(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
					std::string name = currentGame.end + time + ".png";
					SystemInterface::saveImage(image, name);
				}
			}
		}
		stateMutex.unlock();
	}

	HS_ERROR << "an error while reading a frame occured" << std::endl;
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

std::string StreamManager::processCommand(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser) {
	if (cmd.empty() || cmd.find("!") != 0) return std::string("");
	return cp->process(user, cmd, isMod, isSuperUser);
}

}
