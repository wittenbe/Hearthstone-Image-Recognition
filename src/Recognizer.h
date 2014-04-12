#ifndef RECOGNIZER_H_
#define RECOGNIZER_H_

#include "Database.h"
#include "PerceptualHash.h"
#include "Calibration/Calibration.h"
#include "Calibration/Calibrator.h"

#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/flann/flann.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"

namespace hs {

class Calibrator;

//supported
const unsigned int RECOGNIZER_DRAFT_CLASS_PICK = 1;
const unsigned int RECOGNIZER_DRAFT_CARD_PICK = 4;
const unsigned int RECOGNIZER_DRAFT_CARD_CHOSEN = 8;
const unsigned int RECOGNIZER_GAME_CLASS_SHOW = 16;
const unsigned int RECOGNIZER_GAME_COIN = 32;
const unsigned int RECOGNIZER_GAME_END = 64;
const unsigned int RECOGNIZER_GAME_DRAW = 128;
const unsigned int RECOGNIZER_GAME_DRAW_INIT_1 = 256;
const unsigned int RECOGNIZER_GAME_DRAW_INIT_2 = 512;

//partial support or planned
const unsigned int RECOGNIZER_DRAFT_CLASS_CHOSEN = 2;

const unsigned int RECOGNIZER_ALLOW_ALL = -1;
const unsigned int RECOGNIZER_ALLOW_NONE = 0;

const unsigned int SETTYPE_CARDS = 0;
const unsigned int SETTYPE_HEROES = 1;

const unsigned int RESULT_GAME_END_VICTORY = 0;
const unsigned int RESULT_GAME_END_DEFEAT = 1;
const unsigned int RESULT_GAME_COIN_FIRST = 2;
const unsigned int RESULT_GAME_COIN_SECOND = 3;

//typedef boost::shared_ptr<cv::FlannBasedMatcher> FlannBasedMatcherPtr;
typedef boost::shared_ptr<cv::BFMatcher> BFMatcherPtr;

class Recognizer {
friend class Calibrator;

public:
	typedef std::vector<cv::Rect_<float> > VectorROI;
	typedef std::vector<std::pair<cv::Mat, int> > VectorDescriptor;
	static const VectorROI DRAFT_CLASS_PICK;
	static const VectorROI DRAFT_CARD_PICK;
	static const VectorROI DRAFT_CARD_CHOSEN;
	static const VectorROI GAME_CLASS_SHOW;
	static const VectorROI GAME_COIN;
	static const VectorROI GAME_END;
	static const VectorROI GAME_DRAW;
	static const VectorROI GAME_DRAW_INIT_1;
	static const VectorROI GAME_DRAW_INIT_2;

	struct RecognitionResult {
		bool valid;
		unsigned int sourceRecognizer;
		std::vector<int> results;
	};

	struct DataSetEntry {
		DataSetEntry() : valid(false), id(-1) {}
		DataSetEntry(int id) : valid(true), id(id) {}
		bool valid;
		int id;
	};

	struct DataSet {
		int typeID = -1;
		std::vector<DataSetEntry> entries;
		std::vector<ulong64> hashes; //for quick access
		int phashThreshold;
	};

	Recognizer(DatabasePtr db);
	std::vector<RecognitionResult> recognize(const cv::Mat& image, unsigned int allowedRecognizers);
	RecognitionResult compareFeatures(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const VectorDescriptor& descriptors);
	RecognitionResult comparePHashes(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const DataSet& dataSet);
	std::vector<DataSetEntry> bestPHashMatches(const cv::Mat& image, const VectorROI& roi, const DataSet& dataSet);
	int getIndexOfBluest(const cv::Mat& image, const VectorROI& roi);

	cv::Mat getDescriptor(cv::Mat& image);
	bool isGoodDescriptorMatch(const std::vector<cv::DMatch>& matches);
	std::vector<cv::DMatch> getMatches(const cv::Mat& descriptorObj, const cv::Mat& descriptorScene);
private:
	void precomputeData();

//	boost::property_tree::ptree data;
	DatabasePtr db;
	int phashThreshold;
	DataSet setCards;
	DataSet setClasses;
	std::vector<int> lastDraftRecognition;

	cv::SURF surf;
//	FlannBasedMatcherPtr matcher;
	BFMatcherPtr matcher;
	VectorDescriptor descriptorCoin;
	VectorDescriptor descriptorEnd;
};

typedef boost::shared_ptr<Recognizer> RecognizerPtr;
}

#endif /* RECOGNIZER_H_ */
