#include "StreamManager.h"
#include "Config.h"
#include "SystemInterface.h"
#include "Logger.h"

#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
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
	db = DatabasePtr(new Database(Config::getConfig().get<std::string>("config.paths.recognition_data_path")));
	param_strawpolling = true;
	param_backupscoring = true;
	param_drawhandling = true;
	param_debug_level = 0;
	passedFrames = PASSED_FRAMES_THRESHOLD;
	currentCard.second = -1;
	currentCard.second = 0;
	winsLosses = std::make_pair<int, int>(0,0);

	recognizer = RecognizerPtr(new Recognizer(db));

	currentDeck.clear();
	currentDeck.state = 0;
	currentGame.state = 0;
	currentDraw.state = 0;
	currentDraw.buildFromDraws = true;
	enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
	enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);

	enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
	enable(currentGame.state, RECOGNIZER_GAME_END);

	if (param_drawhandling) {
		enable(currentDraw.state, RECOGNIZER_GAME_DRAW);
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
	HS_INFO << "Using " << numThreads << " threads" << std::endl;
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
        currentDeck.textUrl = state.get<decltype(currentDeck.textUrl)>("state.deckURL", currentDeck.textUrl);
        currentDeck.state = state.get<decltype(currentDeck.state)>("state.deckState", currentDeck.state);
        currentGame.state = state.get<decltype(currentGame.state)>("state.gameState", currentGame.state);
        winsLosses.first = state.get<decltype(winsLosses.first)>("state.currentWins", winsLosses.first);
        winsLosses.second = state.get<decltype(winsLosses.second)>("state.currentLosses", winsLosses.second);
        param_backupscoring = state.get<decltype(param_backupscoring)>("state.backupscoring", param_backupscoring);
        param_strawpolling = state.get<decltype(param_strawpolling)>("state.strawpolling", param_strawpolling);
        param_drawhandling = state.get<decltype(param_drawhandling)>("state.drawhandling", param_drawhandling);
        currentDraw.buildFromDraws = state.get<decltype(currentDraw.buildFromDraws)>("state.buildFromDraws", currentDraw.buildFromDraws);
        HS_INFO << "state loaded" << std::endl;
    }
}

