from osgeo import ogr, osr, gdal
import sys, getopt, os

def main(argv):
    ds = gdal.OpenEx("US5NYCBH.000")
    if(ds == None):
        print("Error open source file. Exiting.")
        sys.exit(1)
    dsid_layer = ds.GetLayerByName("DSID")
    #print(dsid_layer.GetFeature(0).GetFieldDefnRef("DSID_INTU").name)
    print(dsid_layer.GetFeature(0).GetFieldAsString("DSID_ISDT"))
    test_layer = ds.GetLayerByName("M_COVR")
    m_covr_feat = test_layer.GetNextFeature()
    #print(m_covr_feat.GetGeometryRef())
    print(test_layer.GetExtent())
    

if __name__ == "__main__":
    main(sys.argv[1:])
