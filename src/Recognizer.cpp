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

//regions of interest for SURF comparison
const Recognizer::VectorROI Recognizer::GAME_COIN = list_of
		(cv::Rect_<float>(0.6441f, 0.4f, 0.0996f, 0.2292f));

const Recognizer::VectorROI Recognizer::GAME_END = list_of
		(cv::Rect_<float>(0.3841f, 0.53751f, 0.2471f, 0.148f));

Recognizer::Recognizer() {
	auto cfg = Config::getConfig();
	phashThreshold = cfg.get<int>("config.image_recognition.phash_threshold");
	std::string dataPath = cfg.get<std::string>("config.paths.recognition_data_path");
	std::ifstream dataFile(dataPath);
	boost::property_tree::read_xml(dataFile, data, boost::property_tree::xml_parser::trim_whitespace);
	if (cfg.get<bool>("config.image_recognition.precompute_data")) {
		precomputeData(dataPath);
	}

    populateFromData("hs_data.cards", setCards);
    populateFromData("hs_data.heroes", setClasses);

	surf = cv::SURF(100, 4, 3, true, true);
    HS_INFO << "Using SURF parameters: " << surf.hessianThreshold << " " << surf.nOctaves << " " << surf.nOctaveLayers << " " << surf.extended << " " << surf.upright << std::endl;
//	matcher = FlannBasedMatcherPtr(new cv::FlannBasedMatcher());
	matcher = BFMatcherPtr(new cv::BFMatcher(cv::NORM_L2));

    cv::Mat tempImg;

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_victory.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorEnd.push_back(std::make_pair(getDescriptor(tempImg), "w"));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_end_defeat.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorEnd.push_back(std::make_pair(getDescriptor(tempImg), "l"));

    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_first.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorCoin.push_back(std::make_pair(getDescriptor(tempImg), "1"));
    tempImg = cv::imread(cfg.get<std::string>("config.paths.misc_image_path") + "/" + "game_coin_second.png", CV_LOAD_IMAGE_GRAYSCALE);
    descriptorCoin.push_back(std::make_pair(getDescriptor(tempImg), "2"));

    //set each set's phash threshold
    setCards.phashThreshold = phashThreshold;
    setClasses.phashThreshold = phashThreshold;
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
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CLASS_PICK, DRAFT_CLASS_PICK, setClasses);
		if (rr.valid) results.push_back(rr);
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_PICK) {
		RecognitionResult rr = comparePHashes(image, RECOGNIZER_DRAFT_CARD_PICK, DRAFT_CARD_PICK, setCards);
		if (rr.valid) results.push_back(rr);
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
			rr.results.push_back(boost::lexical_cast<std::string>(index));
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

		if (best.distance < dataSet.phashThreshold) {
			results.push_back(dataSet.entries[best.index].name);
		} else {
			results.push_back("");
		}
	}

	return results;
}

int Recognizer::getIndexOfBluest(const cv::Mat& image, const VectorROI& roi) {
	std::vector<float> resultsBlue;
	std::vector<float> resultsGreen;
	std::vector<float> resultsRed;
	std::vector<std::vector<float> > results;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));

		  cv::Scalar s = cv::mean(roiImage);
		  resultsBlue.push_back(s[0]);
		  resultsGreen.push_back(s[1]);
		  resultsRed.push_back(s[2]);
	}
	results.push_back(resultsBlue);
	results.push_back(resultsGreen);
	results.push_back(resultsRed);

	std::vector<int> maxIndex(3, -1);
	std::vector<int> minIndex(3, -1);
	std::vector<float> maxVal(3, 0.f);
	std::vector<float> minVal(3, 255.f);
	for (size_t chann = 0; chann < maxIndex.size(); chann++) {
		for (size_t i = 0; i < results[chann].size(); i++) {
			if (results[chann][i] > maxVal[chann]) {
				maxIndex[chann] = i;
				maxVal[chann] = results[chann][i];
			}

			if (results[chann][i] < minVal[chann]) {
				minIndex[chann] = i;
				minVal[chann] = results[chann][i];
			}
		}
	}

	int best = -1;
	if (maxIndex[0] == minIndex[2]) {
		float blueRatio = maxVal[0] / minVal[0];
		float greenRatio = maxVal[1] / minVal[1];
		float redRatio = maxVal[2] / minVal[2];
		if (blueRatio >= 1.9 || (blueRatio >= 1.4 && redRatio >= 1.4)) {
			best = maxIndex[0];
		}
	}
//	HS_INFO << "(" << maxVal[0] << " " << minVal[0] << ") (" << maxVal[1] << " " << minVal[1] << ") (" << maxVal[2] << " " << minVal[2] << ") (" << maxIndex[0] << " " << maxIndex[1] << " " << maxIndex[2] << ") (" << minIndex[0] << " " << minIndex[1] << " " << minIndex[2] << ")" << std::endl;
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
//		std::cout << descriptorImage << std::endl;

	    int bestResultMatchesCount = 0;
	    std::string bestResult;

	    if (!descriptorImage.data) continue;

	    std::vector<cv::DMatch> matches;
	    for (size_t i = 0; i < descriptors.size(); i++) {
	    	matches = getMatches(descriptorImage, descriptors[i].first);
	    	if (isGoodDescriptorMatch(matches) && matches.size() > bestResultMatchesCount) {
	    		bestResult = descriptors[i].second;
	    		bestResultMatchesCount = matches.size();
	    	}
	    }
	    if (!bestResult.empty()) {
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
//			HS_INFO << m1.distance << " " << m2.distance << std::endl;
		}
	}

	return goodMatches;
}

}
