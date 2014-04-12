#include "Deck.h"
#include "../Logger.h"
#include <boost/format.hpp>

namespace hs {

Deck::Deck() {
	unknownCard.id = -1;
	unknownCard.name = "?";
	unknownCard.cost = -1;
	unknownCard.heroClass = "?";
	unknownCard.quality = -1;

	cardCount = 0;
	addUnknownCard();
}

std::string Deck::createTextRepresentation() {
	std::string deckString = "";
	std::string pickString = "";

	while (!isComplete()) addUnknownCard();
	if (setHistory.size() != 0) {
		while (pickHistory.size() < 30) addUnknownPick();
		while (setHistory.size() < 30) addUnknownSet();
	}

	//pick list
	for (size_t i = 0; i < setHistory.size(); i++) {
		pickString += (boost::format(DECK_PICK_FORMAT) % (i + 1) % setHistory[i][0].name % setHistory[i][1].name % setHistory[i][2].name).str();
		pickString += "\n";
		pickString += (boost::format(DECK_PICKED_FORMAT) % pickHistory[i].name).str();
		pickString += "\n";
	}

	//card list
	for (auto& e : cards) {
		std::string cost;
		if (e.c.cost == -1) {
			cost = "?";
		} else {
			cost = boost::lexical_cast<std::string>(e.c.cost);
		}

		deckString += boost::lexical_cast<std::string>(e.amount) + "x (" + cost + ")" + e.c.name;
		deckString += "\n";
	}
	deckString += "\n\n";
	deckString += pickString;

	return deckString;
}

void Deck::addCard(const Card& c, int amount) {
	if (c.id != unknownCard.id) removeUnknown(amount); //replace an unknown if possible
	cardCount += amount;
	bool inserted = cards.insert(DeckEntry(c, amount, amount)).second;
	if (!inserted) { //i.e. already exists
		for (auto& card : cards) {
			if (card.c.id == c.id) {
				card.amount += amount;
				card.remaining += amount;
			}
		}
	}
}

void Deck::addSet(const Card& c0, const Card& c1, const Card& c2) {
	std::vector<Card> cards;
	cards.push_back(c0);
	cards.push_back(c1);
	cards.push_back(c2);
	setHistory.push_back(cards);
}

void Deck::addPickedCard(const Card& c) {
	pickHistory.push_back(c);
	addCard(c);
}

void Deck::addUnknownSet() {
	addSet(unknownCard, unknownCard, unknownCard);
}

void Deck::addUnknownCard() {
	addCard(unknownCard);
}

void Deck::addUnknownPick() {
	addPickedCard(unknownCard);
}

bool Deck::draw(const Card& c, bool addIfNotPresent) {
	auto entry = cards.find(DeckEntry(c));
	const bool found = entry != cards.end() && entry->c.id == c.id;

	bool addedCard = true;
	if (found) {
		if (entry->remaining > 0) {
			entry->remaining--;
			addedCard = false;
		} else {
			HS_WARNING << "Draw Request for \"" << c.name << "\", but amount is at 0!" << std::endl;
			if (addIfNotPresent && hasCardSpace()) {
				removeUnknown(1);
				entry->amount++;
				cardCount++;
			}
		}
	} else {
		HS_WARNING << "Draw Request for \"" << c.name << "\", but card not in deck!" << std::endl;
		if (addIfNotPresent && hasCardSpace()) {
			removeUnknown(1);
			cards.insert(DeckEntry(c, 1, 0));
			cardCount++;
		}
	}

	return addedCard;
}

void Deck::resetDraws() {
	for (auto& c : cards) {
		c.remaining = c.amount;
	}
}

void Deck::clear() {
	cards.clear();
	cardCount = 0;
	pickHistory.clear();
	setHistory.clear();
}

bool Deck::isComplete() {
	return cardCount == 30;
}

int Deck::getCardCount() {
	return cardCount;
}

void Deck::removeUnknown(int amount) {
	for (int i = 0; i < amount; i++) {
		for (auto& e : cards) {
			if (e.c.id == unknownCard.id) {
				e.amount--;
				cardCount--;
				if (e.amount <= 0) {
					cards.erase(e);
				}
				break;
			}
		}
	}
}

bool Deck::hasCardSpace() {
	const bool hasUnknown = cards.size() != 0 && cards.begin()->c.id == unknownCard.id;
	return !isComplete() || hasUnknown;
}

}
