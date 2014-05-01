#include "Stream.h"
#include "../Logger.h"
#include <stdio.h>
#include <iostream>
#include <algorithm>

namespace hs {

Stream::Stream(std::vector<std::string> urls) {
	this->urls = urls;
	copyOnRead = false;
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
			HS_ERROR << "Error opening video stream or file(s)" << std::endl;
		} else {
			opened = true;
			frameCount = vcap.get(CV_CAP_PROP_FRAME_COUNT);
			isLiveStream = frameCount < 0; //frameCount is a large negative number if this is a live stream
		}
	}
	return opened;
}

void Stream::setCopyOnRead(bool copyOnRead) {
	this->copyOnRead = copyOnRead;
}

bool Stream::read(cv::Mat& image) {
	frameMutex.lock();
	bool success = vcap.isOpened() && vcap.read(image);
	if (!isLiveStream && !success) {
		success = openNextStream() && vcap.read(image);
	}

	if (copyOnRead) {
		cv::Mat copyImage;
		image.copyTo(copyImage);
		image = copyImage;
	}
	frameMutex.unlock();
	return success;
}

void Stream::setStreamIndex(size_t i) {
	if (!isLiveStream) {
		urlIndex = i;
		openNextStream();
	}
}

void Stream::setFramePos(double n) {
	vcap.set(CV_CAP_PROP_POS_FRAMES, n);
}

size_t Stream::getStreamIndex() {
	return urlIndex - 1;
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
