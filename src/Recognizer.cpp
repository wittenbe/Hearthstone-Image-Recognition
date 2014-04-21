#include "Recognizer.h"
#include "SystemInterface.h"

#include <stdio.h>
#include <iostream>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/timer.hpp>
#include <boost/progress.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

using namespace boost::assign;

namespace hs {

//to avoid dependency on a specific resolution, these values are ratios of the screen width/height

//regions of interest for phash comparisons
const Recognizer::VectorROI Recognizer::DRAFT_CLASS_PICK = list_of
		(cv::Rect_<float>(0.2366f, 0.2938f, 0.0656f, 0.1751f))
		(cv::Rect_<float>(0.3806f, 0.2938f, 0.0656f, 0.1751f))
		(cv::Rect_<float>(0.5235f, 0.2938f, 0.0656f, 0.1751f));

const Recognizer::VectorROI Recognizer::DRAFT_CARD_PICK = list_of
		(cv::Rect_<float>(0.2448f, 0.2459f, 0.0504f, 0.1230f))
		(cv::Rect_<float>(0.3876f, 0.2459f, 0.0504f, 0.1230f))
		(cv::Rect_<float>(0.5293f, 0.2459f, 0.0504f, 0.1230f));

const Recognizer::VectorROI Recognizer::DRAFT_CARD_CHOSEN = list_of
		(cv::Rect_<float>(0.2448f, 0.4459f, 0.0504f, 0.073f))
		(cv::Rect_<float>(0.3876f, 0.4459f, 0.0504f, 0.073f))
		(cv::Rect_<float>(0.5293f, 0.4459f, 0.0504f, 0.073f));

const Recognizer::VectorROI Recognizer::GAME_CLASS_SHOW = list_of
		(cv::Rect_<float>(0.2799f, 0.5938f, 0.1101f, 0.2938f))
		(cv::Rect_<float>(0.6593f, 0.123f, 0.1101f, 0.2938f));

const Recognizer::VectorROI Recognizer::GAME_DRAW = list_of
		(cv::Rect_<float>(0.6815f, 0.3438f, 0.0633f, 0.1542f));

const Recognizer::VectorROI Recognizer::GAME_DRAW_INIT_1 = list_of
		(cv::Rect_<float>(0.3033f, 0.3521f, 0.0469f, 0.1125f))
		(cv::Rect_<float>(0.4778f, 0.3480f, 0.0469f, 0.1125f))
		(cv::Rect_<float>(0.6523f, 0.3459f, 0.0469f, 0.1125f));

const Recognizer::VectorROI Recognizer::GAME_DRAW_INIT_2 = list_of
		(cv::Rect_<float>(0.2834f, 0.3521f, 0.0469f, 0.1125f))
		(cv::Rect_<float>(0.4134f, 0.3500f, 0.0469f, 0.1125f))
		(cv::Rect_<float>(0.5445f, 0.348f, 0.0469f, 0.1125f))
		(cv::Rect_<float>(0.6748f, 0.3459f, 0.0469f, 0.1125f));

//regions of interest for SURF comparison
const Recognizer::VectorROI Recognizer::GAME_COIN = list_of
		(cv::Rect_<float>(0.6441f, 0.4f, 0.0996f, 0.2292f));

const Recognizer::VectorROI Recognizer::GAME_END = list_of
		(cv::Rect_<float>(0.3841f, 0.53751f, 0.2471f, 0.148f));

Recognizer::Recognizer(DatabasePtr db) {
	this->db = db;
	auto cfg = Config::getConfig();
	phashThreshold = cfg.get<int>("config.image_recognition.phash_threshold");
	if (db->hasMissingData()) {
		HS_INFO << "pHashes missing from database, filling..." << std::endl;
		precomputeData();
	}

	setCards.typeID = SETTYPE_CARDS;
    for (auto& e : db->cards) {
		DataSetEntry o(e.id);
		setCards.entries.push_back(o);
		setCards.hashes.push_back(e.phash);
    }

	setClasses.typeID = SETTYPE_HEROES;
    for (auto& e : db->heroes) {
		DataSetEntry o(e.id);
		setClasses.entries.push_back(o);
		setClasses.hashes.push_back(e.phash);
    }

	surf = cv::SURF(100, 2, 2, true, true);
    HS_INFO << "Using SURF parameters: " << surf.hessianThreshold << " " << surf.nOctaves << " " << surf.nOctaveLayers << " " << surf.extended << " " << surf.upright << std::endl;
	matcher = BFMatcherPtr(new cv::BFMatcher(cv::NORM_L2));

    cv::Mat tempImg;

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_victory.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorEnd.push_back(std::make_pair(getDescriptor(tempImg), RESULT_GAME_END_VICTORY));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_defeat.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorEnd.push_back(std::make_pair(getDescriptor(tempImg), RESULT_GAME_END_DEFEAT));

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_first.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorCoin.push_back(std::make_pair(getDescriptor(tempImg), RESULT_GAME_COIN_FIRST));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_second.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorCoin.push_back(std::make_pair(getDescriptor(tempImg), RESULT_GAME_COIN_SECOND));

    //set each set's phash threshold
    setCards.phashThreshold = phashThreshold;
    setClasses.phashThreshold = phashThreshold;
}

void Recognizer::precomputeData() {
	const std::string cardImagePath = Config::getConfig().get<std::string>("config.paths.card_image_path") + "/";
	const std::string heroImagePath = Config::getConfig().get<std::string>("config.paths.hero_image_path") + "/";

    for (auto& c : db->cards) {
    	std::string stringID = (boost::format("%03d") % c.id).str();
    	cv::Mat image = cv::imread(cardImagePath + stringID + ".png", CV_LOAD_IMAGE_GRAYSCALE);
    	c.phash = PerceptualHash::phash(image);
    }

    for (auto& h : db->heroes) {
    	std::string stringID = (boost::format("%03d") % h.id).str();
    	cv::Mat image = cv::imread(heroImagePath + stringID + ".png", CV_LOAD_IMAGE_GRAYSCALE);
    	h.phash = PerceptualHash::phash(image);
    }

    db->save();
}

std::vector<Recognizer::RecognitionResult> Recognizer::recognize(const cv::Mat& image, unsigned int allowedRecognizers) {
	std::vector<RecognitionResult> results;

	if (allowedRecognizers & RECOGNIZER_DRAFT_CLASS_PICK) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CLASS_PICK, DRAFT_CLASS_PICK, setClasses);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_PICK) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CARD_PICK, DRAFT_CARD_PICK, setCards);
		if (rr.valid) {
			results.push_back(rr);
			lastDraftRecognition = rr.results;
		}
	}

	if (allowedRecognizers & RECOGNIZER_GAME_CLASS_SHOW) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_GAME_CLASS_SHOW, GAME_CLASS_SHOW, setClasses);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_CHOSEN) {
		int index = getIndexOfBluest(image, DRAFT_CARD_CHOSEN);
		if (index >= 0) {
			RecognitionResult rr;
			rr.sourceRecognizer = RECOGNIZER_DRAFT_CARD_CHOSEN;
			rr.results.push_back(index);
			results.push_back(rr);
		}
	}

	if (allowedRecognizers & RECOGNIZER_GAME_COIN) {
		RecognitionResult rr = compareFeatures(image, RECOGNIZER_GAME_COIN, GAME_COIN, descriptorCoin);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_GAME_END) {
		RecognitionResult rr = compareFeatures(image, RECOGNIZER_GAME_END, GAME_END, descriptorEnd);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_GAME_DRAW) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_GAME_DRAW, GAME_DRAW, setCards);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_GAME_DRAW_INIT_1) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_GAME_DRAW_INIT_1, GAME_DRAW_INIT_1, setCards);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_GAME_DRAW_INIT_2) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_GAME_DRAW_INIT_2, GAME_DRAW_INIT_2, setCards);
		if (rr.valid) results.push_back(rr);
	}

	return results;
}

