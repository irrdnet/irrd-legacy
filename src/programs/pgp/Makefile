

include ../../Make.include

libirrpgp.a:
	@echo "cd $(PGPDIR); $(MAKE)"; cd $(PGPDIR); \
	$(MAKE) "CPPFLAGS=$(CPPFLAGS) -I../../../include"; cd ..; 

clean:
	@echo "cd $(PGPDIR); $(MAKE) $@"; cd $(PGPDIR); $(MAKE) $@; cd ..;
	$(RM) *.a *.o

depend:
	@echo "cd $(PGPDIR); $(MAKE) $@"; cd $(PGPDIR); $(MAKE) $@; cd ..;

# DO NOT DELETE THIS LINE -- make depend depends on it.