void StreamManager::saveState() {
	HS_INFO << "attempting to save state" << std::endl;
	std::ifstream stateFile(STATE_PATH);
	boost::property_tree::ptree state;
//	boost::property_tree::read_xml(stateFile, state, boost::property_tree::xml_parser::trim_whitespace);

    state.put("state.deckURL", currentDeck.textUrl);
//    state.put("state.deckState", currentDeck.state);
//    state.put("state.gameState", currentGame.state);
    state.put("state.backupscoring", param_backupscoring);
    state.put("state.strawpolling", param_strawpolling);
    state.put("state.drawhandling", param_strawpolling);
    state.put("state.currentWins", winsLosses.first);
    state.put("state.currentLosses", winsLosses.second);
    state.put("state.buildFromDraws", currentDraw.buildFromDraws);

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
//	stream->setStreamIndex(3);
//	stream->setFramePos(29830);
//	stream->setFramePos(10000);

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

		std::vector<Recognizer::RecognitionResult> results = recognizer->recognize(image, currentDeck.state | currentGame.state | currentDraw.state);

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

		passedFrames++;
		if (results.empty()) continue;

		stateMutex.lock();
		for (auto& result : results) {
			if (RECOGNIZER_DRAFT_CLASS_PICK == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CLASS_PICK)) {
				deck.clear();
				currentDeck.clear();
				disable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				HS_INFO << "new draft: " << db->heroes[result.results[0]].name << ", " << db->heroes[result.results[1]].name << ", " << db->heroes[result.results[2]].name << std::endl;
				bot->message("!score -arena");
				winsLosses = std::make_pair<int, int>(0,0);

				if (param_strawpolling) {
					bot->message("!subon");
					bool success = false;
					std::vector<std::string> classNames;
					for (auto& r : result.results) {
						classNames.push_back(db->heroes[r].name);
					}
					for (int i = 0; i <= MSG_CLASS_POLL_ERROR_RETRY_COUNT && !success; i++) {
						std::string strawpoll = SystemInterface::createStrawpoll((boost::format(MSG_CLASS_POLL) % sName).str(), classNames);
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
				const size_t last = deck.setHistory.size() - 1;
				bool isNew = deck.pickHistory.size() == 0 && last == -1;
				for (size_t i = 0; i < result.results.size() && !isNew; i++) {
					//is there at least one new card in the current recognized pick?
					isNew |= (result.results[i] != deck.setHistory[last][i].id);
				}

				if (isNew) {
					deck.addSet(db->cards[result.results[0]], db->cards[result.results[1]], db->cards[result.results[2]]);
					if ((currentDeck.state & RECOGNIZER_DRAFT_CARD_CHOSEN) && deck.setHistory.size() == deck.pickHistory.size() + 2) {
						deck.addUnknownPick();
						HS_WARNING << "Missed pick " << deck.cards.size() << std::endl;
					}
					enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
					enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_CHOSEN);
					HS_INFO << "pick " << deck.getCardCount() + 1 << ": " + db->cards[result.results[0]].name + ", " + db->cards[result.results[1]].name + ", " << db->cards[result.results[2]].name << std::endl;
				}
			}
			else if (RECOGNIZER_DRAFT_CARD_CHOSEN  == result.sourceRecognizer && (currentDeck.state & RECOGNIZER_DRAFT_CARD_CHOSEN)) {
				enable(currentDeck.state, RECOGNIZER_DRAFT_CLASS_PICK);
				enable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
				disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_CHOSEN);

				Card c = deck.setHistory.back()[result.results[0]];
				HS_INFO << "picked " << c.name << std::endl;
				deck.addPickedCard(c);

				if (deck.isComplete()) {
					disable(currentDeck.state, RECOGNIZER_DRAFT_CARD_PICK);
					std::string deckString = deck.createTextRepresentation();
					currentDeck.textUrl = SystemInterface::createHastebin(deckString);

					bot->message((boost::format(CMD_DECK_FORMAT) % sName % currentDeck.textUrl).str());
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
				currentGame.player = db->heroes[result.results[0]].name;
				currentGame.opponent = db->heroes[result.results[1]].name;
			}
			else if (RECOGNIZER_GAME_COIN == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_COIN)) {
				enable(currentGame.state, RECOGNIZER_GAME_END);
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_COIN);
				currentGame.fs = (result.results[0] == RESULT_GAME_COIN_FIRST)? "1" : "2";
				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_START) % currentGame.player % currentGame.opponent % currentGame.fs).str());
				}
				if (param_debug_level & 4) {
					const std::string& time = boost::lexical_cast<std::string>(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
					std::string name = "coin" + currentGame.fs + time + ".png";
					SystemInterface::saveImage(image, name);
				}
				if (currentGame.fs == "1" && param_drawhandling) {
					enable(currentDraw.state, RECOGNIZER_GAME_DRAW_INIT_1);
				} else if (currentGame.fs == "2" && param_drawhandling) {
					enable(currentDraw.state, RECOGNIZER_GAME_DRAW_INIT_2);
				}
				currentDraw.latestDraw = -1;
				deck.resetDraws();
			}
			else if (RECOGNIZER_GAME_END == result.sourceRecognizer && (currentGame.state & RECOGNIZER_GAME_END)) {
				enable(currentGame.state, RECOGNIZER_GAME_CLASS_SHOW);
				disable(currentGame.state, RECOGNIZER_GAME_END);
				currentGame.end = (result.results[0] == RESULT_GAME_END_VICTORY)? "w" : "l";
				if (currentGame.end == "w") winsLosses.first++;
				else winsLosses.second++;

				if (param_backupscoring) {
					bot->message((boost::format(MSG_GAME_END) % currentGame.end).str());
				}

				if (winsLosses.first == 12 || winsLosses.second == 3) {
//					bot->message("!score -constructed", 10);
				}
				if (param_debug_level & 4) {
					const std::string& time = boost::lexical_cast<std::string>(boost::posix_time::microsec_clock::local_time().time_of_day().total_milliseconds());
					std::string name = currentGame.end + time + ".png";
					SystemInterface::saveImage(image, name);
				}
			}
			else if (RECOGNIZER_GAME_DRAW_INIT_1 == result.sourceRecognizer && (currentDraw.state & RECOGNIZER_GAME_DRAW_INIT_1)) {
				currentDraw.initialDraw = result.results;
			}
			else if (RECOGNIZER_GAME_DRAW_INIT_2 == result.sourceRecognizer && (currentDraw.state & RECOGNIZER_GAME_DRAW_INIT_2)) {
				currentDraw.initialDraw = result.results;
			}
			else if (RECOGNIZER_GAME_DRAW == result.sourceRecognizer && (currentDraw.state & RECOGNIZER_GAME_DRAW) && passedFrames.load() >= PASSED_FRAMES_THRESHOLD) {
				bool pass = result.results[0] == currentCard.first && ++currentCard.second >= PASSED_CARD_RECOGNITIONS;
				if (result.results[0] != currentCard.first) currentCard.second = 0;
				currentCard.first = result.results[0];
				if (pass) {
					passedFrames = 0;
					currentCard.second = 0;
					currentCard.first = -1;
					if (currentDraw.latestDraw == -1) {
						std::vector<std::string> initDrawNames;
						for (int id : currentDraw.initialDraw) initDrawNames.push_back(db->cards[id].name);
						std::string initDraw = boost::algorithm::join(initDrawNames, "; ");
//						bot->message((boost::format(MSG_INITIAL_DRAW) % initDraw).str());
						for (auto& d : currentDraw.initialDraw) {
							deck.draw(db->cards[d], currentDraw.buildFromDraws);
						}
						currentDraw.initialDraw.clear();
						disable(currentDraw.state, RECOGNIZER_GAME_DRAW_INIT_1);
						disable(currentDraw.state, RECOGNIZER_GAME_DRAW_INIT_2);
					}
//					bot->message((boost::format(MSG_DRAW) % db->cards[result.results[0]].name).str());
					deck.draw(db->cards[result.results[0]], currentDraw.buildFromDraws);
					currentDraw.latestDraw = result.results[0];
				}
			}
		}
		stateMutex.unlock();
	}

	HS_ERROR << "an error while reading a frame occured" << std::endl;
}

std::string StreamManager::processCommand(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser) {
	if (cmd.empty() || cmd.find("!") != 0) return std::string("");
	return cp->process(user, cmd, isMod, isSuperUser);
}

}
