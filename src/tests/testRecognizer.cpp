#include "../Recognizer.h"
#include "../Config.h"
#include "../Database.h"
#include <boost/timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace hs;
using namespace cv;
using namespace std;

RecognizerPtr r;
vector<Recognizer::RecognitionResult> rr;

string p(string relpath) {
	return Config::getConfig().get<std::string>("config.paths.misc_image_path") + "/" + relpath;
}

string data(string filename) {
	return p("../../data/" + filename);
}

void testRecognition(string image, int expected, int recognizers) {
	printf("------- %20s ...", image.c_str());
	cv::Mat img = cv::imread(data(image), CV_LOAD_IMAGE_COLOR);
	auto startTime = boost::posix_time::microsec_clock::local_time();
	rr = r->recognize(img, recognizers);
	bool success = false;
	if (rr.size() >= 1) {
		printf("FOUND %7d ...", rr[0].results[0]);
	} else {
		printf("FOUND %7s ...", "NOTHING");
	}

	if (expected == -1) {
		success = rr.size() == 0;
	} else {
		success = rr.size() == 1 && rr[0].results[0] == expected;
	}

	//time stuff
	auto endTime = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_duration diff = endTime - startTime;
	const auto elapsed = diff.total_milliseconds();

	if (success) {
		printf("PASS (%3dms)\n", elapsed);
	} else {
		printf("FAIL (%3dms) !!!!!!!!\n", elapsed);
	}
}

int main( int argc, char** argv ) {
	r = RecognizerPtr(
			new Recognizer(
					DatabasePtr(new Database(Config::getConfig().get<std::string>("config.paths.recognition_data_path"))),
					Config::getConfig().get<std::string>("config.stream.streamer")));

	HS_INFO << "=========== Win/Loss Tests start ===========" << std::endl;
	testRecognition("data_d1.png", RESULT_GAME_END_DEFEAT, RECOGNIZER_GAME_END);
	testRecognition("data_d2.png", RESULT_GAME_END_DEFEAT, RECOGNIZER_GAME_END);
	testRecognition("data_d3.png", RESULT_GAME_END_DEFEAT, RECOGNIZER_GAME_END);
	testRecognition("data_v1.png", RESULT_GAME_END_VICTORY, RECOGNIZER_GAME_END);
	testRecognition("data_v2.png", RESULT_GAME_END_VICTORY, RECOGNIZER_GAME_END);
	testRecognition("data_v3.png", RESULT_GAME_END_VICTORY, RECOGNIZER_GAME_END);
	testRecognition("data_v4.png", RESULT_GAME_END_VICTORY, RECOGNIZER_GAME_END);
	testRecognition("massan_victory.png", RESULT_GAME_END_VICTORY, RECOGNIZER_GAME_END);
	testRecognition("defeat_trumpsc.png", RESULT_GAME_END_DEFEAT,RECOGNIZER_GAME_END);
	testRecognition("victory_trumpsc.png", RESULT_GAME_END_VICTORY,RECOGNIZER_GAME_END);
	testRecognition("winfalse0.png", -1, RECOGNIZER_GAME_END);
	testRecognition("winfalse1.png", -1, RECOGNIZER_GAME_END);
	testRecognition("winfalse2.png", -1, RECOGNIZER_GAME_END);
	testRecognition("winfalse3.png", -1, RECOGNIZER_GAME_END);
	HS_INFO << "=========== Win/Loss Tests end ===========" << std::endl;

	HS_INFO << "=========== Coin Tests start ===========" << std::endl;
	testRecognition("data_coinf1.png", RESULT_GAME_COIN_FIRST, RECOGNIZER_GAME_COIN);
	testRecognition("data_coins0.png", RESULT_GAME_COIN_SECOND, RECOGNIZER_GAME_COIN);
	HS_INFO << "=========== Coin Tests end ===========" << std::endl;
}
