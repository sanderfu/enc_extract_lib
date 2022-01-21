from __future__ import annotations
from osgeo import ogr, osr, gdal
import sys, getopt, os, glob
import pandas as pd
import numpy as np
import math
from enum import Enum
from vincenty import vincenty
import time

def main():
    targetPattern = "*.000"
    enc_paths = glob.glob(os.path.join("./backup",targetPattern))
    print(enc_paths)
    outPoly = "mergedHazards"
    relevantLayers= ['DEPARE',"COALNE","LNDARE","M_COVR"]
    safeDepth = 0.1

    ds_sqlite = gdal.OpenEx("./backup/testScript.sqlite")
    ds_enc = gdal.OpenEx("./backup/US5NY1BE.000")

    if ds_sqlite==None or ds_enc==None:
        print("Error open source file(s). Exiting.")
        sys.exit(1)

    print("ds_enc: ", ds_enc.GetLayerCount())
    print("ds_sqlite: ", ds_sqlite.GetLayerCount())

    for i in range(ds_sqlite.GetLayerCount()):
        print(ds_sqlite.GetLayerByIndex(i).GetName(), " has ", ds_sqlite.GetLayerByIndex(i).GetFeatureCount(), "features" )


    

main()