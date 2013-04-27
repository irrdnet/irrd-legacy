ACLOCAL_AMFLAGS         = -I m4
AM_CFLAGS               = -Wall

include Make.include.in

all:	
	@test $(.CURDIR) && cd $(.CURDIR); \
	if [ "$(NEED_CONFIGURE)" ]; then sh ./configure; fi && \
	$(MAKE) library program

install: library
	@echo "Installing programs"; \
	    test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) install

library:
	@echo "Making lib"; \
	    test $(.CURDIR) && cd $(.CURDIR); cd lib; $(MAKE) all

program:
	@echo "Making programs"; \
	    test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) all

clean-progs:
	@echo "Cleaning up lib"; \
	test $(.CURDIR) && cd $(.CURDIR); cd lib; $(MAKE) clean
	@echo "Cleaning up programs"; \
	test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) clean

.PHONY:	clean-progs

clean-am:	clean-progs
distclean-am:	clean-progs

depend:
	@echo "Making dependency in lib"; \
	test $(.CURDIR) && cd $(.CURDIR); cd lib; $(MAKE) $@
	@echo "Making dependency in programs"; \
	test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) $@

depend-clean:
	@echo "Removing dependency in lib"; \
	test $(.CURDIR) && cd $(.CURDIR); cd lib; $(MAKE) $@
	@echo "Removing dependency in programs"; \
	test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) $@
