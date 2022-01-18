#!/usr/bin/python
from __future__ import annotations
from osgeo import ogr, osr, gdal
import sys, getopt, os, glob
import pandas as pd
import numpy as np
import math

from vincenty import vincenty

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



def main():
    start = (40.56638,-73.88525)
    end2 = (40.51,-73.83934)
    ds_list = load_datasources(determine_charts_to_load(start,end2))

main()