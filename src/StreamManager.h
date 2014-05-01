#ifndef STREAMMANAGER_H_
#define STREAMMANAGER_H_

#include "CommandProcessor.h"
#include "Recognizer.h"
#include "types/Stream.h"
#include "types/Deck.h"
#include "bot.h"
#include "Database.h"

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/atomic.hpp>
#include "opencv2/core/core.hpp"

namespace hs {

const std::string CMD_DECK_FORMAT = "%s's current decklist: %s (draft history: %s )";
const std::string CMD_REMAINING_FORMAT = "Remaining cards in deck: %s";
const std::string DEFAULT_DECKURL = "Draft is in progress (!deckprogress for more info)";

const std::string MSG_CLASS_POLL = "Which class should %s pick next?";
const int MSG_CLASS_POLL_ERROR_RETRY_COUNT = 1;
const std::string MSG_CLASS_POLL_ERROR = "Could not connect to strawpoll, retrying %d more time(s)";
const std::string MSG_CLASS_POLL_ERROR_GIVEUP = "Could not create a strawpoll";
const std::string MSG_CLASS_POLL_VOTE = "Vote for %s's next class: %s";
const std::string MSG_CLASS_POLL_VOTE_REPEAT = "relink: %s";

const std::string MSG_WINS_POLL = "How many wins do you think %s will be able to achieve with this deck?";
const std::string MSG_WINS_POLL_VOTE = "How many wins do you think %s will get? %s";
const std::string MSG_WINS_POLL_VOTE_REPEAT = "relink: %s";

const std::string MSG_GAME_START = "!score -as %s -vs %s -%s";
const std::string MSG_GAME_END = "!score -%s";

const std::string MSG_INITIAL_DRAW = "Initial Draw: %s";
const std::string MSG_DRAW = "Drew \"%s\"";

const std::string STATE_PATH_FORMAT = "state_%s.xml";

const unsigned int PASSED_FRAMES_THRESHOLD = 20;
const unsigned int PASSED_CARD_RECOGNITIONS = 2;

const unsigned int INTERNAL_STRAWPOLLING = 1;
const unsigned int INTERNAL_SCORING = 2;
const unsigned int INTERNAL_DRAWHANDLING = 4;
const unsigned int INTERNAL_APICALLING = 8;
const unsigned int INTERNAL_BUILDFROMDRAWS = 16;
const unsigned int INTERNAL_ENABLE_ALL = 31;

class CommandProcessor;

class StreamManager {
friend class CommandProcessor;

public:
	struct DraftInfo {
		std::string msg;
		void clear() {msg=DEFAULT_DECKURL;}

		unsigned int state;
	};

	struct GameInfo {
		std::string player;
		std::string opponent;
		std::string fs;
		std::string end;

		unsigned int state;
	};

	struct DrawInfo {
		std::vector<int> initialDraw;
		int latestDraw;

		unsigned int state;
	};

	struct APIInfo {
		std::string submitDeckFormat;
		std::string drawCardFormat;
		std::string resetDrawsFormat;
	};

	StreamManager(StreamPtr stream, clever_bot::botPtr bot);
	~StreamManager();
	void loadState();
	void saveState();
	void setStream(StreamPtr stream);
	void startAsyn();
	void wait();
	void run();
	std::string processCommand(const std::string& user, const std::string& cmd, bool isMod, bool isSuperUser);

	std::string createDeckURLs();
	void setDeckURL(std::string deckURL);


private:
	boost::shared_ptr<CommandProcessor> cp;
	StreamPtr stream;
	DatabasePtr db;
	clever_bot::botPtr bot;
	RecognizerPtr recognizer;

	unsigned int internalState;
	unsigned int param_debug_level;

	int numThreads;
	boost::thread_group processingThreads;

	std::string sName; //streamer name
	Deck deck;
	bool shouldUpdateDeck;
	DraftInfo deckInfo;
	GameInfo gameInfo;
	DrawInfo drawInfo;
	APIInfo api;
	boost::atomic<unsigned int> passedFrames;
	std::pair<int, size_t> currentCard;
	std::pair<int, int> winsLosses;

	//make sure the execution of commands doesn't interfere with other vars
    boost::mutex stateMutex;

    void disable(unsigned int& state, unsigned int recognizer) {state &= (~recognizer);}
    void enable(unsigned int& state, unsigned int recognizer) {state |= recognizer;}
};

typedef boost::shared_ptr<StreamManager> StreamManagerPtr;

}


#endif /* STREAMMANAGER_H_ */
