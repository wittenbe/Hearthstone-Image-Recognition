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

//regions of interest for SIFT comparison
const Recognizer::VectorROI Recognizer::GAME_COIN = list_of
		(cv::Rect_<float>(0.6441f, 0.4f, 0.0996f, 0.2292f));

const Recognizer::VectorROI Recognizer::GAME_END = list_of
		(cv::Rect_<float>(0.3771f, 0.5542f, 0.2506f, 0.1188f));

Recognizer::Recognizer() {
	auto cfg = Config::getConfig();
	phashThreshold = cfg.get<int>("config.image_recognition.phash_threshold");
	std::string dataPath = cfg.get<std::string>("config.paths.recognition_data_path");
	std::ifstream dataFile(dataPath);
	boost::property_tree::read_xml(dataFile, data, boost::property_tree::xml_parser::trim_whitespace);
	if (cfg.get<bool>("config.image_recognition.precompute_data")) {
		precomputeData(dataPath);
	}

    populateFromData("hs_data.cards", cards);
    populateFromData("hs_data.heroes", heroes);

    //prepare the SIFT stuff
	detector = SiftFeatureDetectorPtr(new cv::SiftFeatureDetector(cfg.get<int>("config.image_recognition.sift_feature_count")));
	extractor = SiftDescriptorExtractorPtr(new cv::SiftDescriptorExtractor());
	matcher = FlannBasedMatcherPtr(new cv::FlannBasedMatcher());

    cv::Mat tempImg, gsImg;

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_victory.png", CV_LOAD_IMAGE_COLOR);
    DataSetEntry endV("w", PerceptualHash::phash(tempImg));
    cv::cvtColor(tempImg, gsImg, CV_BGR2GRAY);
    descriptorEnd.push_back(std::make_pair(getDescriptor(gsImg), "w"));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_defeat.png", CV_LOAD_IMAGE_COLOR);
    DataSetEntry endD("l", PerceptualHash::phash(tempImg));
    cv::cvtColor(tempImg, gsImg, CV_BGR2GRAY);
    descriptorEnd.push_back(std::make_pair(getDescriptor(gsImg), "l"));
    setEnd.entries.push_back(endV);
    setEnd.entries.push_back(endD);
    setEnd.hashes.push_back(endV.phash);
    setEnd.hashes.push_back(endD.phash);

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_first.png", CV_LOAD_IMAGE_COLOR);
    DataSetEntry coin1("1", PerceptualHash::phash(tempImg));
    cv::cvtColor(tempImg, gsImg, CV_BGR2GRAY);
    descriptorCoin.push_back(std::make_pair(getDescriptor(gsImg), "1"));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_second.png", CV_LOAD_IMAGE_COLOR);
    DataSetEntry coin2("2", PerceptualHash::phash(tempImg));
    cv::cvtColor(tempImg, gsImg, CV_BGR2GRAY);
    descriptorCoin.push_back(std::make_pair(getDescriptor(gsImg), "2"));
    setCoin.entries.push_back(coin1);
    setCoin.entries.push_back(coin2);
    setCoin.hashes.push_back(coin1.phash);
    setCoin.hashes.push_back(coin2.phash);
}

void Recognizer::precomputeData(const std::string& dataPath) {
	const std::string cardImagePath = Config::getConfig().get<std::string>("config.paths.card_image_path") + "/";
	const std::string heroImagePath = Config::getConfig().get<std::string>("config.paths.hero_image_path") + "/";

    for (auto& v : data.get_child("hs_data.cards")) {
    	if (v.first == "entry") {
            std::string id = v.second.get<std::string>("ID");
            cv::Mat image = cv::imread(cardImagePath + id + ".png");
            auto phash = PerceptualHash::phash(image);

            v.second.put("phash", phash);
    	}
    }

    for (auto& v : data.get_child("hs_data.heroes")) {
    	if (v.first == "entry") {
            std::string id = v.second.get<std::string>("ID");
            cv::Mat image = cv::imread(heroImagePath + id + ".png");
            auto phash = PerceptualHash::phash(image);

            v.second.put("phash", phash);
    	}
    }
    boost::property_tree::xml_writer_settings<char> settings('\t', 1);
    write_xml(dataPath, data, std::locale(""), settings);
}

void Recognizer::populateFromData(const std::string& dataPath, DataSet& dataSet) {
    for (auto& v : data.get_child(dataPath)) {
    	if (v.first == "entry") {
            const std::string name = v.second.get<std::string>("name");
            const ulong64 phash = v.second.get<ulong64>("phash");
            DataSetEntry o(name, phash);
            dataSet.entries.push_back(o);
        	dataSet.hashes.push_back(phash);
    	}
    }
}

std::vector<Recognizer::RecognitionResult> Recognizer::recognize(const cv::Mat& image, unsigned int allowedRecognizers) {
	std::vector<RecognitionResult> results;

	if (allowedRecognizers & RECOGNIZER_DRAFT_CLASS_PICK) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CLASS_PICK, DRAFT_CLASS_PICK, heroes);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_PICK) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CARD_PICK, DRAFT_CARD_PICK, cards);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_GAME_CLASS_SHOW) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_GAME_CLASS_SHOW, GAME_CLASS_SHOW, heroes);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_CHOSEN) {
		int index = getIndexOfBluest(image, DRAFT_CARD_CHOSEN);
		if (index >= 0) {
			RecognitionResult rr;
			rr.sourceRecognizer = RECOGNIZER_DRAFT_CARD_CHOSEN;
			rr.results.push_back(boost::lexical_cast<std::string>(index));
			results.push_back(rr);
		}
	}

	//only perform the expensive sift detection if the image succeeded with the phashes
	if (allowedRecognizers & RECOGNIZER_GAME_COIN) {
		RecognitionResult rrPH = comparePHashes(image, RECOGNIZER_GAME_COIN, GAME_COIN, setCoin);
		if (rrPH.valid) {
			RecognitionResult rr = compareSIFT(image, RECOGNIZER_GAME_COIN, GAME_COIN, descriptorCoin);
			if (rr.valid) results.push_back(rr);
		}
	}

	if (allowedRecognizers & RECOGNIZER_GAME_END) {
		RecognitionResult rrPH = comparePHashes(image, RECOGNIZER_GAME_END, GAME_END, setEnd);
		if (rrPH.valid) {
			RecognitionResult rr = compareSIFT(image, RECOGNIZER_GAME_END, GAME_END, descriptorEnd);
			if (rr.valid) results.push_back(rr);
		}
	}

	return results;
}

