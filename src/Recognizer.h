#ifndef RECOGNIZER_H_
#define RECOGNIZER_H_

#include "PerceptualHash.h"

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

//supported
#define RECOGNIZER_DRAFT_CLASS_PICK 1
#define RECOGNIZER_DRAFT_CARD_PICK 4
#define RECOGNIZER_DRAFT_CARD_CHOSEN 8

//partial support or planned
#define RECOGNIZER_DRAFT_CLASS_CHOSEN 2
#define RECOGNIZER_GAME_CLASS_SHOW 16
#define RECOGNIZER_GAME_COIN 32
#define RECOGNIZER_GAME_END 64

//only the supported are included
#define RECOGNIZER_ALLOW_ALL 13
#define RECOGNIZER_ALLOW_NONE 0

class Recognizer {
public:
	static const std::vector<cv::Rect_<float> > ARENA_CLASS_START;
	static const std::vector<cv::Rect_<float> > ARENA_CARD_PICK;
	static const std::vector<cv::Rect_<float> > ARENA_CARD_BLUE;
	static const std::vector<cv::Rect_<float> > GAME_CLASS_SHOW;

	struct RecognitionResult {
		unsigned int sourceRecognizer;
		std::vector<std::string> results;
	};

	struct DataSetEntry {
		std::string name;
		ulong64 phash;
	};

	struct DataSet {
		std::vector<DataSetEntry> dataPoints;
		std::vector<ulong64> hashes; //quick access
	};

	Recognizer();
	std::vector<RecognitionResult> recognize(const cv::Mat& image, unsigned int allowedRecognizers);
private:
	void precomputeData(const std::string& dataPath);
	void populateFromData(const std::string& dataPath, DataSet& dataSet);
	std::vector<std::string> comparePHashes(const cv::Mat& image, const std::vector<cv::Rect_<float> >& roi, DataSet dataSet);
	int getIndexOfBluest(const cv::Mat& image, const std::vector<cv::Rect_<float> >& roi);

	boost::property_tree::ptree data;
	int phashThreshold;
	DataSet cards;
	DataSet heroes;
	unsigned int gameState; //three states: before class selection, before coin and before end
	bool waitForBlue; //substate of card picking
};

typedef boost::shared_ptr<Recognizer> RecognizerPtr;
}

#endif /* RECOGNIZER_H_ */
