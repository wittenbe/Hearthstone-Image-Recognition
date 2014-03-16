#include "Calibrator.h"
#include "../Recognizer.h"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <boost/timer.hpp>

using namespace hs;

//int main(int argc, char* argv[]) {
//	if (argc < 2) {
//		std::cout << "Usage: " << std::endl << " " << argv[0] << " <path_to_image>" << std::endl;
//	}
//
//	cv::Mat img = cv::imread("ref.png");
//
//	RecognizerPtr r = RecognizerPtr(new Recognizer());
//	Calibrator::calibrate(img, r);
//
//}

//#include <stdio.h>
//#include <iostream>
//#include "opencv2/core/core.hpp"
//#include "opencv2/features2d/features2d.hpp"
//#include "opencv2/highgui/highgui.hpp"
//#include "opencv2/calib3d/calib3d.hpp"
//#include "opencv2/nonfree/nonfree.hpp"
//#include <boost/timer.hpp>
//
//using namespace cv;
//
//void readme();
//
///** @function main */
//int main( int argc, char** argv )
//{
//
//  Mat img_object = imread( argv[2], CV_LOAD_IMAGE_GRAYSCALE );
//  Mat img_scene = imread( argv[1], CV_LOAD_IMAGE_GRAYSCALE );
////  img_scene = img_scene(
////  		cv::Range(598, 598+128),
////  		cv::Range(724, 724+481));
//
//  if( !img_object.data || !img_scene.data )
//  { std::cout<< " --(!) Error reading images " << std::endl; return -1; }
//
//  //-- Step 1: Detect the keypoints using SURF Detector
//  int minHessian = 400;
//
//  SurfFeatureDetector detector( minHessian );
//
//  std::vector<KeyPoint> keypoints_object, keypoints_scene;
//
//  detector.detect( img_object, keypoints_object );
//  detector.detect( img_scene, keypoints_scene );
//
//  //-- Step 2: Calculate descriptors (feature vectors)
//  SurfDescriptorExtractor extractor;
//
//  Mat descriptors_object, descriptors_scene;
//
//  extractor.compute( img_object, keypoints_object, descriptors_object );
//  extractor.compute( img_scene, keypoints_scene, descriptors_scene );
//
//  //-- Step 3: Matching descriptor vectors using FLANN matcher
//  FlannBasedMatcher matcher;
//  std::vector< DMatch > matches;
//  matcher.match( descriptors_object, descriptors_scene, matches );
//
//  double max_dist = 0; double min_dist = 100;
//
//  //-- Quick calculation of max and min distances between keypoints
//  for( int i = 0; i < descriptors_object.rows; i++ )
//  { double dist = matches[i].distance;
//    if( dist < min_dist ) min_dist = dist;
//    if( dist > max_dist ) max_dist = dist;
//  }
//
//  printf("-- Max dist : %f \n", max_dist );
//  printf("-- Min dist : %f \n", min_dist );
//
//  //-- Draw only "good" matches (i.e. whose distance is less than 3*min_dist )
//  std::vector< DMatch > good_matches;
//
//  for( int i = 0; i < descriptors_object.rows; i++ )
//  { if( matches[i].distance < 3*min_dist )
//     { good_matches.push_back( matches[i]); }
//  }
//
//  Mat img_matches;
//  drawMatches( img_object, keypoints_object, img_scene, keypoints_scene,
//               good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
//               vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );
//
//  //-- Localize the object
//  std::vector<Point2f> obj;
//  std::vector<Point2f> scene;
//
//  for( int i = 0; i < good_matches.size(); i++ )
//  {
//    //-- Get the keypoints from the good matches
//    obj.push_back( keypoints_object[ good_matches[i].queryIdx ].pt );
//    scene.push_back( keypoints_scene[ good_matches[i].trainIdx ].pt );
//  }
//
//  Mat H = findHomography( obj, scene, CV_RANSAC);
//  std::cout << H << std::endl;
//
//
//  //-- Get the corners from the image_1 ( the object to be "detected" )
//  std::vector<Point2f> obj_corners(4);
//  obj_corners[0] = cvPoint(0,0); obj_corners[1] = cvPoint( img_object.cols, 0 );
//  obj_corners[2] = cvPoint( img_object.cols, img_object.rows ); obj_corners[3] = cvPoint( 0, img_object.rows );
//  std::vector<Point2f> scene_corners(4);
//
//  perspectiveTransform( obj_corners, scene_corners, H);
//
//  //-- Draw lines between the corners (the mapped object in the scene - image_2 )
//  line( img_matches, scene_corners[0] + Point2f( img_object.cols, 0), scene_corners[1] + Point2f( img_object.cols, 0), Scalar(0, 255, 0), 4 );
//  line( img_matches, scene_corners[1] + Point2f( img_object.cols, 0), scene_corners[2] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
//  line( img_matches, scene_corners[2] + Point2f( img_object.cols, 0), scene_corners[3] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
//  line( img_matches, scene_corners[3] + Point2f( img_object.cols, 0), scene_corners[0] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
//
//  //-- Show detected matches
//  imshow( "Good Matches & Object detection", img_matches );
//
//  waitKey(0);
//  return 0;
//  }
//
//  /** @function readme */
//  void readme()
//  { std::cout << " Usage: ./SURF_descriptor <img1> <img2>" << std::endl; }

