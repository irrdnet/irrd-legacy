#
# $Id: Makefile,v 1.6 2001/08/09 20:42:26 ljb Exp $
# * originally Id: Makefile,v 1.7 1998/08/09 22:16:26 dogcow Exp 
#

include ../../Make.include

GOAL   = irr_notify
IRR_LIBS = -L../irrd_ops -lirrd_ops -L../hdr_comm -lhdrs -L../pgp -lirrpgp -L$(EXEC_PREFIX)/lib
LIBOBJS= notify.o trans_hdrs.o notify_msgs.o util.o 
OBJS   = $(LIBOBJS) main.o

all:  $(GOAL) libnfy.a

$(GOAL): $(OBJS)
	@echo "Linking: $(LDFLAGS) $(LD) $(OBJS) -o $@ $(IRR_LIBS) $(IRRD_OPS_LIBS)"
	@$(LD) $(LDFLAGS) $(OBJS) -o $@ $(IRR_LIBS) $(IRRD_OPS_LIBS)  -L../../lib/ $(SYS_LIBS)


$(GOAL).purify:	$(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) $(DB_LIB)

$(GOAL).quantify: $(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) $(KVM_LIB) $(DB_LIB)

libnfy.a: $(LIBOBJS)
	@$(AR) $(ARFLAGS) $@ $(LIBOBJS) ../hdr_comm/hdr_comm.o ../irrd_ops/irrd_ops.o ../pgp/pgp.o
	@$(RANLIB) $@

install: $(GOAL)
	@$(INSTALL) -m 755 -o root -g bin $(GOAL) $(DESTDIR)/$(GOAL)

clean:
	$(RM) *.a *.o core *.core *~*  *.purify $(GOAL)

libnfy.a: ../../include/irr_notify.h ../hdr_comm/libhdrs.a \
        ../irrd_ops/libirrd_ops.a
notify.o: ../../include/irr_notify.h ../hdr_comm/libhdrs.a \
	../irrd_ops/libirrd_ops.a
main.o:  ../../include/irr_notify.h ../hdr_comm/libhdrs.a \
	../irrd_ops/libirrd_ops.a
notify_msgs.o:  ../../include/irr_notify.h ../hdr_comm/libhdrs.a \
	../irrd_ops/libirrd_ops.a
util.o:  ../../include/irr_notify.h ../hdr_comm/libhdrs.a \
	../irrd_ops/libirrd_ops.a ../pgp/libirrpgp.a

depend:
	@$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c


# DO NOT DELETE THIS LINE -- make depend depends on it.

