#          
# File: Makefile for the ?? package
#
# arguments:
#   JC - the java compiler
#   JCFLAGS - options to the java compiler
#

JCFLAGS = -deprecation
JC = javac

# get the filenames for this subdirectory of the project
# the include file must define ALLCLASSFILES
#
INCFL = Makefile.filelist
include $(INCFL)

all:
	$(MAKE) $(ALLCLASSFILES)

.SUFFIXES: .java .class

.java.class:
	$(JC) $(JCFLAGS) $<

default:
	$(MAKE) all

clear:
	/bin/rm -f *.class
	/bin/rm -f *~
	/bin/rm -f #*#

clean:
	$(MAKE) clear