#include <stdio.h>
#include <iostream>
#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/nonfree/nonfree.hpp"
#include "opencv2/gpu/gpu.hpp"
#include <boost/timer.hpp>
#include <algorithm>

using namespace cv;

void readme();

/** @function main */
int main( int argc, char** argv )
{
	Rect_<float> r = Rect_<float>(0.3841f, 0.53751f, 0.2471f, 0.148f);
	Mat img_object = imread( argv[2], CV_LOAD_IMAGE_GRAYSCALE );
	Mat img_sceneO = imread( argv[1], CV_LOAD_IMAGE_GRAYSCALE );
	Mat img_scene = img_sceneO(
    		Range(r.y * img_sceneO.rows, (r.y + r.height) * img_sceneO.rows),
    		Range(r.x * img_sceneO.cols, (r.x + r.width) * img_sceneO.cols));
	std::cout << r.y * img_sceneO.rows << " " << r.x * img_sceneO.cols << " " << (r.y + r.height) * img_sceneO.rows << " " << (r.x + r.width) * img_sceneO.cols << std::endl;

	if( !img_object.data || !img_scene.data )
	{ std::cout<< " --(!) Error reading images " << std::endl; return -1; }

	//-- Step 1: Detect the keypoints using SURF Detector
	int minHessian = 400;

	SurfFeatureDetector detector(100, 4, 3, false, true);

	std::vector<KeyPoint> keypoints_object, keypoints_scene;
	Mat descriptors_object, descriptors_scene;

	detector.detect( img_object, keypoints_object );


	//-- Step 2: Calculate descriptors (feature vectors)
	SurfDescriptorExtractor extractor(4,3);

	extractor.compute( img_object, keypoints_object, descriptors_object );


	boost::timer t;
	detector.detect( img_scene, keypoints_scene );
	extractor.compute( img_scene, keypoints_scene, descriptors_scene );
	vector< DMatch > good_matches;

	//-- Step 3: Matching descriptor vectors using FLANN matcher
//	FlannBasedMatcher matcher;
//	std::vector<std::vector<DMatch> > matches;
//	matcher.knnMatch( descriptors_object, descriptors_scene, matches, 2);
//	float mindist = 100.f;
	std::cout << descriptors_object.rows << " " << descriptors_object.cols << std::endl;
	std::cout << descriptors_scene.rows << " " << descriptors_scene.cols << std::endl;

	BFMatcher matcher(NORM_L2);
	std::vector<std::vector<DMatch> > matches;
	matcher.knnMatch(descriptors_object, descriptors_scene, matches, 2);

	for (size_t i = 0; i < matches.size(); ++i) {
		if (matches[i].size() < 2)
				continue;

		const DMatch &m1 = matches[i][0];
		const DMatch &m2 = matches[i][1];

		if (m1.distance <= 0.75f * m2.distance) {
			std::cout << m1.distance << " " << m2.distance << std::endl;
			good_matches.push_back(m1);
		}
//		if (good_matches.size() >= 7) break;
	}
	std::cout << t.elapsed() << std::endl;
	if ( (good_matches.size() >= 7)) {
		std::cout << "good matches " << good_matches.size() << std::endl;
	} else {
		std::exit(0);}


	Mat img_matches;
	drawMatches( img_object, keypoints_object, img_scene, keypoints_scene,
			   good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
			   vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );

	//-- Localize the object
	std::vector<Point2f> obj;
	std::vector<Point2f> scene;

	for( int i = 0; i < good_matches.size(); i++ )
	{
	//-- Get the keypoints from the good matches
	obj.push_back( keypoints_object[ good_matches[i].queryIdx ].pt );
	scene.push_back( keypoints_scene[ good_matches[i].trainIdx ].pt );
	}

	Mat H = findHomography( obj, scene, CV_RANSAC);
	std::cout << H << std::endl;


	//-- Get the corners from the image_1 ( the object to be "detected" )
	std::vector<Point2f> obj_corners(4);
	obj_corners[0] = cvPoint(0,0); obj_corners[1] = cvPoint( img_object.cols, 0 );
	obj_corners[2] = cvPoint( img_object.cols, img_object.rows ); obj_corners[3] = cvPoint( 0, img_object.rows );
	std::vector<Point2f> scene_corners(4);

	perspectiveTransform( obj_corners, scene_corners, H);

	//-- Draw lines between the corners (the mapped object in the scene - image_2 )
	line( img_matches, scene_corners[0] + Point2f( img_object.cols, 0), scene_corners[1] + Point2f( img_object.cols, 0), Scalar(0, 255, 0), 4 );
	line( img_matches, scene_corners[1] + Point2f( img_object.cols, 0), scene_corners[2] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
	line( img_matches, scene_corners[2] + Point2f( img_object.cols, 0), scene_corners[3] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
	line( img_matches, scene_corners[3] + Point2f( img_object.cols, 0), scene_corners[0] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );

	//-- Show detected matches
	imshow( "Good Matches & Object detection", img_matches );

	waitKey(0);
	return 0;
	}

	/** @function readme */
	void readme()
	{ std::cout << " Usage: ./SURF_descriptor <img1> <img2>" << std::endl; }