Recognizer::RecognitionResult Recognizer::comparePHashes(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const DataSet& dataSet) {
	std::vector<DataSetEntry> bestMatches = bestPHashMatches(image, roi, dataSet);
	bool valid = true;
	for (auto& bestMatch : bestMatches) {
		valid &= bestMatch.valid;
	}
	RecognitionResult rr;
	rr.valid = valid;
	if (valid) {
		rr.sourceRecognizer = recognizer;
		for (DataSetEntry dse : bestMatches) {
			rr.results.push_back(dse.id);
		}
	}

	return rr;
}

std::vector<Recognizer::DataSetEntry> Recognizer::bestPHashMatches(const cv::Mat& image, const VectorROI& roi, const DataSet& dataSet) {
	std::vector<DataSetEntry> results;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
		ulong64 phash = PerceptualHash::phash(roiImage);
		PerceptualHash::ComparisonResult best = PerceptualHash::best(phash, dataSet.hashes);

		if (best.distance < dataSet.phashThreshold) {
			results.push_back(dataSet.entries[best.index]);
		} else {
			results.push_back(DataSetEntry());
		}
	}

	return results;
}

int Recognizer::getIndexOfBluest(const cv::Mat& image, const VectorROI& roi) {
	if (lastDraftRecognition.empty()) return -1;
	int quality = db->cards[lastDraftRecognition[0]].quality;
	std::vector<float> hV;
	std::vector<float> sV;
	std::vector<float> vV;
	std::vector<std::vector<float> > hsv;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
		cv::Mat hsvImage;
		cv::cvtColor(roiImage, hsvImage, CV_BGR2HSV);

		cv::Scalar means = cv::mean(hsvImage);
		hV.push_back(means[0]);
		sV.push_back(means[1]);
		vV.push_back(means[2]);
	}
	//hsv[0] == h of all slots, hsv[1][2] s value of third slot
	hsv.push_back(hV);
	hsv.push_back(sV);
	hsv.push_back(vV);

	int best = -1;
	int cand = -1;
