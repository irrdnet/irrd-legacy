/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: synchkif.c,v 1.2 2001/06/12 18:19:40 ljb Exp $
 * originally Id: synchkif.c,v 1.1 1998/08/10 19:23:47 dogcow Exp  */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <errno.h>

#include <irrauth.h>
#include "irr_check.h"


extern int CANONICALIZE_OBJS_FLAG;
extern int yyparse (void);

int callsyntaxchk(trace_t *tr, char *infn, char *outfn, char *tmpfntmpl) {
  FILE *infile;
  extern char *flushfntmpl;


  /* set some globals */
  INFO_HEADERS_FLAG = CANONICALIZE_OBJS_FLAG = 1;

  flushfntmpl = tmpfntmpl;

  strcpy (outfn, tmpfntmpl);
  my_mktemp (tr, outfn);

  umask(0077);
  infile = fopen(infn, "r");
  if (NULL == infile) {
    fprintf(stderr, "Error opening input file %s: %s(%d).\n", 
	    infn, strerror(errno), errno);
    return -1;
  }
  
  /* this is a global var in irr_check, this should be changed later */
  ofile = fopen(outfn, "w");
  if (NULL == ofile) {
    fprintf(stderr, "Error opening output file %s: %s(%d).\n", 
	    infn, strerror(errno), errno);
    return -1;
  }

  yyin = infile;
  yyparse(); /* XXX need to set an output fh one of these days in irr_check */

  fflush (ofile);
  fclose (ofile);
  fclose (infile);
  return 0;
}

