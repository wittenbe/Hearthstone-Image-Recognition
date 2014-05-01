#ifndef DECK_H_
#define DECK_H_

#include "../Database.h"
#include <set>
#include <cv.h>
#include "opencv2/highgui/highgui.hpp"

namespace hs {

const std::string DECK_PICK_FORMAT = "Pick %02i: (%s, %s, %s)";
const std::string DECK_PICKED_FORMAT = " --> picked %s";

class Deck {
public:

	struct DeckEntry {
		DeckEntry(Card c) : c(c), amount(0), remaining(0) {}
		DeckEntry(Card c, int amount, int remaining) : c(c), amount(amount), remaining(remaining) {}
		Card c;
		mutable int amount;
		mutable int remaining;
	};

	struct DeckSorterLess {
	    bool operator()(const DeckEntry& c1, const DeckEntry& c2) const {
	        if (c1.c.cost == c2.c.cost) {
	        	if (c1.c.type == c2.c.type) {
	        		return c1.c.name < c2.c.name;
	        	}
	        	return c1.c.type > c2.c.type;
	        }
	        return c1.c.cost < c2.c.cost;
	    }
	};

	std::set<DeckEntry, DeckSorterLess> cards;
	std::vector<std::vector<Card> > setHistory;
	std::vector<Card> pickHistory;
	std::string heroClass;

	Deck();
	std::string createInternalRepresentation();
	void fillFromInternalRepresentation(DatabasePtr db, std::string s);
	std::string createTextRepresentation();
	cv::Mat createImageRepresentation();
	cv::Mat createImageRemainingRepresentation();
	void addCard(const Card& c, int amount = 1, int remaining = 1);
	void addSet(const Card& c0, const Card& c1, const Card& c2);
	void addPickedCard(const Card& c);
	void addUnknownSet();
	void addUnknownCard();
	void addUnknownPick();
	bool draw(const Card& c, bool addIfNotPresent = true);
	void resetDraws();
	void clear();
	bool isComplete();
	int getCardCount();
	bool hasUnknown() {return cards.size() != 0 && cards.begin()->c.id == unknownCard.id;}

private:
	std::string imagePath;
	Card unknownCard;
	int cardCount;
	void removeUnknown(int amount = 1);
	bool hasCardSpace();
	void overlayImage(cv::Mat &background, const cv::Mat &foreground);
};

}

#endif /* DECK_H_ */
