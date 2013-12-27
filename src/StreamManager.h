#ifndef STREAMMANAGER_H_
#define STREAMMANAGER_H_

#include "Recognizer.h"
#include "Stream.h"
#include "bot.h"

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include "opencv2/core/core.hpp"

namespace hs {

#define DEFAULT_DECKURL "unknown (either draft is in progress or I missed it)"

class StreamManager {
public:
	struct Deck {
		std::string url;
		std::vector<std::string> cards;
		std::vector<std::vector<std::string> > picks;

		bool isValid() {return !url.empty();}
		void clear() {url=DEFAULT_DECKURL; cards.clear(); picks.clear();}
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
	unsigned int allowedRecognizers;

	bool param_silent;
	bool param_backupscoring;
	unsigned int param_debug_level;

	int numThreads;
	boost::thread_group processingThreads;

	Deck currentDeck;

	//make sure the execution of commands doesn't interfere with other vars
    boost::mutex commandMutex;

    void disable(unsigned int recognizer) {allowedRecognizers &= (~recognizer);}
    void enable(unsigned int recognizer) {allowedRecognizers |= recognizer;}
    void enableAllBut(unsigned int recognizers) {allowedRecognizers = ~recognizers;}
    void disableAllBut(unsigned int recognizers) {allowedRecognizers = recognizers;}
};

}


#endif /* STREAMMANAGER_H_ */
