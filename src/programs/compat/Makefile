
include ../../Make.include

GOAL = libcompat.a
OBJS = snprintf.o mktemp.o inet_ntop.o inet_pton.o

$(GOAL): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

clean:
	$(RM) *.o *.a core *.core *~* $(GOAL)

depend:
	$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c

