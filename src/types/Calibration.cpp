#include "Calibration.h"
#include "../Logger.h"
#include <algorithm>
#include <boost/property_tree/xml_parser.hpp>

namespace hs {

Calibration::Calibration(const std::string& calibrationPath) : valid(false) {
	boost::property_tree::ptree c;
    std::ifstream calibFile(calibrationPath.c_str());
    if (calibFile.fail()) {
    	HS_ERROR << "Could not load calibration file at " << calibrationPath << std::endl;
    	return;
    }

    boost::property_tree::read_xml(calibFile, c);
    res = cv::Size(c.get<int>("calibration.resolution.width"), c.get<int>("calibration.resolution.height"));
    roiDraftClassPick  = parseROI(c, "calibration.ROIs.draft_class_pick");
    roiDraftCardPick   = parseROI(c, "calibration.ROIs.draft_card_pick");
    roiDraftCardChosen = parseROI(c, "calibration.ROIs.draft_card_chosen");
    roiGameClassShow   = parseROI(c, "calibration.ROIs.game_class_show");
    roiGameDraw        = parseROI(c, "calibration.ROIs.game_draw");
    roiGameDrawInit1   = parseROI(c, "calibration.ROIs.game_draw_init_1");
    roiGameDrawInit2   = parseROI(c, "calibration.ROIs.game_draw_init_2");
    roiGameCoin        = parseROI(c, "calibration.ROIs.game_coin");
    roiGameEnd         = parseROI(c, "calibration.ROIs.game_end");
    valid = true;
}

cv::Rect Calibration::bruteforceOptimize(const cv::Mat& image, ulong64 phash, const cv::Rect& min, const cv::Rect& max) {
	cv::Mat gs;
	if (image.channels() == 1) {
		gs = image.clone();
	} else {
		cv::cvtColor(image, gs, CV_BGR2GRAY);
	}

	int dist = 0;
	ulong64 roiPHash = 0;

	int bestDistance = 64;
	cv::Rect result;

	for (int w = min.width; w <= max.width; w++) {
		for (int h = min.height; h <= max.height; h++) {
			for (int x = min.x; x <= max.x; x++) {
				for (int y = min.y; y <= max.y; y++) {
					const cv::Mat roi = gs(cv::Range(y, y + h), cv::Range(x, x + w));
					roiPHash = PerceptualHash::phash(roi);
					dist = PerceptualHash::hammingDistance(roiPHash, phash);
					if (dist < bestDistance) {
						bestDistance = dist;
						result = cv::Rect(x, y, w, h);
					}
				}
			}
		}
	}

	return result;
}

Calibration::VectorROI Calibration::parseROI(const boost::property_tree::ptree& ptree, const std::string& ptreePath) {
	VectorROI vROI;
    for (const auto& rois : ptree.get_child(ptreePath)) {
    	const cv::Rect roi(
    			rois.second.get<int>("<xmlattr>.x"),
    			rois.second.get<int>("<xmlattr>.y"),
    			rois.second.get<int>("<xmlattr>.width"),
    			rois.second.get<int>("<xmlattr>.height"));
    	vROI.push_back(roi);
    }
    return vROI;
}

}
