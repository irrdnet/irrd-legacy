#
# $Id: Makefile,v 1.6 2001/10/18 15:29:38 ljb Exp $
# * originally Id: Makefile,v 1.7 1998/08/09 22:16:26 dogcow Exp 
#

include ../../Make.include

GOAL   = irr_rpsl_check
LIBOBJS= lex.yy.o y.tab.o irr_attrs.o prefix.o util.o syntax_attrs.o canonical.o hdr_build.o dictionary.o 
IRRD_LIBS = -L$(EXEC_PREFIX)/lib 
OBJS   = $(LIBOBJS) main.o
LEX    = $(FLEX_PATH)

all:  $(GOAL) librpslchk.a

$(GOAL): $(OBJS)
	@echo "Linking: $(LD) $(LDFLAGS) $(OBJS) $(IRRD_OPS_LIBS) -o $@ -L../hdr_comm -lhdrs -L../pgp -lirrpgp $(SYS_LIBS)"
	@$(LD) $(LDFLAGS) $(OBJS) $(IRRD_OPS_LIBS) -o $@ -L../hdr_comm -lhdrs  -L../pgp -lirrpgp $(SYS_LIBS)

#	@$(CP) ./$@ $(EXEC_PREFIX)/bin/

lex.yy.c: rpsl.fl y.tab.c
	@$(LEX) rpsl.fl

y.tab.c: rpsl.y
	@$(YACC) -v -d rpsl.y

$(GOAL).purify:	$(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ -L../hdr_comm -lhdrs -L../pgp -lirrpgp $(IRRD_OPS_LIBS) $(IRRD_LIBS)

$(GOAL).quantify: $(OBJS) $(MRT_LIBS)
	$(PURIFY) $(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS) $(KVM_LIB) $(DB_LIB)

librpslchk.a: $(LIBOBJS)
	@$(AR) $(ARFLAGS) $@ $(LIBOBJS)
	@$(RANLIB) $@

#	@ar cr $@ $(LIBOBJS)

install: $(GOAL)
	@$(INSTALL) -m 755 $(GOAL) $(DESTDIR)/$(GOAL)

clean:
	$(RM) *.output *.a *.o core *.core *~* *tab.* lex.yy.c *.purify $(GOAL)

lex.yy.o:  ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
y.tab.o:  ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
irr_attrs.o:  attrs.rpsl ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
prefix.o:   ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
util.o:   ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
syntax_attrs.o: ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
canonical.o:  ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
hdr_build.o:  ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a
dictionary.o:  ../../include/irr_rpsl_check.h ../hdr_comm/libhdrs.a ../pgp/libirrpgp.a

depend:
	@$(MAKEDEP) $(CFLAGS) $(CPPFLAGS) $(DEFINES) *.c


# DO NOT DELETE THIS LINE -- make depend depends on it.

