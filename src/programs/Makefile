#
# $Id: Makefile,v 1.9 2001/07/13 18:17:37 ljb Exp $
# * originally Id: Makefile,v 1.3 1998/05/06 17:23:35 masaki Exp 
#

include ../Make.include

INSTALL_DIRS= IRRd irr_rpsl_submit irr_rpsl_check irr_notify 
#DIRS=$(PROGRAM_DIRS)
DIRS= irr_util atomic_ops IRRd hdr_comm pgp irr_rpsl_check irrd_ops \
	irr_notify irr_rpsl_submit 
# Does not build on non-thread systems
# rps_dist

all:  make-programs


make-programs:
	for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE)"; cd $$i; $(MAKE); cd ..); done

irrd:
	@echo "cd irrd; $(MAKE)"; cd IRRd; $(MAKE); cd ..; 


install:
	@for i in $(INSTALL_DIRS); \
	do (echo "cd $$i; $(MAKE)"; cd $$i; $(MAKE) install; cd ..); done

clean:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE) $@"; cd $$i; $(MAKE) $@; cd ..); done

depend:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE) $@"; cd $$i; $(MAKE) $@; cd ..); done

depend-clean:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MKDEPCLEAN)"; cd $$i; $(MAKEDEPCLEAN); cd ..); done
