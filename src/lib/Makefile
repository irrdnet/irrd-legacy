#
# Makefile,v 1.1 1996/04/15 18:47:33 labovit Exp
#

include ../Make.include

DIRS=config mrt radix struct timer

all:  make-libs

make-libs:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE)"; cd $$i; $(MAKE); cd ..); done

clean:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE) $@"; cd $$i; $(MAKE) $@; cd ..); done; \
	rm -f *.a

depend:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MAKE) $@"; cd $$i; $(MAKE) $@; cd ..); done

depend-clean:
	@for i in $(DIRS); \
	do (echo "cd $$i; $(MKDEPCLEAN)"; cd $$i; $(MAKEDEPCLEAN); cd ..); done
