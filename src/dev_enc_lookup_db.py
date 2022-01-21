#!/usr/bin/python


from osgeo import ogr, osr, gdal
import sys, getopt, os, glob

feature_layer_names = ["LNDARE","DEPARE","OBSTRN","OFSPLF","PILPNT","PYLONS","SOUNDG","UWTROC","WRECKS","BCNSPP","BOYLAT","BRIDGE","CTNARE","FAIRWY","RESARE"]
def calculate_wgs_reference()->osr.SpatialReference:
    wgs_reference = osr.SpatialReference()
    wgs_reference.ImportFromEPSG(4326)
    return wgs_reference
def main():
    targetPattern = "*.000"
    enc_paths = glob.glob(os.path.join("./backup",targetPattern))
    print(enc_paths)
    outPoly = "./backup/testScript.sqlite"
    relevantLayers= ['DEPARE',"COALNE","LNDARE","M_COVR"]
    safeDepth = 0.1

    ds_list = []
    for enc_path in enc_paths:
        ds_list.append(gdal.OpenEx(enc_path))

    if any(ds_list)==None:
        print("Error open source file(s). Exiting.")
        sys.exit(1)
    
    driver = ogr.GetDriverByName("SQLite")

    if os.path.exists(outPoly):
        driver.DeleteDataSource(outPoly)
    out_ds = driver.CreateDataSource(outPoly)
    
    wgs_reference = osr.SpatialReference()
    wgs_reference.ImportFromEPSG(4326)
    utm_32v_reference = osr.SpatialReference()
    utm_32v_reference.SetProjCS("UTM 32 / WGS84")
    utm_32v_reference.SetWellKnownGeogCS("WGS84")
    utm_32v_reference.SetUTM(32)

    #Define layers
    layer_dict = {}
    for ds in ds_list:
        for i in range(ds.GetLayerCount()):
            layer = ds.GetLayerByIndex(i)
            if not layer.GetName() in feature_layer_names: continue 
            if out_ds.GetLayerByName(layer.GetName())==None:
                #print("Layer ", layer.GetName().upper(), " does not exist,creating")
                layer_dict[layer.GetName()] = out_ds.CreateLayer(layer.GetName(), calculate_wgs_reference(), layer.GetGeomType())

    for ds in ds_list:
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


            
        
main()