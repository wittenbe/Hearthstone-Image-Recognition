#include "PerceptualHash.h"
#include <iostream>

namespace hs {

cv::Mat PerceptualHash::k = 1.f/9.f * (cv::Mat_<float>(3,3) << 1, 1, 1, 1, 1, 1, 1, 1, 1);

ulong64 PerceptualHash::phash(const cv::Mat& image) {
	cv::Mat temp1, temp2;
	if (image.channels() == 1) {
		temp1 = image.clone();
	} else {
		cv::cvtColor(image, temp1, CV_BGR2GRAY);
	}
	cv::filter2D(temp1, temp1, CV_32FC1, k);

	cv::resize(temp1, temp2, cv::Size(32, 32), 0, 0, CV_INTER_LINEAR);
	cv::dct(temp2, temp2);

	std::vector<float> vals(64);
	for(int r = 1; r < 9; r++){
	    for(int c = 1; c < 9; c++){
	        vals.push_back(temp2.at<float>(r, c));
	    }
	}
	std::sort(vals.begin(), vals.end());
	const float median = (vals[31] + vals[32]) / 2.f;

	ulong64 result = 0;
	ulong64 index = 1;
	for(int r = 1; r < 9; r++){
	    for(int c = 1; c < 9; c++){
	        if (temp2.at<float>(r, c) > median) {
	        	result |= index;
	        }
	        index <<= 1;
	    }
	}
	return result;
}

int PerceptualHash::hammingDistance(const hs::ulong64& a, const hs::ulong64& b) {
	hs::ulong64 x = a ^ b;
    int count;
    for (count=0; x; count++)
        x &= x-1;
    return count;
}

PerceptualHash::ComparisonResult PerceptualHash::best(const ulong64& hash, const std::vector<ulong64>& dataSet) {
	size_t minIndex = -1;
	int minDist = sizeof(ulong64) * 8;

	for (size_t i = 0; i < dataSet.size(); i++) {
		const int dist = hammingDistance(hash, dataSet[i]);
		if (dist < minDist) {
			minDist = dist;
			minIndex = i;
		}
	}

	ComparisonResult c;
	c.distance = minDist;
	c.index = minIndex;

	return c;
}

std::vector<PerceptualHash::ComparisonResult> PerceptualHash::nbest(int n, const ulong64& hash, const std::vector<ulong64>& dataSet) {
	if (n == 1) {
		std::vector<PerceptualHash::ComparisonResult> result;
		result.push_back(best(hash, dataSet));
		return result;
	}

	std::vector<int> nbestDistances(n, sizeof(ulong64) * 8);
	std::vector<size_t> nbestObjectIndexes(n, -1);

	for (size_t i = 0; i < dataSet.size(); i++) {
		const int dist = hammingDistance(hash, dataSet[i]);
		for (size_t j = 0; j < nbestDistances.size(); j++) {
			if (dist < nbestDistances[j]) {
				for (size_t k = n - 1; k > j; k--) {
					nbestDistances[k] = nbestDistances[k - 1];
					nbestObjectIndexes[k] = nbestObjectIndexes[k - 1];
				}
				nbestDistances[j] = dist;
				nbestObjectIndexes[j] = i;
				break;
			}
		}
	}

	std::vector<ComparisonResult> nbestObjects(n);
	for (size_t i = 0; i < n; i++) {
		ComparisonResult c;
		c.distance = nbestDistances[i];
		c.index = nbestObjectIndexes[i];
		nbestObjects[i] = c;
	}

	return nbestObjects;
}

}
