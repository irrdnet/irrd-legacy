#          
# File: Makefile for Netscape privilege classes.
#

default:
	$(MAKE) all

all:
	cd ./security; $(MAKE) all

clear:
	cd ./security; $(MAKE) clear

clean:
	$(MAKE) clear

over:
	$(MAKE) clear
	$(MAKE) all

jarfile:
	$(MAKE) all
	cp ./security/*.class ./jar/netscape/security/*.class
