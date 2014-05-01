#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#include "../PerceptualHash.h"
#include <string>
#include <opencv2/opencv.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

namespace hs {

class Calibration {

public:
	typedef std::vector<cv::Rect> VectorROI;
	cv::Size res;
	VectorROI roiDraftClassPick;
	VectorROI roiDraftCardPick;
	VectorROI roiDraftCardChosen;
	VectorROI roiGameClassShow;
	VectorROI roiGameCoin;
	VectorROI roiGameEnd;
	VectorROI roiGameDraw;
	VectorROI roiGameDrawInit1;
	VectorROI roiGameDrawInit2;
	bool valid;

	Calibration(const std::string& calibrationName);
	static cv::Rect bruteforceOptimize(const cv::Mat& image, ulong64 phash, const cv::Rect& min, const cv::Rect& max);
private:
	inline VectorROI parseROI(const boost::property_tree::ptree& ptree, const std::string& ptreePath);

};

typedef boost::shared_ptr<Calibration> CalibrationPtr;

}

#endif /* CALIBRATION_H_ */