Recognizer::RecognitionResult Recognizer::comparePHashes(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const DataSet& dataSet) {
	std::vector<std::string> bestMatches = bestPHashMatches(image, roi, dataSet);
	bool valid = true;
	for (auto& bestMatch : bestMatches) {
		valid &= !bestMatch.empty();
	}
	RecognitionResult rr;
	rr.valid = valid;
	if (valid) {
		rr.sourceRecognizer = recognizer;
		rr.results = bestMatches;
	}

	return rr;
}

std::vector<std::string> Recognizer::bestPHashMatches(const cv::Mat& image, const VectorROI& roi, const DataSet& dataSet) {
	std::vector<std::string> results;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
		ulong64 phash = PerceptualHash::phash(roiImage);
		PerceptualHash::ComparisonResult best = PerceptualHash::best(phash, dataSet.hashes);

		if (best.distance < phashThreshold) {
			results.push_back(dataSet.entries[best.index].name);
		} else {
			results.push_back("");
		}
	}

	return results;
}

int Recognizer::getIndexOfBluest(const cv::Mat& image, const VectorROI& roi) {
	std::vector<float> resultsBlue;
	std::vector<float> resultsRed;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));

		  cv::Scalar s = cv::mean(roiImage);
		  resultsBlue.push_back(s[0]);
		  resultsRed.push_back(s[2]);
	}

	//find max and second highest
	int maxIndex = -1;
	float maxVal = 0, secondVal = 0;
	float minRed = 255;
	float maxRed = 0;
	for (size_t i = 0; i < resultsBlue.size(); i++) {
		if (resultsBlue[i] > maxVal) {
			secondVal = maxVal;
			maxIndex = i;
			maxVal = resultsBlue[i];
		} else if (resultsBlue[i] > secondVal) {
			secondVal = resultsBlue[i];
		}

		minRed = std::min(minRed, resultsRed[i]);
		maxRed = std::max(maxRed, resultsRed[i]);
	}
	float redRatioDeviation = fabs(maxRed/minRed - 1);

	// a roi is considered "pretty blue" if the blue channel is at least 60% higher
	//than other roi's blue and if it's past an absolute threshold
	int bluest = (maxVal > (1.6 * secondVal) && maxVal > 180)? maxIndex : -1;
	if (redRatioDeviation >= 0.3) bluest = -1; //to prevent some false positives; real matches have a ratio very close to 1, i.e. the deviation 0

	return bluest;
}

Recognizer::RecognitionResult Recognizer::compareSIFT(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const std::vector<std::pair<cv::Mat, std::string> > descriptors) {
    RecognitionResult rr;
    rr.valid = false;

	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
	    cv::Mat greyscaleImage;
	    cv::cvtColor(roiImage, greyscaleImage, CV_BGR2GRAY);
	    cv::Mat descriptorImage = getDescriptor(greyscaleImage);


	    for (size_t i = 0; i < descriptors.size(); i++) {
	    	if (isSIFTMatch(descriptorImage, descriptors[i].first)) {
	    		rr.valid = true;
	    		rr.sourceRecognizer = recognizer;
	    		rr.results.push_back(descriptors[i].second);
	    		break;
	    	}
	    }
	    if (rr.valid) break;
	}

	return rr;
}

//get descriptor of SIFT features
cv::Mat Recognizer::getDescriptor(cv::Mat& image) {
	std::vector<cv::KeyPoint> keypoints;
	cv::Mat descriptor;
	detector->detect(image, keypoints);
	extractor->compute(image, keypoints, descriptor);
	return descriptor;
}

bool Recognizer::isSIFTMatch(const cv::Mat& descriptorObj, const cv::Mat& descriptorScene) {
	bool match = false;
	std::vector<std::vector<cv::DMatch> > matches;
	std::vector<cv::DMatch> good_matches;
	matcher->knnMatch(descriptorObj, descriptorScene, matches, 2);

	//the 0.4 constant is a measure on how similar the features are, with lower being more strict (but also unlikely to find matches)
	//there's a tradeoff of strictness and the initial amount of features calculated
	for (int i = 0; i < std::min(descriptorScene.rows - 1,(int) matches.size()); i++) {
		if ((matches[i].size() <= 2 && matches[i].size() > 0) && (matches[i][0].distance < 0.4 * (matches[i][1].distance))) {
			good_matches.push_back(matches[i][0]);
		}
	}

	//to identify where the object is in the scene, i.e. what the homography is, we'd need 4 known points to solve for 4 unknowns (2 rotation, 2 translation)
	//that means taht if we HAVE 4 or more points, we can most likely calculate a homography to find the reference image in the stream image
	if (good_matches.size() >= 4) {
		//the actual homography isn't calculated to save time; and its analysis is probably not that simple anyway
		//this also means that it's actually unknown WHERE the object is in the scene, only THAT it probably appears somewhere
		match = true;
	}

	return match;
}

}
