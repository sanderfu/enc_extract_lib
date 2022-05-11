#include "enc_extraction/enc_extract_lib.h"

ENCExtractor::ENCExtractor(extractorRegion& r, extractorVessel& v,GDALDataset* check_db):
region_(r),
vessel_(v),
check_db_(check_db){
    GDALAllRegister();
    check_db_->CreateLayer("COLLISION", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    check_db_->CreateLayer("CAUTION", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    check_db_->CreateLayer("mission_region", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    check_db_->CreateLayer("MAP_COVERAGE", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);
    check_db_->CreateLayer("UNKNOWN", OGRSpatialReference::GetWGS84SRS(), wkbPolygon);

    
    loadDatasets(determineChartsToLoad(region_));
    std::vector<std::string> dataset_strings = determineChartsToLoad(region_);
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
    return lon>=r.min_lon_ && lat>=r.min_lat_ && lon<=r.max_lon_ && lat<=r.max_lat_;
}

std::vector<std::string> ENCExtractor::determineChartsToLoad(extractorRegion& r){
    std::vector<std::string> enc_paths;
    std::ifstream index_file;
    index_file.open("/home/sanderfu/catkin_ws/src/enc-extraction/registered/index.txt");
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
                enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["min_long"]]),std::stod(row[header_map_["max_lat"]]),r)){
                enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["max_long"]]),std::stod(row[header_map_["max_lat"]]),r)){
                enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["max_long"]]),std::stod(row[header_map_["min_lat"]]),r)){
                enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
            } else if (pointInRegion(std::stod(row[header_map_["mid_lon"]]),std::stod(row[header_map_["mid_lat"]]),r)){
                enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
            } else {
                //Check if region corners or center in ENC coverage
                extractorRegion enc_region(std::stod(row[header_map_["min_long"]]),std::stod(row[header_map_["min_lat"]]),std::stod(row[header_map_["max_long"]]),std::stod(row[header_map_["max_lat"]]));
                if (pointInRegion(r.min_lon_,r.min_lat_,enc_region)){
                    enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
                } else if (pointInRegion(r.min_lon_,r.max_lat_,enc_region)){
                    enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
                } else if (pointInRegion(r.max_lon_,r.max_lat_,enc_region)){
                    enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
                } else if (pointInRegion(r.max_lon_,r.min_lat_,enc_region)){
                    enc_paths.push_back("/home/sanderfu/catkin_ws/src/enc-extraction/registered/"+row[header_map_["filename"]]);
                }
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
        layer->SetAttributeFilter(("DRVAL1 < " + std::to_string(vessel_.draft_)).c_str());
        break;
    case S57::OBSTRN:
        layer->SetAttributeFilter(("EXPSOU = 2 and VALSOU < "+std::to_string(vessel_.draft_)).c_str());
        break;
    case S57::BRIDGE:
        layer->SetAttributeFilter(("VERCLR<"+std::to_string(vessel_.height_)).c_str());
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
            if (mode == Mode::COLLISION_M && (restrn_id!=7 && restrn_id!=14)) return false;
            else if(mode == Mode::CAUTION_M && (restrn_id!=8 && restrn_id!=13 && restrn_id!=27)) return false;
        }
        if (feature->IsFieldSet(feature->GetFieldIndex("CATREA"))){
            int catrea_id = feature->GetFieldAsInteger64("CATREA");
            if(mode==Mode::COLLISION_M && (catrea_id!=9 && catrea_id!=12 && catrea_id!=14)) return false;
            else if(mode==Mode::CAUTION_M && (catrea_id!=24)) return false;
        }
    }
    return true;
}

void ENCExtractor::addFeaturesToLayer(OGRLayer* in_layer,OGRLayer* out_layer, Mode mode){
    manipulateLayer(in_layer);
    OGRFeature* feat;
    in_layer->ResetReading();

    OGRLayer* clipping_layer = check_db_->GetLayerByName("mission_region");
    clipping_layer->ResetReading();
    OGRFeature* clipping_feat = clipping_layer->GetNextFeature();
    OGREnvelope region_env;
    clipping_feat->GetGeometryRef()->getEnvelope(&region_env);

    OGREnvelope geom_env;


    while((feat = in_layer->GetNextFeature()) != NULL){
        if(!addFeature(in_layer,feat,mode)) continue;
        OGRGeometry* geom = feat->GetGeometryRef();
        geom->getEnvelope(&geom_env);
        if(!geom_env.Intersects(region_env)) continue;
        if(std::string(geom->getGeometryName())=="POINT" || std::string(geom->getGeometryName())=="LINESTRING"){
            OGRGeometry* geom_buffered = geom->Buffer(0.00001*(vessel_.length_)); //0.00001 is approx 1.11 m
            OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
            //OGRGeometry* geom_buffered_in_region = geom_buffered->Intersection(clipping_feat->GetGeometryRef());
            new_feat->SetGeometry(geom_buffered);
            out_layer->CreateFeature(new_feat);
        } else if (std::string(geom->getGeometryName())=="POLYGON"){
            OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
            //OGRGeometry* geom_in_region = geom->Intersection(clipping_feat->GetGeometryRef());
            new_feat->SetGeometry(geom);
            out_layer->CreateFeature(new_feat);
        } else{
            std::cout << "Unhandled geometry type: " << std::string(geom->getGeometryName()) << std::endl;
        }
        OGRFeature::DestroyFeature(feat);
    }
}

void ENCExtractor::extractCollisionFeatures(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRLayer* out_layer = out_ds->GetLayerByName("collision");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    addFeaturesToLayer(in_layer,out_layer,Mode::COLLISION_M);
}

