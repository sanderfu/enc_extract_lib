#include "enc_extraction/enc_extract_lib.h"

ENCExtractor::ENCExtractor(std::string path, extractorRegion& r, extractorVessel& v):
path_(path),
region_(r),
vessel_(v){
    GDALAllRegister();
    GDALDriver* driver_sqlite = GetGDALDriverManager()->GetDriverByName("SQLite");
    if(driver_sqlite==NULL){
        throw std::runtime_error("Unable to find SQLite driver");
        return;
    }
    std::string check_path = path_+"/data/check_db.sqlite";
    check_db_ = driver_sqlite->Create(check_path.c_str(),0,0,0,GDT_Unknown,NULL);
    check_db_->CreateLayer("COLLISION", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    check_db_->CreateLayer("CAUTION", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    
}

void ENCExtractor::loadDatasets(std::vector<std::string> enc_paths){
    for (auto path_it=enc_paths.begin(); path_it!=enc_paths.end(); path_it++){
        datasets_.push_back((GDALDataset*) GDALOpenEx((*path_it).c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL));
        if((*(datasets_.end()-1))==NULL){
            throw std::runtime_error("Failed to load datasource: "+*path_it);
        }
    } 
   return;
}

bool ENCExtractor::pointInRegion(double lon, double lat, extractorRegion& r){
    return lon>=r.min_lon && lat>=r.min_lat && lon<=r.max_lon && lat<=r.max_lat;
}

std::vector<std::string> ENCExtractor::determineChartsToLoad(extractorRegion& r){
    std::vector<std::string> enc_paths;
    std::ifstream index_file;
    index_file.open(path_+"/registered/index.txt");
    if(!index_file){
        throw std::runtime_error("Unable to open ENC index file");
        exit(1);
    }
    std::string line;
    bool header=true;
    while(std::getline(index_file,line)){
        if(header){
            std::vector<std::string> index_header;
            boost::algorithm::split(index_header, line, boost::is_any_of(","));
            for (int i=0; i<index_header.size(); i++){
                header_map_.insert(std::make_pair(index_header[i],i));
            }
            header=false;
        } else{
            std::vector<std::string> row;
            boost::algorithm::split(row, line, boost::is_any_of(","));
            if (pointInRegion(std::stod(row[header_map_["min_long"]]),std::stod(row[header_map_["min_lat"]]),r)){
                enc_paths.push_back(path_+"/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["min_long"]]),std::stod(row[header_map_["max_lat"]]),r)){
                enc_paths.push_back(path_+"/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["max_long"]]),std::stod(row[header_map_["max_lat"]]),r)){
                enc_paths.push_back(path_+"/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["max_long"]]),std::stod(row[header_map_["min_lat"]]),r)){
                enc_paths.push_back(path_+"/registered/"+row[header_map_["filename"]]);
            }
        }
    }
    return enc_paths;
}

void ENCExtractor::manipulateLayer(OGRLayer* layer, bool verbose){
    switch (stringToS57(layer->GetName()))
    {
    case S57::LNDARE:
        if (verbose) std::cout << "Layer is LNDARE, which require no manipulation" <<std::endl;
        break;
    case S57::DEPARE:
        if (verbose) std::cout << "Layer is DEPARE, only keep features where ship risks grounding" << std::endl;
        layer->SetAttributeFilter(("DRVAL1 < " + std::to_string(vessel_.draft+vessel_.draft_safety_margin)).c_str());
        break;
    case S57::OBSTRN:
        layer->SetAttributeFilter(("EXPSOU = 2 and VALSOU < "+std::to_string(vessel_.draft+vessel_.draft_safety_margin)).c_str());
        break;
    case S57::BRIDGE:
        layer->SetAttributeFilter(("VERCLR<"+std::to_string(vessel_.height+vessel_.height_safety_margin)).c_str());
        break;
    default:
        break;
    }
   return;
}

bool ENCExtractor::addFeature(OGRLayer* layer, OGRFeature* feature, Mode mode){
    if (stringToS57(layer->GetName())==S57::RESARE){
        if (feature->IsFieldSet(feature->GetFieldIndex("RESTRN"))){
            int restrn_id = feature->GetFieldAsInteger64("RESTRN");
            if (mode == Mode::COLLISION && (restrn_id!=7 && restrn_id!=14)) return false;
            else if(mode == Mode::CAUTION && (restrn_id!=8 && restrn_id!=13 && restrn_id!=27)) return false;
        }
        if (feature->IsFieldSet(feature->GetFieldIndex("CATREA"))){
            int catrea_id = feature->GetFieldAsInteger64("CATREA");
            if(mode==Mode::COLLISION && (catrea_id!=9 && catrea_id!=12 && catrea_id!=14)) return false;
            else if(mode==Mode::CAUTION && (catrea_id!=24)) return false;
        }
    }
    return true;
}

void ENCExtractor::addFeaturesToLayer(OGRLayer* in_layer,OGRLayer* out_layer, Mode mode){
    manipulateLayer(in_layer);
    OGRFeature* feat;
    in_layer->ResetReading();
    while((feat = in_layer->GetNextFeature()) != NULL){
        if(!addFeature(in_layer,feat,mode)) continue;
        OGRGeometry* geom = feat->GetGeometryRef();
        if(std::string(geom->getGeometryName())=="POINT"){
            OGRGeometry* geom_buffered = geom->Buffer(0.00001*(vessel_.length+vessel_.horisontal_safety_margin)); //0.00001 is approx 1.11 m
            OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
            new_feat->SetGeometry(geom_buffered);
            out_layer->CreateFeature(new_feat);
        } else if (std::string(geom->getGeometryName())=="POLYGON"){
            OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
            new_feat->SetGeometry(geom);
            out_layer->CreateFeature(new_feat);
        }
        OGRFeature::DestroyFeature(feat);
    }
}

void ENCExtractor::extractCollisionFeatures(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRLayer* out_layer = out_ds->GetLayerByName("collision");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    addFeaturesToLayer(in_layer,out_layer,Mode::COLLISION);
}

void ENCExtractor::extractCautionFeatures(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRLayer* out_layer = out_ds->GetLayerByName("caution");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    addFeaturesToLayer(in_layer,out_layer,Mode::CAUTION);
}

void ENCExtractor::extractFeature(std::string layername, GDALDataset* in_ds, GDALDataset* out_ds){
    OGRLayer* in_layer = in_ds->GetLayerByName(layername.c_str());
    if(in_layer==NULL){
        return;
    }
    if(std::find(collision_features.begin(),collision_features.end(),layername)!=collision_features.end()){
        extractCollisionFeatures(in_layer,out_ds);
    }
    if(std::find(caution_features.begin(),caution_features.end(),layername)!=caution_features.end()){
       extractCautionFeatures(in_layer,out_ds);
    }
}

void ENCExtractor::dissolveLayer(OGRLayer* in_layer, GDALDataset* in_ds, GDALDataset* out_ds){
    OGRLayer* out_dissolved = out_ds->CreateLayer((std::string(in_layer->GetName())+"_DISSOLVED").c_str(),in_layer->GetSpatialRef(),wkbPolygon);
    OGRMultiPolygon multi;
    OGRFeature* feat;
    in_layer->ResetReading();
    while((feat=in_layer->GetNextFeature())!=NULL){
        feat->GetGeometryRef()->closeRings();
        multi.addGeometry(feat->GetGeometryRef());
        OGRFeature::DestroyFeature(feat);
    }
    OGRMultiPolygon* union_geom = multi.UnionCascaded()->toMultiPolygon();
    for(auto poly: union_geom){
        OGRFeature* out_feat = OGRFeature::CreateFeature(out_dissolved->GetLayerDefn());
        out_feat->SetGeometry(poly);
        out_dissolved->CreateFeature(out_feat);
    }
}

void ENCExtractor::createCheckDB(){
    for (auto ds_it=datasets_.begin(); ds_it!=datasets_.end(); ds_it++){
        for (auto feature_it=feature_layer_names.begin(); feature_it!=feature_layer_names.end(); feature_it++){
            extractFeature(*feature_it,*ds_it,check_db_);
        }
    }
    dissolveLayer(check_db_->GetLayerByName("collision"),check_db_,check_db_);
    dissolveLayer(check_db_->GetLayerByName("caution"),check_db_,check_db_);
}



//Helping functions
S57 ENCExtractor::stringToS57(std::string str){
    auto string_it = std::find(feature_layer_names.begin(),feature_layer_names.end(),str);
    return static_cast<S57>(string_it-feature_layer_names.begin());
}