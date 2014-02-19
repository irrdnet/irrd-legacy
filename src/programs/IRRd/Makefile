#
# $Id: Makefile,v 1.17 2002/02/04 20:53:55 ljb Exp $
# * originally Id: Makefile,v 1.53 1998/08/07 00:13:32 gerald Exp 
#

include ../../Make.include

GOAL   = irrd

OBJS   = main.o telnet.o scan.o config.o commands.o database.o update.o mirror.o uii_commands.o journal.o indicies.o rpsl_commands.o route.o hash_spec.o templates.o irrd_util.o mirrorstatus.o statusfile.o atomic_trans.o $(CFGLIB) $(MRTLIB) 

IRRD_LIBS = -L../atomic_ops -latomic_ops

all:  $(GOAL) 

irrd: $(OBJS)
	@echo "Linking: $(LD) $(OBJS) $(LDFLAGS) -o $@"
	@$(LD) $(OBJS) $(LDFLAGS) -o $@ $(SYS_LIBS) $(IRRD_LIBS)

$(GOAL).purify:	$(OBJS) 
	$(PURIFY) -follow-child-processes $(LD) $(OBJS) $(LDFLAGS) -o $@ $(SYS_LIBS) $(IRRD_LIBS)

$(GOAL).quantify: $(OBJS) 
	$(QUANTIFY) $(LD) $(OBJS) $(LDFLAGS) -o $@ $(SYS_LIBS) $(KVM_LIB) $(IRRD_LIBS)

$(GOAL).lint:
	lint $(CPPFLAGS) `echo $(OBJS) | sed -e 's,\.o,.c,g'`

install: $(GOAL)
	@$(INSTALL) -m 755 $(GOAL) $(DESTDIR)/$(GOAL)

install-man: irrd.8
	@$(INSTALL) -m 644 irrd.8 $(MANDIR)/man8/irrd.8

clean:
	$(RM) *.o core *.core *~* *.quanitfy *.purify $(GOAL)

*.o: ./irrd.h ./scan.h ./irrd_prototypes.h ../atomic_ops/libatomic_ops.a

depend:
	@$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c


# DO NOT DELETE THIS LINE -- make depend depends on it.

