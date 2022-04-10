#!/usr/bin/python
#Description: The purpose of this script is to ingest .000 ENC files, register them for easy lookup and store them ina defined folder structure

from osgeo import ogr, osr, gdal
import sys, getopt, os, glob, csv

def main():
    producer_code = "NO"
    input = "./ingest"
    output = "./registered"
    targetPattern = "*.000"
    enc_paths = glob.glob(os.path.join(input,targetPattern))

    ds_list = []
    for enc_path in enc_paths:
        ds_list.append(gdal.OpenEx(enc_path))

    if any(ds_list)==None or len(ds_list)==0:
        print("Error open source file(s). Exiting.")
        sys.exit(1)
    
    column_names = ["filename","DSID_INTU","min_long","max_long","min_lat","max_lat","center_long","center_lat"]
    existing_encs = []
    with open(os.path.join(output,"index.txt"),mode="r",newline='') as index_file:
        index_reader = csv.reader(index_file)
        for row in index_reader:
            if row[0]=="filename":
                continue
            existing_encs.append(row[0])


    rows_list = []
    for i, ds in enumerate(ds_list):
        row = []

        #Check if dataset file already is registered
        if enc_paths[i].split(sep="/")[-1] in existing_encs:
            print("Warning: ",enc_paths[i].split(sep="/")[-1], " already registered. Skipping.")
            continue
        else:
            print("From: ", )
            os.rename(enc_paths[i],os.path.join(output,enc_paths[i].split(sep="/")[-1]))

        #Add filename and intended usage (INTU)
        row.append(enc_paths[i].split(sep="/")[-1])
        row.append(ds.GetLayerByName("DSID").GetFeature(0).GetFieldAsString("DSID_INTU"))

        #Add latitude and longitude of the area extent
        row.extend(ds.GetLayerByName("M_COVR").GetExtent())

        #Add center long/lat
        row.append(row[2]+(row[3]-row[2])/2)
        row.append(row[4]+(row[5]-row[4])/2)

        print(row)

        rows_list.append(row)
    
    existing_encs = []
    with open(os.path.join(output,"index.txt"),mode="r",newline='') as index_file:
        index_reader = csv.reader(index_file)
        for row in index_reader:
            if row[0]=="filename":
                continue
            existing_encs.append(row[0])

    with open(os.path.join(output,"index.txt"),mode="a") as index_file:
        index_writer = csv.writer(index_file)
        index_writer.writerows(rows_list)
    



    




main()