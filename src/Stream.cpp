#include "Stream.h"
#include <stdio.h>
#include <iostream>
#include <algorithm>

namespace hs {

Stream::Stream(std::vector<std::string> urls) {
	this->urls = urls;
	isLiveStream = false;
	frameCount = 0;
	urlIndex = 0;

	openNextStream();
}

bool Stream::openNextStream() {
	bool opened = false;
	if (urlIndex < urls.size()) {
		vcap.release();
		vcap.open(urls[urlIndex++]);
		if (!vcap.isOpened()) {
			printf("Error opening video stream or file\n");
		} else {
			opened = true;
			frameCount = vcap.get(CV_CAP_PROP_FRAME_COUNT);
			isLiveStream = frameCount < 0; //frameCount is a large negative number if this is a live stream
		}
	}
	return opened;
}

bool Stream::read(cv::Mat& image) {
	frameMutex.lock();
	bool success = vcap.isOpened() && vcap.read(image);
	if (!isLiveStream && !success) {
		success = openNextStream() && vcap.read(image);
	}

	frameMutex.unlock();
	return success;
}

void Stream::setStream(unsigned int i) {
	if (!isLiveStream) {
		urlIndex = i;
		openNextStream();
	}
}

void Stream::setFramePos(double n) {
	vcap.set(CV_CAP_PROP_POS_FRAMES, n);
}

double Stream::getFramePos() {
	return vcap.get(CV_CAP_PROP_POS_FRAMES);
}

void Stream::skipFrame() {
	skipFrames(1);
}

void Stream::skipFrames(double n) {
	vcap.set(CV_CAP_PROP_POS_FRAMES, vcap.get(CV_CAP_PROP_POS_FRAMES) + n); //doesn't seem to always work on live streams?
}

bool Stream::isLivestream() {
	return isLiveStream;
}

}