//	HS_INFO << "(" << hsv[0][0] << "; " << hsv[0][1] << "; " << hsv[0][2] << ") (" << hsv[1][0] << "; " << hsv[1][1] << "; " << hsv[1][2] << ") (" << hsv[2][0] << "; " << hsv[2][1] << "; " << hsv[2][2] << ")" << std::endl;
	float averageValue = 1/3.0f * (hsv[2][0] + hsv[2][1] + hsv[2][2]);

	const bool averageAboveThres =
			((quality == 5) && averageValue >= 200) ||
			((quality != 5) && averageValue >= 220);
	if (!averageAboveThres) return best;

	std::vector<bool> red(3);
	std::vector<bool> candidateColor(3);
	bool match = true;
	size_t minSIndex = 0;
	float minS = hsv[1][0];

	//h value comparison
	for (size_t i = 0; i < hsv[0].size(); i++) {
		if (hsv[1][i] < minS) {
			minS = hsv[1][i];
			minSIndex = i;
		}
		const float h = hsv[0][i];
		const bool candidateEpic = (quality == 4) && 110 <= h && h <= 150;
		const bool candidateOther = (quality != 4) && ((90 <= h && h <= 110) || (50 <= h && h <= 80));
		red[i] = h < 30;
		candidateColor[i] = candidateEpic || candidateOther;
		//this is only true if there was another blue/green/purple candidate before
		if (cand >= 0 && candidateColor[i]) match = false;
		//chose a blue as candidate
		if (candidateColor[i]) cand = (int)i;
		//make sure the others are red
//		match &= (candidateColor[i] || red[i]);
	}
	if (match && minSIndex == cand) best = cand;

	return best;
}

Recognizer::RecognitionResult Recognizer::compareFeatures(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const VectorDescriptor& descriptors) {
    RecognitionResult rr;
    rr.valid = false;

	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
	    cv::Mat greyscaleImage;
	    if (roiImage.channels() == 1) {
	    	greyscaleImage = roiImage.clone();
	    } else {
	    	cv::cvtColor(roiImage, greyscaleImage, CV_BGR2GRAY);
	    }

	    cv::Mat descriptorImage = getDescriptor(greyscaleImage);

	    int bestResultMatchesCount = 0;
	    int bestResult = -1;

	    if (!descriptorImage.data) continue;

	    std::vector<cv::DMatch> matches;
	    for (size_t i = 0; i < descriptors.size(); i++) {
	    	matches = getMatches(descriptorImage, descriptors[i].first);
	    	if (isGoodDescriptorMatch(matches) && matches.size() > bestResultMatchesCount) {
	    		bestResult = descriptors[i].second;
	    		bestResultMatchesCount = matches.size();
	    	}
	    }
	    if (bestResult != -1) {
    		rr.results.push_back(bestResult);
			rr.valid = true;
			rr.sourceRecognizer = recognizer;
	    }
	}

	return rr;
}

cv::Mat Recognizer::getDescriptor(cv::Mat& image) {
	std::vector<cv::KeyPoint> keypoints;
	cv::Mat descriptor;
	surf(image, cv::Mat(), keypoints, descriptor);
	return descriptor;
}

bool Recognizer::isGoodDescriptorMatch(const std::vector<cv::DMatch>& matches) {
	return matches.size() >= 7;
}

std::vector<cv::DMatch> Recognizer::getMatches(const cv::Mat& descriptorObj, const cv::Mat& descriptorScene) {
	std::vector<std::vector<cv::DMatch> > matches;
	matcher->knnMatch(descriptorObj, descriptorScene, matches, 2);
	std::vector<cv::DMatch> goodMatches;

	//ratio test
	for (size_t i = 0; i < matches.size(); ++i) {
		if (matches[i].size() < 2)
				continue;

		const cv::DMatch &m1 = matches[i][0];
		const cv::DMatch &m2 = matches[i][1];

		if (m1.distance <= 0.6f * m2.distance) {
			goodMatches.push_back(m1);
		}
	}

	return goodMatches;
}

}
