#
# $Id: Makefile,v 1.2 2001/07/13 18:13:54 ljb Exp $
#

include Make.include

all::	
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

clean:
	@$(RM) config.cache
	@$(RM) config.log
	@$(RM) config.status
	@echo "Cleaning up lib"; \
	test $(.CURDIR) && cd $(.CURDIR); cd lib; $(MAKE) $@
	@echo "Cleaning up programs"; \
	test $(.CURDIR) && cd $(.CURDIR); cd programs; $(MAKE) $@
	@test $(.CURDIR) && cd $(.CURDIR); $(RM) config.cache

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
