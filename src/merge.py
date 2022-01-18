#!/usr/bin/python


from osgeo import ogr, osr, gdal
import sys, getopt, os, glob

def main():
    targetPattern = "*.000"
    enc_paths = glob.glob(os.path.join("./backup",targetPattern))
    print(enc_paths)
    outPoly = "mergedHazards"
    relevantLayers= ['DEPARE',"COALNE","LNDARE"]
    safeDepth = 0.1

    ds_list = []
    for enc_path in enc_paths:
        ds_list.append(gdal.OpenEx(enc_path))

    if any(ds_list)==None:
        print("Error open source file(s). Exiting.")
        sys.exit(1)
    
    driver = ogr.GetDriverByName("ESRI Shapefile")

    if os.path.exists(outPoly):
        driver.DeleteDataSource(outPoly)
    outDSPoly = driver.CreateDataSource(outPoly)
    
    wgs_reference = osr.SpatialReference()
    wgs_reference.ImportFromEPSG(4326)
    utm_32v_reference = osr.SpatialReference()
    utm_32v_reference.SetProjCS("UTM 32 / WGS84")
    utm_32v_reference.SetWellKnownGeogCS("WGS84")
    utm_32v_reference.SetUTM(32)

    outLayerPoly = outDSPoly.CreateLayer(outPoly.split(".")[0], wgs_reference, ogr.wkbPolygon)

    for layername in relevantLayers:
        for ds in ds_list:
            layer = ds.GetLayerByName(layername)
            if layer==None:
                print("Did not find layer: ", layername)
                sys.exit(1)

            # Keep only Geometry areas less than "safeDepth" meter
            if (layername == "DEPARE"):
                layer.SetAttributeFilter("DRVAL1 < %d" % safeDepth)

            for feat in layer:
                geom = feat.GetGeometryRef()
                if (geom.GetGeometryName() == "POLYGON"):
                    outFeatDefn = outLayerPoly.GetLayerDefn()
                    outFeat = ogr.Feature(outFeatDefn)
                    outFeat.SetGeometry(geom)
                    outLayerPoly.CreateFeature(outFeat)

main()