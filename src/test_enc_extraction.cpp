#include "enc_extraction/enc_extract_lib.h"
#include "ros/ros.h"
#include "ros/package.h"

int main(int argc, char *argv[]){
    ros::init(argc, argv, "test_enc_extraction");
    std::string path = ros::package::getPath("enc_extraction");
    std::cout << path << std::endl;
    extractorRegion r = {-74.02483,40.49961,-73.72579,40.64967};
    extractorVessel v = {3,10,3,0.5,0.5,0.5,0};
    ENCExtractor extractor(path,r,v);
    extractor.loadDatasets(extractor.determineChartsToLoad(r));
    extractor.createCheckDB();
    //ros::spin();
}