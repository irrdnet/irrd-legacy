JCC=		javac
ZIP0=		zip -0 -l
ZIP9=		zip -9 -l
JAR=		jar
DOC=		javadoc

#WWW_DIR=/afs/merit.net/group/Ipma/share/Public/external_html/java
WWW_DIR=/afs/merit.net/group/ie/sushila/ipma1.1/
SIGN_JAR_DIR=/afs/merit.net/group/ie/polandj/jar2/
SIGNTOOL=/afs/merit.net/group/ie/polandj/pvt/signtool/
KEYDBDIR=/afs/merit.net/group/ie/polandj/.netscape

IPMAPACK=	\
	ipma.Help	\
	ipma.Util	\
	ipma.Window	

IPMADIRS=	\
	ipma/Help	\
	ipma/Util	\
	ipma/Window	

DIRS=	\
        ipma            \
	netscape        \
	irr		

RELEASE_DIRS =  \
	irr	

NETSCAPE_DIRS = 	\
	security

default:	all 

all:	java

over:	clean all

clean:
	for i in $(DIRS);	\
	do (	\
		cd $$i;  \
		$(MAKE) clean;  \
		cd ..; \
	); done;

wwwclean:
	for i in $(DIRS);	\
	do (	\
		echo "rm -rf $(WWW_DIR)/$$i/*.class";	\
		rm -rf $(WWW_DIR)/$$i/*.class;	\
		echo "rm -rf $(WWW_DIR)/$$i/*.zip";	\
		rm -rf $(WWW_DIR)/$$i/*.zip;	\
		echo "rm -rf $(WWW_DIR)/$$i/*.jar";	\
		rm -rf $(WWW_DIR)/$$i/*.jar	\
	); done;

java:
	set CLASSPATH=$(PWD)
	for i in $(DIRS);	\
	do (	\
		cd $$i; \
		$(MAKE) all; \
		cd ..; \
	); done;


signmerit: 
	$(MAKE) all

#delete entire signing tree if it already exists.
# to be very sure we are picking up the latest classes.

	if [ -d "$(SIGN_JAR_DIR)tmp" ]; then	\
		rm -fR $(SIGN_JAR_DIR)tmp;	\
		rm -f $(SIGN_JAR_DIR)ipma11test.jar;	\
	fi

# copy all the dirs, this is for the merit release
	mkdir $(SIGN_JAR_DIR)/tmp/
	for i in $(DIRS);	\
	do ( 	\
		echo "creating $$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/$$i;	\
		if [ $$i != ipma ] && [ $$i != netscape ]; then	\
			cp $$i/*.class $(SIGN_JAR_DIR)/tmp/$$i/;	\
		fi;	\
	); done;

# copy classes under IPMA
	for i in $(IPMADIRS); 	\
	do (	\
		echo "creating $$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/$$i;	\
		if [ ! -d "$$i/awt" ]; then 	\
			cp $$i/*.class $(SIGN_JAR_DIR)/tmp/$$i/;	\
		fi;	\
	); done;

# copy netscape classes.
	for i in $(NETSCAPE_DIRS);	\
	do (	\
		echo "creating netscape/$$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/netscape/$$i;	\
		cp netscape/$$i/*.class $(SIGN_JAR_DIR)/tmp/netscape/$$i/;	\
	); done;

#sign the tool, this also creates the jar file.
	(	\
		cd  $(SIGN_JAR_DIR);	\
		$(SIGNTOOL) -Z "./ipma11test.jar" -k "Merit Network, Inc.'s VeriSign, Inc. ID" -d"$(KEYDBDIR)"  tmp ;	\
	);



signrelease: 
	$(MAKE) all

#delete entire signing tree if it already exists.
# to be very sure we are picking up the latest classes.

	if [ -d "$(SIGN_JAR_DIR)tmp" ]; then	\
		rm -fR $(SIGN_JAR_DIR)tmp;	\
		rm -f $(SIGN_JAR_DIR)ipma11_ext.jar;	\
	fi

	mkdir $(SIGN_JAR_DIR)/tmp/
	mkdir $(SIGN_JAR_DIR)/tmp/ipma
	mkdir $(SIGN_JAR_DIR)/tmp/netscape

# copy all the release dirs, this is for the external release
	for i in $(RELEASE_DIRS);	\
	do ( 	\
		echo "creating $$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/$$i;	\
		cp $$i/*.class $(SIGN_JAR_DIR)/tmp/$$i/;	\
	); done;

# copy classes under IPMA, note special case for pwp/awt
	for i in $(IPMADIRS); 	\
	do (	\
		echo "creating $$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/$$i;	\
		if [ ! -d "$$i/awt" ]; then 	\
			cp $$i/*.class $(SIGN_JAR_DIR)/tmp/$$i/;	\
		fi;	\
	); done;

# copy netscape classes.
	for i in $(NETSCAPE_DIRS);	\
	do (	\
		echo "creating netscape/$$i";	\
		mkdir $(SIGN_JAR_DIR)/tmp/netscape/$$i;	\
		cp netscape/$$i/*.class $(SIGN_JAR_DIR)/tmp/netscape/$$i/;	\
	); done;

#sign the tool, this also creates the jar file.
	(	\
		cd  $(SIGN_JAR_DIR)/tmp;	\
		$(SIGNTOOL) -Z "../ipma11_ext.jar" -k "Merit Network, Inc.'s VeriSign Trust Network ID" -d"$(KEYDBDIR)"  . ;	\
	);


www:
	for i in $(DIRS);	\
	do (	\
		mkdir $(WWW_DIR)$$i; \
		cp -r $$i/*.class $(WWW_DIR)$$i/; \
		cp -r $$i/*.html $(WWW_DIR)$$i/; \
		cp -r $$i/$i $(WWW_DIR)$$i/; \
	); done;

wwwjar:
	for i in $(DIRS);	\
	do (	\
		echo "$(JAR) cvf $(WWW_DIR)/$$i.jar $(WWW_DIR)/$$i/*.class";	\
		$(JAR) cvf $(WWW_DIR)/$$i.jar $(WWW_DIR)/$$i/*.class	\
	); done;	\
	for i in $(IPMADIRS);	\
	do (	\
		echo "$(JAR) cvf $(WWW_DIR)/ipma/$$i.jar $(WWW_DIR)/$$i/*.class";	\
		$(JAR) cvf $(WWW_DIR)/$$i.jar $(WWW_DIR)/$$i/*.class \
	); done;


wwwzip:
	cd $(WWW_DIR);	\
	for i in $(DIRS);	\
	do (	\
		echo "$(ZIP0) $(WWW_DIR)/$$i.zip $$i/*.class";	\
		$(ZIP0) $(WWW_DIR)/$$i.zip $$i/*.class	\
	); done;	\
	for i in $(IPMADIRS);	\
	do (	\
		echo "$(ZIP0) $(WWW_DIR)/$$i.zip $$i/*.class";	\
		$(ZIP0) $(WWW_DIR)/$$i.zip $$i/*.class	\
	); done;

ipmasrc:
	for i in $(IPMADIRS);   \
	do (    \
		echo "$(ZIP9) ipmasrc.zip $$i/*.java";  \
		$(ZIP9) ipmasrc.zip $$i/*.java  \
	); done;        \
	for i in $(DIRS);       \
	do (    \
		echo "$(ZIP9) ipmasrc.zip $$i/*.java";  \
		$(ZIP9) ipmasrc.zip $$i/*.java  \
	); done;        \
	echo "$(ZIP9) ipmasrc.zip *.java";      \
	$(ZIP9) ipmasrc.zip *.java;

touch:
	for i in $(IPMADIRS);   \
	do (	\
		touch -c $$i/*.java"  \
	); done;        \
	for i in $(DIRS);       \
	do (	\
		touch -c $$i/*.java"  \
	); done;
