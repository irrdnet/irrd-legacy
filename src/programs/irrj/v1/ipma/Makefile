#          
# File: Makefile for IPMA tool  packages
# Modified from UARC Makefile by Jason Breslau.
#

default:
	$(MAKE) all

all:
	cd ./Help; $(MAKE) all
	cd ./Util; $(MAKE) all
	cd ./Window; $(MAKE) all

clear:
	cd ./Help; $(MAKE) clear
	cd ./Util; $(MAKE) clear
	cd ./Window; $(MAKE) clear

clean:
	$(MAKE) clear

over:
	$(MAKE) clear
	$(MAKE) all

jarfile:
	$(MAKE) all
	cp ./Help/*.class ./jar/ipma/Help/*.class
	cp ./Util/*.class ./jar/ipma/Util/*.class
	cp ./Window/*.class ./jar/ipma/Window/*.class
#	signtool -Z "mural.jar" -k "Test" -d ./.key ./jar
#	mv ./mural.jar /net/uri.si.umich.edu/usr/private/apache-1.1.3/htdocs

