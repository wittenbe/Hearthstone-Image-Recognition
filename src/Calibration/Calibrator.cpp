#include "Calibrator.h"
#include "../Logger.h"
#include "../Config.h"

#include <iostream>

namespace hs {

Calibration Calibrator::calibrate(const cv::Mat& image, boost::shared_ptr<Recognizer> r) {
	Calibration c;
//	cv::Mat gsImg;
//
//	if (image.channels() == 1) {
//		gsImg = image.clone();
//	} else {
//		cv::cvtColor(image, gsImg, CV_BGR2GRAY);
//	}
//
//	cv::Mat descriptor = r->getDescriptor(gsImg);
//	const std::string heroImagePath = Config::getConfig().get<std::string>("config.paths.hero_image_path") + "/";
//	Recognizer::VectorDescriptor classDesc;
//
//    for (auto& v : r->data.get_child("hs_data.heroes")) {
//    	if (v.first == "entry") {
//            const std::string id = v.second.get<std::string>("ID");
//            const std::string name = v.second.get<std::string>("name");
//            cv::Mat image = cv::imread(heroImagePath + id + ".png");
//            if (!image.data) {
//            	HS_ERROR << "Calibration failed; Can't find class images." << std::endl;
//            	return c;
//            }
//
//            classDesc.push_back(std::make_pair(r->getDescriptor(image), name));
//    	}
//    }
//
//    for (auto& d : classDesc) {
//    	HS_INFO << "trying " << d.second << std::endl;
//    	bool match = false;
//    	std::vector<std::vector<cv::DMatch> > matches;
//    	std::vector<cv::DMatch> goodMatches;
//    	r->matcher->knnMatch(d.first, descriptor, matches, 2);
//
//    	for (int i = 0; i < std::min(gsImg.rows - 1,(int) matches.size()); i++) {
//    		if ((matches[i].size() <= 2 && matches[i].size() > 0) && (matches[i][0].distance < 0.6 * (matches[i][1].distance))) {
//    			goodMatches.push_back(matches[i][0]);
//    		}
//    	}
//
//    	if (goodMatches.size() >= 4) {
//    		HS_INFO << "Calibration: found " << std::endl;
//    		match = true;
//    	}
//    }


	return c;
}

}
