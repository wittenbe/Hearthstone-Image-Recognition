#ifndef STREAM_H_
#define STREAM_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace hs {

class Stream {
public:
	Stream(std::vector<std::string> urls); //class for managing live streams or vods (which consist of multiple archives/flvs)
	bool read(cv::Mat& image);
	void setStream(unsigned int i);
	void setFramePos(double n);
	double getFramePos();
	void skipFrame();
	void skipFrames(double n);

private:
	bool openNextStream();

	cv::VideoCapture vcap;
	boost::mutex frameMutex; //mutex for sequential frame grabbing
	std::vector<std::string> urls;
	unsigned int urlIndex;

	bool isLiveStream;
	double frameCount;
};

typedef boost::shared_ptr<Stream> StreamPtr;

}


#endif /* STREAM_H_ */
