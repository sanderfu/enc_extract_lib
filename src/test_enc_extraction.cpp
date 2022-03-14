#include "enc_extraction/enc_extract_lib.h"
#include "ros/ros.h"
#include "ros/package.h"

int main(int argc, char *argv[]){
    GDALAllRegister();
    ros::init(argc, argv, "test_enc_extraction");
    std::string path = ros::package::getPath("enc_extraction");
    std::cout << path << std::endl;
    extractorRegion r(-74.02483,40.49961,-73.72579,40.64967);
    extractorVessel v(3,10,3,0.5);
    GDALDriver* driver_sqlite = GetGDALDriverManager()->GetDriverByName("SQLite");
    if(driver_sqlite==NULL){
        throw std::runtime_error("Unable to find SQLite driver");
        exit(1);
    }
    std::string check_path = path+"/data/check_db.sqlite";
    GDALDataset* check_db = driver_sqlite->Create(check_path.c_str(),0,0,0,GDT_Unknown,NULL);
    ENCExtractor extractor(r,v,check_db);
    extractor.createCheckDB();
    //ros::spin();
}