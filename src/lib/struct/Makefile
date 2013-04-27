#
# $Id: Makefile,v 1.2 2001/07/13 18:03:04 ljb Exp $
#

include ../../Make.include

OBJS    = linked_list.o irrdmem.o
#CFLAGS += -D_LL_INTERNAL_DEBUG -DLL_DEBUG
ARCHIVE = $(MRTLIB) $(CFGLIB) 

DEBUG_OBJS= irrdmem_debug.o list_debug.o
INTERNAL_OBJS=list_int.o hash_int.o list_int_deb.o 
DEBUG_LIB=-lhash_debug -llist_debug -lirrdmem

include ../Make.include

# DO NOT DELETE THIS LINE -- make depend depends on it.
