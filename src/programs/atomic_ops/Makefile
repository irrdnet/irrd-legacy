
include ../../Make.include

GOAL = libatomic_ops.a
OBJS = atomic_ops.o

$(GOAL): $(OBJS)
	@$(AR) $(ARFLAGS) $@ $(OBJS)
	@$(RANLIB) $@

clean:
	$(RM) *.o *.a core *.core *~* $(GOAL)

depend:
	@$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c

