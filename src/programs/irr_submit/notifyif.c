/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: notifyif.c,v 1.5 2001/06/12 18:19:40 ljb Exp $
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
#include "irr_notify.h"


int callnotify (trace_t *tr, char *infn, int null_submission,
		int null_notification, int dump_stdout, int rps_dist_flag,
		char *irrd_host, int irrd_port, char *db_admin, char *pgpdir, 
		char *dbdir, char *tlogfn, long tlog_fpos, FILE *ack_fd, 
		char * from_ip) {
  FILE *infile;

  umask (0000);
  infile = fopen (infn, "r+");
  if (NULL == infile) {
    fprintf(stderr, "callnotify (): Error opening input file %s: %s(%d).\n", 
	    infn, strerror(errno), errno);
    return -1;
  }
  
  notify (tr, (char *) tmpfntmpl, infile, null_submission, 
	  null_notification, dump_stdout, rps_dist_flag, irrd_host, irrd_port, 
	  db_admin, pgpdir, dbdir, tlogfn, tlog_fpos, ack_fd, from_ip);

  fclose (infile);

  return 0;
}

