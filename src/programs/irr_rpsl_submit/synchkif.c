/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: synchkif.c,v 1.3 2002/10/17 20:16:15 ljb Exp $
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
#include "irr_rpsl_check.h"

extern int CANONICALIZE_OBJS_FLAG;
extern int yyparse (void);

int callsyntaxchk(trace_t *tr, char *infn, char *outfn, char *tmpfntmpl) {
  FILE *infile;
  extern char *flushfntmpl;

  /* set some globals */
  INFO_HEADERS_FLAG = CANONICALIZE_OBJS_FLAG = 1;
  flushfntmpl = tmpfntmpl;

  infile = myfopen(tr, infn, "r", "callsyntaxchk(): input file");
  if (infile == NULL)
    return (-1);

  ofile = myfopen(tr, outfn, "w", "callsyntaxchk(): output file");
  if (ofile == NULL) {
    fclose(infile);
    return (-1);
  }

  yyin = infile;
  yyparse(); /* XXX need to set an output fh one of these days in irr_rpsl_check */

  fflush (ofile);
  fclose (ofile);
  fclose (infile);
  return 0;
}

