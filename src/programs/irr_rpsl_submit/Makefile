#
# $Id: Makefile,v 1.5 2001/07/13 18:56:35 ljb Exp $
# * originally Id: Makefile,v 1.5 1998/08/10 19:23:43 dogcow Exp 
#

include ../../Make.include

GOAL   = irr_rpsl_submit

RPSLOBJS   = call_pipeline.o auth.o pgpchk.o mailchk.o main.o comdata.o util.o notifyif.o synchkif.o trans_list.o ../hdr_comm/libhdrs.a ../irr_rpsl_check/librpslchk.a  ../irr_notify/libnfy.a ../irrd_ops/libirrd_ops.a ../compat/libcompat.a

all:  $(GOAL) 

irr_rpsl_submit: $(RPSLOBJS)
	@echo "Linking: $(LD) $(LDFLAGS) $(RPSLOBJS) -o $@ $(IRRD_OPS_LIBS)"
	$(LD) $(LDFLAGS) $(RPSLOBJS) -o $@ $(SYS_LIBS) $(CRYPT_LIB)  -L../../lib/
	
lint:
	lint `echo $(RPSLOBJS) | sed -e 's,\.o,.c,g'`

install: $(GOAL)
	$(INSTALL) -m 755 $(GOAL) $(DESTDIR)/


clean:
	$(RM) *.o core *.core *~* *tab.* lex.yy.c $(GOAL)

auth.o: ../../include/irrauth.h ../hdr_comm/libhdrs.a ../irrd_ops/libirrd_ops.a  ../compat/libcompat.a
call_pipeline.o: ../../include/irrauth.h  
pgpchk.o: ../../include/irrauth.h ../pgp/libirrpgp.a
mailchk.o: ../../include/irrauth.h
main.o: ../../include/irrauth.h
comdata.o: ../../include/irrauth.h
util.o: ../../include/irrauth.h
notifyif.o: ../../include/irrauth.h ../hdr_comm/libhdrs.a ../irr_notify/libnfy.a ../compat/libcompat.a
synchkif.o: ../../include/irrauth.h ../hdr_comm/libhdrs.a ../irr_rpsl_check/librpslchk.a ../compat/libcompat.a
trans_list.o: ../../include/irrauth.h
