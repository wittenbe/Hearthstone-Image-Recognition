#ifndef CALIBRATOR_H_
#define CALIBRATOR_H_

#include "Calibration.h"
#include "../Recognizer.h"
#include <opencv2/opencv.hpp>
#include <boost/shared_ptr.hpp>

namespace hs {

class Recognizer;

class Calibrator {
public:
	static Calibration calibrate(const cv::Mat& image, boost::shared_ptr<Recognizer> r);

private:
	Calibrator() {}
	Calibrator(Calibrator const&);
    void operator=(Calibrator const&);
};

}

#endif /* CALIBRATOR_H_ */
