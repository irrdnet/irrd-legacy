/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: notifyif.c,v 1.7 2002/10/17 20:16:14 ljb Exp $
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
		int null_notification, int dump_stdout, char *web_origin_str, int rps_dist_flag,
		char *irrd_host, int irrd_port, char *db_admin, char *pgpdir, 
		char *dbdir, char *tlogfn, long tlog_fpos, FILE *ack_fd, 
		char * from_ip) {
  FILE *infile;

  infile = myfopen (tr, infn, "r+", "callnotify(): input file");
  if (infile == NULL)
    return (-1);
 
  notify (tr, (char *) tmpfntmpl, infile, null_submission, null_notification,
	  dump_stdout, web_origin_str, rps_dist_flag, irrd_host, irrd_port, 
	  db_admin, pgpdir, dbdir, tlogfn, tlog_fpos, ack_fd, from_ip);

  fclose (infile);

  return 0;
}

