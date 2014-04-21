#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#include <string>
#include <opencv2/opencv.hpp>
#include "../PerceptualHash.h"

namespace hs {

class Calibration {

public:
	Calibration();
	static cv::Rect bruteforceOptimize(const cv::Mat& image, ulong64 phash, cv::Rect heuristic = cv::Rect());
private:


};

}



#endif /* CALIBRATION_H_ */
