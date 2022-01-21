#!/usr/bin/python
from __future__ import annotations
from osgeo import ogr, osr, gdal
import sys, getopt, os, glob
import pandas as pd
import numpy as np
import math
from enum import Enum
from vincenty import vincenty
import time

feature_layer_names = ["LNDARE","DEPARE","OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE","CTNARE","FAIRWY","RESARE"]
collision_features  = ["LNDARE","DEPARE","OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE"]
caution_features = ["CTNARE","FAIRWY","RESARE","BCNSPP","BOYLAT","OBSTRN","OFSPLF","PILPNT","PYLONS"]
class S57(Enum):
    LNDARE = 0
    DEPARE = 1
    OBSTRN = 2
    OFSPLF = 3
    PILPNT = 4
    PYLONS = 5
    SOUNDG = 6
    UWTROC = 7
    WRECKS = 8
    BCNSPP = 9
    BOYLAT = 10
    BRIDGE = 11
    CTNARE = 12
    FAIRWY = 13
    RESARE = 14

class Mode(Enum):
    COLLISION = 1
    CAUTION = 2

#Layers that must be duplicated as they are relevant for both collision and caution depending in fields
duplicate_layers = ["RESARE"]

def S57_to_str(s57:S57)->str:
    return feature_layer_names[s57]

def str_to_S57(string:str)->S57:
    mod_string = string.split(sep="_")[0]
    return S57(feature_layer_names.index(mod_string))

def load_datasources(enc_paths: str, verbose=False)->list[ogr.DataSource]:
    datasources = []
    for enc_path in enc_paths:
        datasources.append(gdal.OpenEx(enc_path))
    if None in datasources:
        raise RuntimeError("Failed to load all datasources",enc_paths)
    return datasources


def determine_charts_to_load(min_ll:tuple(float,float),max_ll:tuple(float)) -> str:
    """
    Determine which charts to load

    Remember that latitude (N/S) comes first, then longitude (E/W) in tuples
    """
    index_df = pd.read_csv("./registered/index.txt")

    #Calculate distance to all tiles from min
    smallest_distance_min = math.inf
    smallest_distance_max = math.inf

    tile_min = None
    tile_max = None

    for k,row in index_df.iterrows():
        distance_min = np.sqrt(pow(row["center_lat"]-min_ll[0],2)+pow(row["center_long"]-min_ll[1],2))
        distance_max = np.sqrt(pow(row["center_lat"]-max_ll[0],2)+pow(row["center_long"]-max_ll[1],2))

        if smallest_distance_min>distance_min:
            smallest_distance_min = distance_min
            tile_min = row
        if smallest_distance_max>distance_max:
            smallest_distance_max = distance_max
            tile_max = row

    #Find all columns between(including ends)
    to_load_df = index_df.loc[((index_df["min_long"]<=tile_max["min_long"]) & (index_df["min_lat"]<=tile_max["min_lat"]) & (index_df["min_long"]>=tile_min["min_long"]) & (index_df["min_lat"]>=tile_min["min_lat"]))]

    storage_path = "./registered"
    enc_paths = []
    for k, row in to_load_df.iterrows():
        enc_paths.append(os.path.join(storage_path,row["filename"]))
    
    return enc_paths

def get_all_datasources() -> list[ogr.DataSource]:
    """
    For testing/debugging purposes
    """
    start = (-90,-90)
    end2 = (90,90)
    return load_datasources(determine_charts_to_load(start,end2))

def manipulate_layer(in_layer:ogr.Layer,verbose=False):
    safe_depth = 0.5 #TODO: When this entire driver becomes a class, move it to there and load from config yaml.
    safe_height = 3.5 #TODO: When this entire driver becomes a class, move it to there and load from config yaml.
    if str_to_S57(in_layer.GetName())==S57.LNDARE:
        if verbose: print("Layer is LNDARE, this does not neeed to be manipulated")
    elif str_to_S57(in_layer.GetName())==S57.DEPARE:
        if verbose: print("Layer is DEPARE, only keep features where ship risks grounding")
        in_layer.SetAttributeFilter(f"DRVAL1 < {safe_depth}")
    elif str_to_S57(in_layer.GetName())==S57.OBSTRN:
        #Filter out all obstructions that are not shallower than depth of area
        in_layer.SetAttributeFilter(f"EXPSOU = 2 and VALSOU < {safe_depth}")
    elif str_to_S57(in_layer.GetName())==S57.BRIDGE:
        in_layer.SetAttributeFilter(f"VERCLR<{safe_height}")

