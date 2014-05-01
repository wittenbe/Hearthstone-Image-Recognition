#ifndef STREAM_H_
#define STREAM_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace hs {

class Stream {
public:
	Stream(std::vector<std::string> urls);
	/*
	 * videocapture(the read method) actually only returns a reference to the buffer
	 * if you don't copy and use multiple threads, one thread reading an image will overwrite/replace the data of other threads' images
	 */
	void setCopyOnRead(bool copyOnRead);
	bool read(cv::Mat& image);
	void setStreamIndex(size_t i);
	void setFramePos(double n);
	size_t getStreamIndex();
	double getFramePos();
	void skipFrame();
	void skipFrames(double n);
	bool isLivestream();

private:
	bool openNextStream();

	cv::VideoCapture vcap;
	boost::mutex frameMutex; //mutex for sequential frame grabbing
	std::vector<std::string> urls;
	size_t urlIndex;

	bool isLiveStream;
	bool copyOnRead;
	double frameCount;
};

typedef boost::shared_ptr<Stream> StreamPtr;

}


#endif /* STREAM_H_ */
