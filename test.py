from osgeo import ogr, osr, gdal
import sys, getopt, os

def main(argv):
    driver = ogr.GetDriverByName("ESRI Shapefile")
    file = "./polyHazards/polyHazards.shp"
    dataSource = driver.Open(file,0)
    if dataSource is None:
        print("Could not open ",file)
        sys.exit(1)
    

if __name__ == "__main__":
    main(sys.argv[1:])