def add_feature(in_layer: ogr.Layer, feature: ogr.Feature, mode: Mode)->bool:
    """
    Function for checking if a feature should be added or not.
    For cases where an attribute filter can not be used (such as for RESARE)
    """
    if str_to_S57(in_layer.GetName())==S57.RESARE:
        if feature.IsFieldSet("RESTRN"):
            restrn_id = feature.GetFieldAsInteger("RESTRN")
            if mode == Mode.COLLISION and restrn_id not in [7,14]:
                return False
            elif mode == Mode.CAUTION and restrn_id not in [8,13,27]:
                return False
        if feature.IsFieldSet("CATREA"):
            catrea_id = feature.GetFieldAsInteger("CATREA")
            if mode == Mode.COLLISION and catrea_id not in [9,12,14]:
               return False
            if mode == Mode.CAUTION and catrea_id not in [24]:
               return False
    return True

def add_features_to_layer(in_layer:ogr.Layer,out_layer:ogr.Layer, mode:Mode):
    manipulate_layer(in_layer,verbose=False)
    for feature in in_layer:
        if(not add_feature(in_layer,feature,mode)): continue
        geom = feature.GetGeometryRef()
        if (geom.GetGeometryName() == "POINT"):
            #Has to make a polygon with the center and length of the the area the point describes
            geom_poly = geom.Buffer(0.0001) #TODO Find out if can do better. VERLEN Field not set for OBSTRN
            outFeatDefn = out_layer.GetLayerDefn()
            outFeat = ogr.Feature(outFeatDefn)
            outFeat.SetGeometry(geom_poly)
            out_layer.CreateFeature(outFeat)
        elif (geom.GetGeometryName() == "POLYGON"):
            outFeatDefn = out_layer.GetLayerDefn()
            outFeat = ogr.Feature(outFeatDefn)
            outFeat.SetGeometry(geom)
            out_layer.CreateFeature(outFeat)
    in_layer.ResetReading()

def extract_collision_features(layername:str, in_ds:ogr.DataSource, out_ds:ogr.DataSource):
    """
    Extracts all geometry features from the collision relevant layers
    in the ENC to the collision layer in the target shapefile
    """
    
    in_layer = in_ds.GetLayerByName(layername)
    out_layer = out_ds.GetLayerByName("COLLISION")

    if in_layer==None:
        #Not an issue, the tile can be all ocean, in that case there is no LNDARE feature
        return
    elif out_layer==None:
        raise RuntimeError("Collision layer not defined in target datasource")

    add_features_to_layer(in_layer,out_layer,Mode.COLLISION)

def extract_caution_features(layername:str, in_ds:ogr.DataSource, out_ds:ogr.DataSource):
    in_layer = in_ds.GetLayerByName(layername)
    out_layer = out_ds.GetLayerByName("CAUTION")
    if in_layer==None:
        #Not an issue, the tile can be all ocean, in that case there is no LNDARE feature
        return
    elif out_layer==None:
        raise RuntimeError("Collision layer not defined in target datasource")
    add_features_to_layer(in_layer,out_layer,Mode.CAUTION)

def extract_feature(layername:str, in_ds:ogr.DataSource, out_ds:ogr.DataSource):
    if layername in collision_features:
        extract_collision_features(layername,in_ds,out_ds)
    if layername in caution_features:
        extract_caution_features(layername,in_ds,out_ds)

def calculate_wgs_reference()->osr.SpatialReference:
    wgs_reference = osr.SpatialReference()
    wgs_reference.ImportFromEPSG(4326)
    return wgs_reference

