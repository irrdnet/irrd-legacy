

include ../../../Make.include

GOAL = libirrpgp.a
OBJS = pgp.o

$(GOAL): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@
	cp $@ ../
	cp pgp.o ../pgp.o
clean:
	$(RM) *.o *.a core *.core *~* $(GOAL)

depend:
	$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c


# DO NOT DELETE THIS LINE -- make depend depends on it.