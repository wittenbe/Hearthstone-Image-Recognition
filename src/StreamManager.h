#ifndef STREAMMANAGER_H_
#define STREAMMANAGER_H_

#include "Recognizer.h"
#include "Stream.h"
#include "bot.h"

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include "opencv2/core/core.hpp"

namespace hs {

#define DEFAULT_DECKURL "draft is in progress (if I missed draft, have a mod do !deckforcepublish for a partial draft)"

class StreamManager {
public:
	struct Deck {
		std::string url;
		std::vector<std::string> cards;
		std::vector<std::vector<std::string> > picks;

		bool isValid() {return !url.empty();}
		void clear() {url=DEFAULT_DECKURL; cards.clear(); picks.clear();}

		unsigned int state;
	};

	struct Game {
		std::string player;
		std::string opponent;
		std::string fs;
		std::string end;

		unsigned int state;
	};

	StreamManager(StreamPtr stream, clever_bot::botPtr bot);
	void setStream(StreamPtr stream);
	void startAsyn();
	void wait();
	void run();
	std::string processCommand(std::string user, std::vector<std::string> cmdParams, bool isAllowed, bool isSuperUser);

	std::string getDeckURL();
	void setDeckURL(std::string deckURL);

private:
	std::string createDeckString(Deck deck);
	StreamPtr stream;
	clever_bot::botPtr bot;
	RecognizerPtr recognizer;

	bool param_strawpolling;
	bool param_backupscoring;
	unsigned int param_debug_level;

	int numThreads;
	boost::thread_group processingThreads;

	std::string sName; //streamer name
	Deck currentDeck;
	Game currentGame;

	//make sure the execution of commands doesn't interfere with other vars
    boost::mutex stateMutex;

    void disable(unsigned int& state, unsigned int recognizer) {state &= (~recognizer);}
    void enable(unsigned int& state, unsigned int recognizer) {state |= recognizer;}
};

}


#endif /* STREAMMANAGER_H_ */
