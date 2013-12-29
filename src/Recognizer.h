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
#define RECOGNIZER_GAME_CLASS_SHOW 16
#define RECOGNIZER_GAME_COIN 32
#define RECOGNIZER_GAME_END 64

//partial support or planned
#define RECOGNIZER_DRAFT_CLASS_CHOSEN 2

#define RECOGNIZER_ALLOW_ALL -1
#define RECOGNIZER_ALLOW_NONE 0

typedef boost::shared_ptr<cv::SiftFeatureDetector> SiftFeatureDetectorPtr;
typedef boost::shared_ptr<cv::SiftDescriptorExtractor> SiftDescriptorExtractorPtr;
typedef boost::shared_ptr<cv::FlannBasedMatcher> FlannBasedMatcherPtr;

class Recognizer {
public:
	typedef std::vector<cv::Rect_<float> > VectorROI;
	static const VectorROI DRAFT_CLASS_PICK;
	static const VectorROI DRAFT_CARD_PICK;
	static const VectorROI DRAFT_CARD_CHOSEN;
	static const VectorROI GAME_CLASS_SHOW;
	static const VectorROI GAME_COIN;
	static const VectorROI GAME_END;

	struct RecognitionResult {
		bool valid;
		unsigned int sourceRecognizer;
		std::vector<std::string> results;
	};

	struct DataSetEntry {
		DataSetEntry();
		DataSetEntry(std::string name, ulong64 phash) : name(name), phash(phash) {}
		std::string name;
		ulong64 phash;
	};

	struct DataSet {
		std::vector<DataSetEntry> entries;
		std::vector<ulong64> hashes; //for quick access
	};

	Recognizer();
	std::vector<RecognitionResult> recognize(const cv::Mat& image, unsigned int allowedRecognizers);
private:
	void precomputeData(const std::string& dataPath);
	void populateFromData(const std::string& dataPath, DataSet& dataSet);

	RecognitionResult compareSIFT(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const std::vector<std::pair<cv::Mat, std::string> > descriptors);
	RecognitionResult comparePHashes(const cv::Mat& image, unsigned int recognizer, const VectorROI& roi, const DataSet& dataSet);
	std::vector<std::string> bestPHashMatches(const cv::Mat& image, const VectorROI& roi, const DataSet& dataSet);
	int getIndexOfBluest(const cv::Mat& image, const VectorROI& roi);

	cv::Mat getDescriptor(cv::Mat& image);
	bool isSIFTMatch(const cv::Mat& descriptorObj, const cv::Mat& descriptorScene);

	boost::property_tree::ptree data;
	int phashThreshold;
	DataSet cards;
	DataSet heroes;
	DataSet setCoin;
	DataSet setEnd;

	//SIFT stuff
	SiftFeatureDetectorPtr detector;
	SiftDescriptorExtractorPtr extractor;
	FlannBasedMatcherPtr matcher;
	std::vector<std::pair<cv::Mat, std::string> > descriptorCoin;
	std::vector<std::pair<cv::Mat, std::string> > descriptorEnd;
};

typedef boost::shared_ptr<Recognizer> RecognizerPtr;
}

#endif /* RECOGNIZER_H_ */
