#pragma once
#include "string"
#include "vector"
#include "unordered_map"
#include <fstream>
#include "iostream"
#include <boost/algorithm/string.hpp>
#include "gdal/ogrsf_frmts.h"

enum S57{
    LNDARE = 0,
    DEPARE = 1,
    OBSTRN = 2,
    OFSPLF = 3,
    PILPNT = 4,
    PYLONS = 5,
    SOUNDG = 6,
    UWTROC = 7,
    WRECKS = 8,
    BCNSPP = 9,
    BOYLAT = 10,
    BRIDGE = 11,
    CTNARE = 12,
    FAIRWY = 13,
    RESARE = 14
};

enum Mode{
    COLLISION = 1,
    CAUTION = 2,
};

struct extractorRegion{
    double min_lon;
    double min_lat;
    double max_lon;
    double max_lat;
};

struct extractorVessel{
    double width;
    double length;
    double height;
    double draft;
    //
    double draft_safety_margin;
    double height_safety_margin;
    double horisontal_safety_margin;
};

class ENCExtractor{
    public:
        ENCExtractor(std::string path, extractorRegion& r, extractorVessel& v);
    //private:
        std::vector<GDALDataset*> datasets_;
        std::string path_;
        extractorRegion region_;
        extractorVessel vessel_;

        GDALDataset* check_db_;
        GDALDataset* detailed_db_;

        std::vector<std::string> feature_layer_names = {"LNDARE","DEPARE","OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE","CTNARE","FAIRWY","RESARE"};
        std::vector<std::string> collision_features  = {"LNDARE","DEPARE"}; //,"OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE"};
        std::vector<std::string> caution_features = {"CTNARE","FAIRWY","RESARE","BCNSPP","BOYLAT","OBSTRN","OFSPLF","PILPNT","PYLONS"};

        void loadDatasets(std::vector<std::string> enc_paths);
        bool pointInRegion(double lon, double lat, extractorRegion& r);
        std::vector<std::string> determineChartsToLoad(extractorRegion& r);
        void manipulateLayer(OGRLayer* layer, bool verbose=false);
        bool addFeature(OGRLayer* layer, OGRFeature* feature, Mode mode);
        void addFeaturesToLayer(OGRLayer* in_layer,OGRLayer* out_layer, Mode mode);
        void extractCollisionFeatures(OGRLayer* in_layer, GDALDataset* out_ds);
        void extractCautionFeatures(OGRLayer* in_layer, GDALDataset* out_ds);
        void extractFeature(std::string layername, GDALDataset* in_ds, GDALDataset* out_ds);
        void dissolveLayer(OGRLayer* in_layer, GDALDataset* in_ds, GDALDataset* out_ds);

        void createCheckDB();
        void createDetailedDB();



        //Helping functions
        std::unordered_map<std::string, double> header_map_;
        std::string S57toString(S57 s57);
        S57 stringToS57(std::string str);

        //Debugging functions
        void loadAllDatasetsFromProducer(std::string producer_code);
};