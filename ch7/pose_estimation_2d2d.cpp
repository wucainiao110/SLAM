#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>

using namespace std;
using namespace cv;

void find_feature_matches(const Mat& img_1, const Mat& img_2,
			  std::vector<KeyPoint>& keypoints_1,
			  std::vector<KeyPoint>& keypoints_2,
			  std::vector<DMatch>& matches );

void pose_estimation_2d2d(std::vector<KeyPoint> keypoints_1,
			  std::vector<KeyPoint> keypoints_2,
			  std::vector<DMatch> matches,
			  Mat& R, Mat& t);

//transform pixel coordinate to normalized camera coodinate
Point2d pixel2cam(const Point2d& p,const Mat& K);

int main(int argc, char** argv)
{
  if(argc != 3)
  {
    cout << "usage: pose_estimation_2d2d img1 img2 " << endl;
    return 1;
  }
  
  // read in images
  Mat img_1 = imread(argv[1],CV_LOAD_IMAGE_COLOR);
  Mat img_2 = imread(argv[2],CV_LOAD_IMAGE_COLOR);
  
  //Mat img_1 = imread("1.png",CV_LOAD_IMAGE_COLOR);
  //Mat img_2 = imread("2.png",CV_LOAD_IMAGE_COLOR);
  
  //cout << img_1;
  /*
   */
  
  vector<KeyPoint> keypoints_1,keypoints_2;
  vector<DMatch> matches;
  find_feature_matches(img_1, img_2,keypoints_1,keypoints_2,matches);
  cout << "find " << matches.size() << "matching points" << endl;
  
  //estimate the movement of two images
  Mat R,t;
  pose_estimation_2d2d(keypoints_1,keypoints_2,matches,R,t);
  
  //verify E=t^R*scale
  Mat t_x = (Mat_<double>(3,3) <<
	      0,    -t.at<double>(2,0), t.at<double>(1,0),
	     t.at<double>(2,0) , 0, -t.at<double>(0,0),
	     -t.at<double>(1,0),t.at<double>(0,0),0
  ) ;
  
  cout << "t^R = " << endl << t_x*R << endl;
  
  //verify epipolar constraint
  Mat K = (Mat_<double>(3,3) << 520.9,0,325.1,0,521.0,249.7,0,0,1);
  for(DMatch m: matches)
  {
    Point2d pt1 = pixel2cam(keypoints_1[m.queryIdx].pt,K);
    Mat y1 = (Mat_<double>(3,1) << pt1.x,pt1.y ,1);
    Point2d pt2 = pixel2cam(keypoints_2[m.trainIdx].pt,K);
    Mat y2 = (Mat_<double>(3,1) << pt2.x,pt2.y ,1);
    
    Mat d = y2.t() * t_x * R * y1;
    cout << "epipolar constraint = " << d << endl;
  }
  return 0;
  
}


void find_feature_matches(const Mat& img_1, const Mat& img_2, 
			  std::vector< KeyPoint >& keypoints_1, 
			  std::vector< KeyPoint >& keypoints_2, 
			  std::vector< DMatch >& matches)
{
  //init
  Mat descriptors_1,descriptors_2;
  Ptr<FeatureDetector> detector = ORB::create();
  Ptr<DescriptorExtractor> descriptor = ORB::create();
  
  Ptr<DescriptorMatcher> matcher = DescriptorMatcher::create("BruteForce-Hamming");
  
  //first step : detect Oriented FAST angel points
  detector->detect(img_1, keypoints_1);
  detector->detect(img_2, keypoints_2);
  
  //second step : compute BRIEF descriptor according to the angel points
  descriptor->compute(img_1,keypoints_1,descriptors_1);
  descriptor->compute(img_2,keypoints_2,descriptors_2);
  
  //3th step : match the BRIEF descriptors between the two imgs
  vector<DMatch> match;
  matcher->match(descriptors_1,descriptors_2,match);
  
  //4th step : filtrate the matching points
  double min_dist = 10000,max_dist = 0;
  
  //find out the min distance and max distance among the matchings.
  //in another word,the distance of most similar group of points and worst ones
  for(int i=0;i<descriptors_1.rows ; i++)
  {
    double dist = match[i].distance;
    if(dist < min_dist)
      min_dist = dist;
    if(dist > max_dist)
      max_dist = dist;
    
  }
  
  printf("-- Max dist : %f \n",max_dist);
  printf("-- Min dist : %f \n",min_dist);
  
  //if the distance between the two points is twice bigger than the min distance
  //the matching crashes.
  //however sometimes the min distance is pretty small, we can set a floor of 30
  for(int i =0; i<descriptors_1.rows ; i++)
  {
    if(match[i].distance <= max(2*min_dist,30.0))
      matches.push_back(match[i]);
  }  
}


Point2d pixel2cam(const Point2d& p, const Mat& K)
{
  return Point2d
  (
    (p.x - K.at<double>(0,2)) / K.at<double>(0,0),
    (p.y - K.at<double>(1,2)) / K.at<double>(1,1)
  );
}


void pose_estimation_2d2d(std::vector< KeyPoint > keypoints_1, 
			  std::vector< KeyPoint > keypoints_2, 
			  std::vector< DMatch > matches, 
			  Mat& R, Mat& t)
{
  //the internal reference of the camera
  Mat K = (Mat_<double>(3,3) << 520.9, 0, 325.1, 0, 521.0, 249.7, 0, 0, 1);
  
  //transform the matching points to vector<Point2d>
  vector<Point2f> points1;
  vector<Point2f> points2;
  
  for(int i =0;i<(int)matches.size();i++)
  {
    points1.push_back(keypoints_1[matches[i].queryIdx].pt);
    points2.push_back(keypoints_2[matches[i].trainIdx].pt);
  }
  
  //compute fundamental matrix
  Mat fundamental_matrix;
  fundamental_matrix = findFundamentalMat(points1,points2,CV_FM_8POINT);
  cout << "fundamental_matrix is " <<endl << fundamental_matrix <<endl;
  
  //compute essential matrix
  Point2d principal_point(325.1,249.7);
  double focal_length =521;
  Mat essential_matrix;
  essential_matrix = findEssentialMat(points1,points2,focal_length,principal_point);
  cout << "essential_matrix is " << endl << essential_matrix << endl;
  
  //compute Homography
  Mat homography_matrix;
  homography_matrix = findHomography(points1,points2,RANSAC,3);
  cout << "homography_matrix is " <<endl << homography_matrix << endl;
  
  //recover R and t through essential_matrix
  recoverPose(essential_matrix,points1,points2,R,t,focal_length,principal_point);
  cout << "R is " << endl << R << endl;
  cout << "t is " << endl << t << endl;
   
}








