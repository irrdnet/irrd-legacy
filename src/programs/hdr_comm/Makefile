

include ../../Make.include

GOAL = libhdrs.a
OBJS = hdr_comm.o hdr_fields.o read_conf.o trace.o

$(GOAL): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

clean:
	$(RM) *.o *.a core *.core *~* $(GOAL)

depend:
	$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c

install:
	echo "Nothing to install";


# DO NOT DELETE THIS LINE -- make depend depends on it.
