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

const std::vector<cv::Rect_<float> > Recognizer::ARENA_CLASS_START = list_of
		(cv::Rect_<float>(0.2366f, 0.3063f, 0.0656f, 0.1751f))
		(cv::Rect_<float>(0.3806f, 0.3063f, 0.0656f, 0.1751f))
		(cv::Rect_<float>(0.5235f, 0.3063f, 0.0656f, 0.1751f));

const std::vector<cv::Rect_<float> > Recognizer::ARENA_CARD_PICK = list_of
		(cv::Rect_<float>(0.2448f, 0.2459f, 0.0504f, 0.1230f))
		(cv::Rect_<float>(0.3876f, 0.2459f, 0.0504f, 0.1230f))
		(cv::Rect_<float>(0.5293f, 0.2459f, 0.0504f, 0.1230f));

const std::vector<cv::Rect_<float> > Recognizer::ARENA_CARD_BLUE = list_of
		(cv::Rect_<float>(0.2448f, 0.4459f, 0.0504f, 0.073f))
		(cv::Rect_<float>(0.3876f, 0.4459f, 0.0504f, 0.073f))
		(cv::Rect_<float>(0.5293f, 0.4459f, 0.0504f, 0.073f));

const std::vector<cv::Rect_<float> > Recognizer::GAME_CLASS_SHOW = list_of
		(cv::Rect_<float>(0.2799f, 0.5917f, 0.1101f, 0.2938f))
		(cv::Rect_<float>(0.6581f, 0.123f, 0.1101f, 0.2938f));

Recognizer::Recognizer() {
	waitForBlue = false;

	auto cfg = Config::getConfig();
	phashThreshold = cfg.get<int>("config.image_recognition.phash_threshold");
	std::string dataPath = std::string(HSIR_BASE_DIR) + "/" + cfg.get<std::string>("config.paths.recognition_data_path");
	std::ifstream dataFile(dataPath);
	boost::property_tree::read_xml(dataFile, data, boost::property_tree::xml_parser::trim_whitespace);
	if (cfg.get<bool>("config.image_recognition.precompute_data")) {
		precomputeData(dataPath);
	}

    populateFromData("hs_data.cards", cards);
    populateFromData("hs_data.heroes", heroes);
}

void Recognizer::precomputeData(const std::string& dataPath) {
	const std::string cardImagePath = std::string(HSIR_BASE_DIR) + "/" + Config::getConfig().get<std::string>("config.paths.card_image_path") + "/";
	const std::string heroImagePath = std::string(HSIR_BASE_DIR) + "/" + Config::getConfig().get<std::string>("config.paths.hero_image_path") + "/";

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
            DataSetEntry o;
            o.name = name;
            o.phash = phash;
            dataSet.dataPoints.push_back(o);
        	dataSet.hashes.push_back(phash);
    	}
    }
}

std::vector<Recognizer::RecognitionResult> Recognizer::recognize(const cv::Mat& image, unsigned int allowedRecognizers) {
	std::vector<RecognitionResult> results;

	if (allowedRecognizers & RECOGNIZER_DRAFT_CLASS_PICK) {
		std::vector<std::string> bestMatches = comparePHashes(image, ARENA_CLASS_START, heroes);
		bool valid = true;
		for (auto& bestMatch : bestMatches) {
			valid &= !bestMatch.empty();
		}
		if (valid) {
			RecognitionResult rr;
			rr.sourceRecognizer = RECOGNIZER_DRAFT_CLASS_PICK;
			rr.results = bestMatches;
			results.push_back(rr);
		}
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_PICK) {
		std::vector<std::string> bestMatches = comparePHashes(image, ARENA_CARD_PICK, cards);
		bool valid = true;
		for (auto& bestMatch : bestMatches) {
			valid &= !bestMatch.empty();
		}
		if (valid) {
			RecognitionResult rr;
			rr.sourceRecognizer = RECOGNIZER_DRAFT_CARD_PICK;
			rr.results = bestMatches;
			results.push_back(rr);
		}
	}

	if (allowedRecognizers & RECOGNIZER_DRAFT_CARD_CHOSEN) {
		int index = getIndexOfBluest(image, ARENA_CARD_BLUE);
		if (index >= 0) {
			RecognitionResult rr;
			rr.sourceRecognizer = RECOGNIZER_DRAFT_CARD_CHOSEN;
			rr.results.push_back(boost::lexical_cast<std::string>(index));
			results.push_back(rr);
		}
	}

//	    cv::imshow("Output Window", image);
//	    cv::waitKey(15);
//	    cv::waitKey();

	return results;
}

std::vector<std::string> Recognizer::comparePHashes(const cv::Mat& image, const std::vector<cv::Rect_<float> >& roi, DataSet dataSet) {
	std::vector<std::string> results;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));
		ulong64 phash = PerceptualHash::phash(roiImage);
		PerceptualHash::ComparisonResult best = PerceptualHash::best(phash, dataSet.hashes);

		if (best.distance < phashThreshold) {
			results.push_back(dataSet.dataPoints[best.index].name);
		} else {
			results.push_back("");
		}
	}

	return results;
}

int Recognizer::getIndexOfBluest(const cv::Mat& image, const std::vector<cv::Rect_<float> >& roi) {
	std::vector<float> results;
	for (auto& r : roi) {
		cv::Mat roiImage = image(
	    		cv::Range(r.y * image.rows, (r.y + r.height) * image.rows),
	    		cv::Range(r.x * image.cols, (r.x + r.width) * image.cols));

		  cv::Scalar s = cv::mean(roiImage);
		  results.push_back(s[0]);
	}

	//find max and second highest
	int maxIndex = -1;
	float maxVal = 0, secondVal = 0;
	for (size_t i = 0; i < results.size(); i++) {
		if (results[i] > maxVal) {
			secondVal = maxVal;
			maxIndex = i;
			maxVal = results[i];
		} else if (results[i] > secondVal) {
			secondVal = results[i];
		}
	}

	// a roi is considered "pretty blue" if the blue channel is at least 60% higher
	//than other roi's blue and if it's past an absolute threshold
	int bluest = (maxVal > (1.6 * secondVal) && maxVal > 180)? maxIndex : -1;
	return bluest;
}



}
