#
# $Id: Makefile,v 1.1.1.1 2000/02/29 22:28:54 labovit Exp $
#

include ../../Make.include

OBJS    = timer.o signal.o
ARCHIVE = $(MRTLIB)

include ../Make.include

#all:	$(OBJS)
#       ld -o libtimer.s0.1.1 -G *.o
#       $(CP) ./libtimer.so /usr/lib/

test:   $(OBJS) test.o
	@$(LD) $(LDFLAGS) $(OBJS) -o $@ test.o $(LIBS)

# DO NOT DELETE THIS LINE -- make depend depends on it.
