#!/usr/bin/python

"""
Generate a seed db for geoidx

Currently a *lot* of hardcoded information
Also, requires the 'mapscript' module (http://www.mapserver.org)
dbf hack requires dbview (will fix)
"""

import mapscript
import sys
import os
import subprocess

def usage():
	print "USAGE: %s filename" % sys.argv[0]
	sys.exit(1)

if len(sys.argv) != 2:
	usage()

if os.access(sys.argv[1] + ".shp", os.F_OK):
	shp = mapscript.shapefileObj(sys.argv[1], -1)
	if not os.access(sys.argv[1] + ".dbf", os.F_OK):
		print "Error accessing associated dbf?"
		sys.exit(2)
	dbf = sys.argv[1] + ".dbf"
elif os.access(sys.argv[1][:-4] + ".shp", os.F_OK):
	shp = mapscript.shapefileObj(sys.argv[1][:-4] + ".shp", -1)
	if not os.access(sys.argv[1][:-4] + ".dbf", F_OK):
		print "Error accessing associated dbf?"
		sys.exit(2)
	dbf = sys.argv[1][:-4] + ".dbf"
else:
	print "Error accessing shapefile \"" + sys.argv[1] + "\""

shape = mapscript.shapeObj(-1)
ofile = open("geoidx.db", "w")
#HACK
db = subprocess.Popen(['dbview', dbf], stdout=subprocess.PIPE).stdout

for i in range(shp.numshapes):
	shp.get(i, shape)

	# Index entry
	ofile.write("geoidx:    " + str(i) + "\n")

	# Country
	# HACK!!
	db.readline() # Cat
	db.readline() # Fips cntry
	country = db.readline().split(': ')[-1].strip()
	db.readline() # Area
	db.readline() # Pop cntry
	db.readline() # empty

	ofile.write("country:   %s\n" % (country))

	# Bounding rectangle
	ofile.write("rectangle: (%f, %f) (%f, %f)\n" % (
		shape.bounds.minx,
		shape.bounds.miny,
		shape.bounds.maxx,
		shape.bounds.maxy))

	# Coordinates
	for j in range(shape.numlines):
		part = shape.get(j)
		ofile.write("coordinates: ")
		for k in range(part.numpoints):
			point = part.get(k)
			ofile.write("(%f, %f) " % (point.x, point.y))
		ofile.write("\n")
	
	ofile.write("\n")

print "Successfully seeded geoidx.db with %d shapes" % (i + 1)
