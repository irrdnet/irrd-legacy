#
# $Id: Makefile,v 1.3 2001/08/09 20:42:31 ljb Exp $
# * originally Id: Makefile,v 1.7 1998/08/09 22:16:26 dogcow Exp 
#

include ../../Make.include

GOAL   = irr_ripe181_check
LIBOBJS= lex.yy.o ripe181.tab.o irr_attrs.o prefix.o util.o syntax_attrs.o canonical.o hdr_build.o 
IRRD_LIBS = -L$(EXEC_PREFIX)/lib 
OBJS   = $(LIBOBJS) main.o
BISON  = $(BISON_PATH)
LEX    = $(FLEX_PATH)

all:  $(GOAL) libchk.a

$(GOAL): $(OBJS)
	@echo "Linking: $(LD) $(LDFLAGS) $(OBJS) $(IRRD_OPS_LIBS) -o $@ -L../hdr_comm -lhdrs"
	@$(LD) $(LDFLAGS) $(OBJS) $(IRRD_OPS_LIBS) -o $@ -L../hdr_comm -lhdrs -lmrt -L../../lib
	#@$(CP) ./$@ $(EXEC_PREFIX)/bin/

lex.yy.c: ripe181.fl ripe181.tab.c
	$(LEX) ripe181.fl

ripe181.tab.c: ripe181.y
	$(BISON) -v -d ripe181.y

$(GOAL).purify:	$(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ -L../hdr_comm -lhdrs $(IRRD_OPS_LIBS) $(IRRD_LIBS)

$(GOAL).quantify: $(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) $(KVM_LIB) $(DB_LIB)

libchk.a: $(LIBOBJS)
	$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	$(RANLIB) $@

#	@ar cr $@ $(LIBOBJS)

install: $(GOAL)
	$(INSTALL) -m 755 $(GOAL) $(DESTDIR)/$(GOAL)

clean:
	$(RM) *.output *.a *.o core *.core *~* *tab.* lex.yy.c *.purify $(GOAL)

lex.yy.o:  ../../include/irr_check.h ../hdr_comm/libhdrs.a
ripe181.tab.o:  ../../include/irr_check.h ../hdr_comm/libhdrs.a
irr_attrs.o:  ../../include/irr_check.h ../hdr_comm/libhdrs.a
prefix.o:   ../../include/irr_check.h ../hdr_comm/libhdrs.a
util.o:   ../../include/irr_check.h ../hdr_comm/libhdrs.a
syntax_attrs.o: ../../include/irr_check.h ../hdr_comm/libhdrs.a
canonical.o:  ../../include/irr_check.h ../hdr_comm/libhdrs.a
hdr_build.o:  ../../include/irr_check.h ../hdr_comm/libhdrs.a

depend:
	$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c


# DO NOT DELETE THIS LINE -- make depend depends on it.