def dissolve_layer(in_poly:ogr.Layer, out_ds:ogr.DataSource, multipoly=False)->ogr.Layer:
    """
    Disclaimer: This funtion is based on the core method in dissolve.py in map-extraction by @olesot
    """
    out_collision_poly_dissolved = out_ds.CreateLayer(in_poly.GetName()+"_DISSOLVED", calculate_wgs_reference(), ogr.wkbPolygon)
    defn = out_collision_poly_dissolved.GetLayerDefn()
    multi = ogr.Geometry(ogr.wkbMultiPolygon)
    for feat in in_poly:
        if feat.geometry():
            #feat.SetGeometry(feat.geometry().Buffer(0.001,10))
            feat.geometry().CloseRings() # this copies the first point to the end
            wkt = feat.geometry().ExportToWkt()
            multi.AddGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    union = multi.UnionCascaded()
    if not multipoly:
        for geom in union:
            poly = ogr.CreateGeometryFromWkb(geom.ExportToWkb())
            feat = ogr.Feature(defn)
            feat.SetGeometry(poly)
            out_collision_poly_dissolved.CreateFeature(feat)
    else:
        out_feat = ogr.Feature(defn)
        out_feat.SetGeometry(union)
        out_collision_poly_dissolved.CreateFeature(out_feat)

    #Delete original layers
    return out_collision_poly_dissolved

def create_lookup_database(lookup_db:ogr.DataSource, enc_ds_list:list[ogr.DataSource]):
    #Define layers
    layer_dict = {}
    for ds in enc_ds_list:
        for i in range(ds.GetLayerCount()):
            layer = ds.GetLayerByIndex(i)
            if not layer.GetName() in feature_layer_names: continue 
            if lookup_db.GetLayerByName(layer.GetName())==None:
                #print("Layer ", layer.GetName().upper(), " does not exist,creating")
                layer_dict[layer.GetName()] = lookup_db.CreateLayer(layer.GetName(), calculate_wgs_reference(), layer.GetGeomType())

    for ds in enc_ds_list:
        for i in range(ds.GetLayerCount()):
            layer = ds.GetLayerByIndex(i)
            if not layer.GetName() in feature_layer_names: continue
            for feature in layer:
                defn = layer_dict[layer.GetName()].GetLayerDefn()
                wkt = feature.geometry().ExportToWkt()
                append_feat = ogr.Feature(defn)
                poly = ogr.CreateGeometryFromWkt(wkt)
                append_feat.SetGeometry(poly)
                layer_dict[layer.GetName()].CreateFeature(append_feat)
    
def create_check_db(check_db:ogr.DataSource, enc_ds_list:list[ogr.DataSource]):
    for ds in enc_ds_list:
        for featurename in feature_layer_names:
            extract_feature(featurename,ds,check_db)
    
    #Dissolve collision layer or make multipolygon of it
    for i in range(check_db.GetLayerCount()):
        dissolve_layer(check_db.GetLayerByIndex(i),check_db)
    
    check_db.DeleteLayer("collision")
    check_db.DeleteLayer("caution")


def main():
    start = (40.56638,-73.88525)
    end2 = (40.51,-73.83934)
    #ds_list = load_datasources(determine_charts_to_load(start,end2))
    ds_list = get_all_datasources()

    outPoly = "./databases/check_db.sqlite"
    driver = ogr.GetDriverByName("SQLite")
    if os.path.exists(outPoly):
        driver.DeleteDataSource(outPoly)
    outDSPoly = driver.CreateDataSource(outPoly)
   
    out_collision_poly = outDSPoly.CreateLayer("COLLISION", calculate_wgs_reference(), ogr.wkbPolygon)
    out_caution_poly = outDSPoly.CreateLayer("CAUTION", calculate_wgs_reference(), ogr.wkbPolygon)
    create_check_db(outDSPoly,ds_list)

    lookup_path = "./databases/lookup_db.sqlite"
    lookup_ds = driver.CreateDataSource(lookup_path)
    create_lookup_database(lookup_ds,ds_list)
    
    #for ds in ds_list:
    #    for featurename in feature_layer_names:
    #        extract_feature(featurename,ds,outDSPoly)
    #
    ##Dissolve collision layer or make multipolygon of it
    #dissolve_layer(out_collision_poly,outDSPoly)
    #dissolve_layer(out_caution_poly,outDSPoly)
main()