#include "Deck.h"
#include "../Logger.h"
#include "../Config.h"
#include <boost/format.hpp>
#include <algorithm>
#include <boost/algorithm/string.hpp>

namespace hs {

Deck::Deck() {
	unknownCard.id = -1;
	unknownCard.name = "?";
	unknownCard.cost = -1;
	unknownCard.heroClass = "?";
	unknownCard.quality = -1;

	clear();
	imagePath = Config::getConfig().get<std::string>("config.paths.decklist_image_path");
}

std::string Deck::createInternalRepresentation() {
	std::stringstream ss;

	for (const auto& e : cards) {
		if (e.c.id == unknownCard.id) continue;

		ss << (boost::format("%03d") % e.c.id).str() << "-";
		ss << boost::lexical_cast<std::string>(e.amount) << "-";
		ss << "0;";
	}
	std::string result(ss.str());
	if (result.length() != 0) {
		result = result.substr(0, result.length() - 1); //remove last ';'
	}

	return result;
}

void Deck::fillFromInternalRepresentation(DatabasePtr db, std::string s) {
	if (s.empty()) return;

	clear();
	std::vector<std::string> cardsInfo;
	boost::split(cardsInfo, s, boost::is_any_of(";"), boost::token_compress_on);
	for (const auto& cardInfo : cardsInfo) {
		std::vector<std::string> cardInfoElems;
		boost::split(cardInfoElems, cardInfo, boost::is_any_of("-"), boost::token_compress_on);
		if (cardInfoElems.size() != 3) {
			HS_ERROR << "Reading deck: At least one card is not a triple. Aborting. (input: " << s << ")" << std::endl;
			clear();
			return;
		}
		int id = boost::lexical_cast<int>(cardInfoElems[0]);
		int amount = boost::lexical_cast<int>(cardInfoElems[1]);
		bool golden = boost::lexical_cast<bool>(cardInfoElems[2]);
		const Card& card = db->cards[id];
		addCard(card, amount, amount);
	}
}

std::string Deck::createTextRepresentation() {
	std::string deckString = "";
	std::string pickString = "";

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
	for (const auto& e : cards) {
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

cv::Mat Deck::createImageRepresentation() {
	cv::Mat unknown = cv::imread(imagePath + "/unknown.png", CV_LOAD_IMAGE_COLOR);
	cv::Mat result(unknown.rows * cards.size(), unknown.cols, unknown.type());
	cv::Rect roiRect(0, 0, unknown.cols, unknown.rows);

	size_t i = 0;
	for (const auto& e : cards) {
		bool drawAmount = true;
		roiRect.y = i++ * unknown.rows;
		cv::Mat cardImage = unknown;
		cv::Mat roi = result(roiRect);
		if (e.c.id != unknownCard.id) {
			if (e.amount == 1) {
				cardImage = cv::imread(imagePath + "/1/" + (boost::format("%03d") % e.c.id).str() + ".png", CV_LOAD_IMAGE_COLOR);
				drawAmount = false;
			} else {
				cardImage = cv::imread(imagePath + "/n/" + (boost::format("%03d") % e.c.id).str() + ".png", CV_LOAD_IMAGE_COLOR);
			}
		}

		if (drawAmount) {
			std::string amount = boost::lexical_cast<std::string>(std::min(e.amount, 9));
			cv::Mat amountImage = cv::imread(imagePath + "/amount/" + amount + ".png", CV_LOAD_IMAGE_UNCHANGED);
			cv::Mat cardImageRoi = cardImage(cv::Rect(274, 12, amountImage.cols, amountImage.rows)); //lu of amount image at 274,12; center at 284,25
			overlayImage(cardImageRoi, amountImage);
		}
		cardImage.copyTo(roi);
	}
	return result;
}

cv::Mat Deck::createImageRemainingRepresentation() {
	cv::Mat unknown = cv::imread(imagePath + "/unknown.png", CV_LOAD_IMAGE_COLOR);
	cv::Mat result(unknown.rows * cards.size(), unknown.cols, unknown.type());
	cv::Rect roiRect(0, 0, unknown.cols, unknown.rows);

	size_t i = 0;
	for (const auto& e : cards) {
		roiRect.y = i++ * unknown.rows;
		cv::Mat cardImage = unknown;
		cv::Mat roi = result(roiRect);
		if (e.c.id != unknownCard.id) {
			cardImage = cv::imread(imagePath + "/n/" + (boost::format("%03d") % e.c.id).str() + ".png", CV_LOAD_IMAGE_COLOR);
		}
		std::string amount = boost::lexical_cast<std::string>(std::min(e.remaining, 9));
		cv::Mat amountImage = cv::imread(imagePath + "/amount/" + amount + ".png", CV_LOAD_IMAGE_UNCHANGED);
		cv::Mat cardImageRoi = cardImage(cv::Rect(274, 12, amountImage.cols, amountImage.rows));
		overlayImage(cardImageRoi, amountImage);
		cardImage.copyTo(roi);
	}
	return result;
}

void Deck::addCard(const Card& c, int amount, int remaining) {
	if (!hasCardSpace()) return;
	removeUnknown(amount); //replace an unknown if possible
	cardCount += amount;
	bool inserted = cards.insert(DeckEntry(c, amount, remaining)).second;
	if (!inserted) { //i.e. already exists
		for (auto& card : cards) {
			if (card.c.id == c.id) {
				card.amount += amount;
				card.remaining += remaining;
			}
		}
	}
	if (c.heroClass != "None") heroClass = c.heroClass;
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
//			HS_WARNING << "Draw Request for \"" << c.name << "\", but amount is at 0!" << std::endl;
			if (addIfNotPresent && hasCardSpace()) {
				addCard(c);
			}
		}
	} else {
//		HS_WARNING << "Draw Request for \"" << c.name << "\", but card not in deck!" << std::endl;
		if (addIfNotPresent && hasCardSpace()) {
			addCard(c, 1, 0);
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
	heroClass = "Mage";
	cards.insert(DeckEntry(unknownCard, 30, 30));
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
				if (e.amount <= 0) {
					cards.erase(e);
				}
				break;
			}
		}
	}
}

bool Deck::hasCardSpace() {
	return !isComplete() || hasUnknown();
}

void Deck::overlayImage(cv::Mat &background, const cv::Mat &foreground) {
	for (int y = 0; y < background.rows; ++y) {
		for (int x = 0; x < background.cols; ++x) {
			double opacity = ((double)foreground.data[y * foreground.step + x * foreground.channels() + 3]) / 255.;
			for(int c = 0; opacity > 0 && c < background.channels(); ++c) {
				unsigned char foregroundPx = foreground.data[y * foreground.step + x * foreground.channels() + c];
				unsigned char backgroundPx = background.data[y * background.step + x * background.channels() + c];
				background.data[y*background.step + background.channels()*x + c] = backgroundPx * (1.-opacity) + foregroundPx * opacity;
			}
		}
	}
}

}
