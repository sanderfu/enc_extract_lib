#!/usr/bin/python
from __future__ import annotations
from osgeo import ogr, osr, gdal
import sys, getopt, os, glob
import pandas as pd
import numpy as np
import math
from enum import Enum

from vincenty import vincenty

feature_layer_names = ["LNDARE","DEPARE","OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE"]
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

def S57_to_str(s57:S57)->str:
    return feature_layer_names[s57]

def str_to_S57(string:str)->S57:
    return S57(feature_layer_names.index(string))

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

            


def add_features_to_layer(in_layer:ogr.Layer,out_layer:ogr.Layer):
    manipulate_layer(in_layer,verbose=True)
    for feature in in_layer:
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

def extract_collision_features(layername:str, in_ds:ogr.DataSource, out_ds:ogr.DataSource):
    """
    Extracts all geometry features from the LNDARE layer 
    in the ENC to the collision layer in the target shapefile
    """
    in_layer = in_ds.GetLayerByName(layername)
    out_layer = out_ds.GetLayerByName("COLLI2")

    if in_layer==None:
        print(layername, "not in file")
        #Not an issue, the tile can be all ocean, in that case there is no LNDARE feature
        return
    elif out_layer==None:
        raise RuntimeError("Collision layer not defined in target datasource")

    add_features_to_layer(in_layer,out_layer)
        

def calculate_wgs_reference()->osr.SpatialReference:
    wgs_reference = osr.SpatialReference()
    wgs_reference.ImportFromEPSG(4326)
    return wgs_reference

def main():
    start = (40.56638,-73.88525)
    end2 = (40.51,-73.83934)
    #ds_list = load_datasources(determine_charts_to_load(start,end2))
    ds_list = get_all_datasources()

    outPoly = "testMerge"
    driver = ogr.GetDriverByName("ESRI Shapefile")
    if os.path.exists(outPoly):
        driver.DeleteDataSource(outPoly)
    outDSPoly = driver.CreateDataSource(outPoly)

   
    outLayerPoly = outDSPoly.CreateLayer("COLLI2", calculate_wgs_reference(), ogr.wkbPolygon)
    for ds in ds_list:
        for featurename in feature_layer_names:
            extract_collision_features(featurename,ds,outDSPoly)
    


main()