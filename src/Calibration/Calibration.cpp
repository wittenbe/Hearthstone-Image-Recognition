#include "Calibration.h"
#include <algorithm>

namespace hs {

Calibration::Calibration() {

}

cv::Rect Calibration::bruteforceOptimize(const cv::Mat& image, ulong64 phash, cv::Rect heuristic) {
	cv::Mat gs;
	if (image.channels() == 1) {
		gs = image.clone();
	} else {
		cv::cvtColor(image, gs, CV_BGR2GRAY);
	}

	const int maxWidth = std::min(heuristic.width, image.cols);
	const int maxHeight = std::min(heuristic.height, image.rows);

	int dist = 0;
	ulong64 roiPHash = 0;

	int bestDistance = 64;
	cv::Rect result;

	for (int w = 51; w <= 55; w++) {
		for (int h = 72; h <= 76; h++) {
			for (int x = 580; x <= 584; x++) {
				for (int y = 162; y <= 166; y++) {
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

	std::cout << bestDistance << std::endl;

	return result;
}

}
