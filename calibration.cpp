/** @Image calibration application
 ** @Estimate fundamental or homography matrix
 ** @author Zhe Liu
 **/

/*
Copyright (C) 2011-12 Zhe Liu and Pierre Moulon.
All rights reserved.

This file is part of the KVLD library and is made available under
the terms of the BSD license (see the COPYING file).
*/

#include <algorithm>
#include <memory>

#include "kvld/kvld.h"
#include "convert.h"

#include <cv.hpp>
#include <cxcore.h>
#include <highgui.h>
#include "opencv2/nonfree/features2d.hpp" 


int main(int argc,char*argv[]) {
//================= load images ======================//
	cv::Mat image1, image2;
  int imageID=2;// index of a pair of images you want to use in folder demo_images
  std::string index;
  std::stringstream f;
  f<<imageID;
  f>>index;
  std::string input=std::string(SOURCE_DIR)+"/demo_image/IMG_";
  image1= cv::imread(input+index+".jpg", CV_LOAD_IMAGE_GRAYSCALE);
  image2= cv::imread(input+index+"bis.jpg", CV_LOAD_IMAGE_GRAYSCALE);

//=============== compute SIFT points =================//
  std::cout<<"Extracting SIFT features"<<std::endl;
  std::vector<cv::KeyPoint> feat1,feat2;

  cv::SiftFeatureDetector* detectortype=new  cv::SiftFeatureDetector() ;
  cv::PyramidAdaptedFeatureDetector detector2(detectortype,5);// 5 levels of image scale
  cv::SiftDescriptorExtractor extractor;
  cv::Mat descriptors1,descriptors2;

  detector2.detect(image1,feat1);
  extractor.compute(image1,feat1,descriptors1);
  std::cout<< "sift:: 1st image: " << feat1.size() << " keypoints"<<std::endl;
  
  detector2.detect(image2,feat2);
  extractor.compute(image2,feat2,descriptors2);
  std::cout<< "sift:: 2nd image: " << feat2.size() << " keypoints"<<std::endl;

//=============== compute matches using brute force matching ====================//
  std::vector<cv::DMatch> matches;
  bool bSymmetricMatches = false;
  cv::BFMatcher matcher(cv::NORM_L2, bSymmetricMatches);
  matcher.match(descriptors1,descriptors2,matches);

//=============== convert openCV sturctures to KVLD recognized elements
  Image<float> If1, If2;
  Convert_image(image1, If1);
  Convert_image(image2, If2);
  
  std::vector<keypoint> F1, F2;
  Convert_detectors(feat1,F1);//we only need detectors for KVLD
  Convert_detectors(feat2,F2);//we only need detectors for KVLD
  std::vector<Pair> matchesPair;
  Convert_matches(matches,matchesPair);

//===============================  KVLD method ==================================//
	std::cout<<"K-VLD starts with "<<matches.size()<<" matches"<<std::endl;
    
  std::vector<Pair> matchesFiltered;
 std::vector<double> vec_score;
    
  //In order to illustrate the gvld(or vld)-consistant neighbors, the following two parameters has been externalized as inputs of the function KVLD.
  libNumerics::matrix<float> E = libNumerics::matrix<float>::ones(matches.size(),matches.size())*(-1);
  // gvld-consistency matrix, intitialized to -1,  >0 consistency value, -1=unknow, -2=false  
  std::vector<bool> valide(matches.size(), true);// indices of match in the initial matches, if true at the end of KVLD, a match is kept.

  size_t it_num=0;
  KvldParameters kvldparameters;//initial parameters of KVLD

  while (it_num < 5 && kvldparameters.inlierRate>KVLD(If1, If2,F1,F2, matchesPair, matchesFiltered, vec_score,E,valide,kvldparameters)) {
    kvldparameters.inlierRate/=2;
    std::cout<<"low inlier rate, re-select matches with new rate="<<kvldparameters.inlierRate<<std::endl;
    kvldparameters.K=2;
    it_num++;
  }
	std::cout<<"K-VLD filter ends with "<<matchesFiltered.size()<<" selected matches"<<std::endl;
//====================fundamental matrix verification================//
  double precision=0.0;// ORSA will put an appropriate value
  bool homography=false;
  FCrit crit=Find_Model(If1,If2,F1,F2,matchesFiltered,precision,homography);

//================= write files to output folder ==================//
  std::cout<<"Please check the output folder for results"<<std::endl;
  std::string output=std::string(SOURCE_DIR)+"/demo_output/IMG_"+index+"_";
  writeResult(output,F1, F2, matchesPair, matchesFiltered, vec_score);
  
  std::ofstream matrix((output+"matrix.txt").c_str());
  matrix<<crit.getMatrix();
  matrix.close();
//================= Visualize matching result ====================//
  cv::Mat image1color, image2color, concat;
  image1color= cv::imread(input+index+".jpg", CV_LOAD_IMAGE_COLOR);
  image2color= cv::imread(input+index+"bis.jpg", CV_LOAD_IMAGE_COLOR);

  cv::vconcat(image1color, image2color,concat);
  for(  std::vector<cv::DMatch>::const_iterator ptr = matches.begin(); ptr != matches.end(); ++ptr)
  {
    cv::KeyPoint start = feat1[ptr->queryIdx]; 
    cv::KeyPoint end = feat2[ptr->trainIdx];
    cv::line( concat,start.pt, end.pt+cv::Point2f(0,image1.rows),cv::Scalar(0, 255,0 ));
  }
  cv::imwrite(output+"initial.png",concat);

  //========== KVLD result =============//
  cv::vconcat(image1color, image2color,concat);

  //draw gvld-consistant neighbors (not exhostive), may include outliers rejected by ORSA
  for (int it1=0; it1<matchesPair.size()-1;it1++){
    for (int it2=it1+1; it2<matchesPair.size();it2++){
      if (valide[it1] && valide[it2] && E(it1,it2)>=0){

        cv::KeyPoint l1 = feat1[matchesPair[it1].first];
        cv::KeyPoint l2 = feat1[matchesPair[it2].first];

        cv::KeyPoint r1 = feat2[matchesPair[it1].second];
        cv::KeyPoint r2 = feat2[matchesPair[it2].second];

        cv::line(concat,l1.pt, l2.pt,cv::Scalar(255,0,255),2);
        cv::line(concat,r1.pt+cv::Point2f(0,image1.rows), r2.pt+cv::Point2f(0,image1.rows),cv::Scalar(255,0,255),2);

      }
    }
  }
  for( std::vector<Pair >::const_iterator ptr = matchesFiltered.begin(); ptr != matchesFiltered.end(); ++ptr)
    if (crit(F1[ptr->first],F2[ptr->second])){
      size_t i = ptr->first;
      size_t j = ptr->second;
      cv::KeyPoint start = feat1[i]; 
      cv::KeyPoint end = feat2[j];

      cv::line( concat,start.pt, end.pt+cv::Point2f(0,image1.rows),cv::Scalar(0, 255, 0 ));

  }
  cv::imwrite(output+"kvld_filtered.png",concat);
	return 0;
}
