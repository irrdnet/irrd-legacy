

include ../../Make.include

GOAL = libirrd_ops.a
OBJS = irrd_ops.o

$(GOAL): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)
	$(RANLIB) $@

clean:
	$(RM) *.o *.a core *.core *~* $(GOAL)

depend:
	$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c

*.o: ../hdr_comm/libhdrs.a

# DO NOT DELETE THIS LINE -- make depend depends on it.
