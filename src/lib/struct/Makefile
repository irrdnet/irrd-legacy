#
# $Id: Makefile,v 1.2 2001/07/13 18:03:04 ljb Exp $
#

include ../../Make.include

OBJS    = New.o stack.o linked_list.o hash.o array.o
#CFLAGS += -D_LL_INTERNAL_DEBUG -DLL_DEBUG
ARCHIVE = $(MRTLIB)

DEBUG_OBJS=New_debug.o stack_debug.o array_debug.o list_debug.o hash_debug.o
INTERNAL_OBJS=list_int.o hash_int.o list_int_deb.o hash_int_deb.o
DEBUG_LIB=-lhash_debug -llist_debug -larray -lstack -lNew

include ../Make.include

# DO NOT DELETE THIS LINE -- make depend depends on it.