void ENCExtractor::extractCautionFeatures(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRLayer* out_layer = out_ds->GetLayerByName("caution");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    addFeaturesToLayer(in_layer,out_layer,Mode::CAUTION_M);
}

void ENCExtractor::extractMapCoverage(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRLayer* out_layer = out_ds->GetLayerByName("map_coverage");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    addFeaturesToLayer(in_layer,out_layer,Mode::COVERAGE_M);
}

void ENCExtractor::extractUnknown(OGRLayer* in_layer, GDALDataset* out_ds){
    OGRPolygon region;
    OGRLinearRing region_ring;
    region_ring.addPointM(region_.min_lon_,region_.min_lat_,0);
    region_ring.addPointM(region_.min_lon_,region_.max_lat_,0);
    region_ring.addPointM(region_.max_lon_,region_.max_lat_,0);
    region_ring.addPointM(region_.max_lon_,region_.min_lat_,0);
    region_ring.addPointM(region_.min_lon_,region_.min_lat_,0);
    region.addRing(&region_ring);
    
    OGRMultiPolygon map_coverage;
    OGRFeature* feat;
    in_layer->ResetReading();
    while((feat=in_layer->GetNextFeature())!=NULL){
        map_coverage.addGeometry(feat->GetGeometryRef());
        OGRFeature::DestroyFeature(feat);
    }
    
    OGRPolygon* map_coverage_poly = map_coverage.Simplify(0.0001)->toPolygon(); //Ramer–Douglas–Peucker algorithm used for simplify
    OGRPolygon unknown_poly = *region.Difference(map_coverage_poly)->toPolygon();
    OGRLayer* out_layer = out_ds->GetLayerByName("unknown");
    if(out_layer==NULL){
        throw std::runtime_error("Unable to open collision layer in output dataset");
    }
    OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
    new_feat->SetGeometry(&unknown_poly);
    out_layer->CreateFeature(new_feat);
}

void ENCExtractor::extractMissionRegion(GDALDataset* out_ds){
    OGRPolygon region;
    OGRLinearRing region_ring;
    region_ring.addPointM(region_.min_lon_,region_.min_lat_,0);
    region_ring.addPointM(region_.min_lon_,region_.max_lat_,0);
    region_ring.addPointM(region_.max_lon_,region_.max_lat_,0);
    region_ring.addPointM(region_.max_lon_,region_.min_lat_,0);
    region_ring.addPointM(region_.min_lon_,region_.min_lat_,0);
    region.addRing(&region_ring);

    OGRLayer* out_layer = out_ds->GetLayerByName("mission_region");
    OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
    new_feat->SetGeometry(&region);
    out_layer->CreateFeature(new_feat);
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
    if(layername=="M_COVR"){
        extractMapCoverage(in_layer,out_ds);
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
    std::cout << "Dissolve layer waiting on unioncascaded" << std::endl;
    OGRMultiPolygon* union_geom = multi.UnionCascaded()->toMultiPolygon();
    std::cout << "Union cascaded done" << std::endl;
    for(auto poly: union_geom){
        OGRFeature* out_feat = OGRFeature::CreateFeature(out_dissolved->GetLayerDefn());
        out_feat->SetGeometry(poly);
        out_dissolved->CreateFeature(out_feat);
    }
}

void ENCExtractor::clipLayer(OGRLayer* in_layer, OGRLayer* clipping_layer, GDALDataset* out_ds){
    clipping_layer->ResetReading();
    OGRFeature* clipping_feat = clipping_layer->GetNextFeature();

    OGRLayer* out_layer = out_ds->CreateLayer((std::string(in_layer->GetName())+"_clipped").c_str(),in_layer->GetSpatialRef(),wkbPolygon);
    in_layer->ResetReading();
    OGRFeature* feat;
    while((feat = in_layer->GetNextFeature()) != NULL){
        OGRGeometry* geom = feat->GetGeometryRef()->Intersection(clipping_feat->GetGeometryRef());
        OGRFeature* new_feat = OGRFeature::CreateFeature(out_layer->GetLayerDefn());
        new_feat->SetGeometry(geom);
        out_layer->CreateFeature(new_feat);
        OGRFeature::DestroyFeature(feat);
    }

}

void ENCExtractor::run(){
    extractMissionRegion(check_db_);
    for (auto ds_it=datasets_.begin(); ds_it!=datasets_.end(); ds_it++){
        for (auto feature_it=feature_layer_names.begin(); feature_it!=feature_layer_names.end(); feature_it++){
            extractFeature(*feature_it,*ds_it,check_db_);
        }

        for( auto&& poLayer: (*ds_it)->GetLayers() )
        {
            layernames_.insert(poLayer->GetName());
        }   

    }

    //Print all layers existing inn all included DBs
    /*
    for(auto it=layernames_.begin(); it!= layernames_.end(); it++){
        std::cout << (*it) << std::endl;
    }
    */

    std::cout << "Extract unknown" << std::endl;
    extractUnknown(check_db_->GetLayerByName("map_coverage"),check_db_);
    std::cout << "Extract unknown sucess" << std::endl;
    dissolveLayer(check_db_->GetLayerByName("collision"),check_db_,check_db_);
    clipLayer(check_db_->GetLayerByName("collision_dissolved"),check_db_->GetLayerByName("mission_region"),check_db_);
    std::cout << "Dissolve collision success" << std::endl;
    dissolveLayer(check_db_->GetLayerByName("caution"),check_db_,check_db_);
    std::cout << "Dissolve caution success" << std::endl;
}



//Helping functions
S57 ENCExtractor::stringToS57(std::string str){
    auto string_it = std::find(feature_layer_names.begin(),feature_layer_names.end(),str);
    return static_cast<S57>(string_it-feature_layer_names.begin());
}