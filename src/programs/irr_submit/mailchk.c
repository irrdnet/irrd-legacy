/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: mailchk.c,v 1.5 2001/09/19 21:36:45 ljb Exp $
 * originally Id: mailchk.c,v 1.2 1998/08/10 19:24:23 dogcow Exp  */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <regex.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>

#include <irrauth.h> /* Includes the PGP cookies, pgpmailauth and pgpkeyauth */

extern trace_t *default_trace;

/* Given a transaction file that has been stripped of PGP encoding
 * information (if there are any PGP signatures), append email header
 * information and 'password:' fields to each object in the transaction file.  
 * Email header information is the 'From', 'msg-id', 'date', and 'subject' 
 * email fields.
 *
 * Recall the ripe181 password convention.  If a password field appears
 * anywhere in a transaction it applies to the rest of the objects.
 * So this routine looks for any password fields and appends the password
 * to all subsequent objects in the transaction.
 *
 * When we refer to "append" information, we mean append a field of the form:
 *    HDR-XXX: <value>
 * the syntax checker is designed to recognized these fields as special
 * and to pass them to the auth checker to use.  See hdr_fields.c for a
 * listing of all the header fields.
 *
 * Return:
 *   0..n (ie, number of non-null lines read)
 *        this routine can detect null submissions
 *   -1 an error occured (eg, opening the input file)
 */
int addmailcookies (trace_t *tr, int daemon_mode, char *infn, char *outfn) {
  char curline[4096], passwd[256];
  FILE *infile, *outfile;
  regex_t mailfromre, mailfromncre, messidre, subjre, datere;
  regex_t blanklinere, cookieinsre, passwdre, mailreplytore;
  regmatch_t mailfromrm[4];
  int in_headers = 1, count_lines = 0;
  int first_line = 1, seen_reply_to = 0;
  int unknown_user = 0;
  int whichoffset = 0, eo, so;
  int end_obj, src_seen, in_obj, prev, done;
  char *hdr, email_info[1024], from_line[256];

  strcpy (outfn, tmpfntmpl);
  my_mktemp (tr, outfn);
  umask(0077);
  infile = fopen(infn, "r");
  if (NULL == infile) {
    fprintf(stderr, "Error opening input file %s: %s(%d).\n", infn, strerror(errno), errno);
    return(-1);
  }
  
  outfile = fopen(outfn, "w");
  if (NULL == outfile) {
    fprintf(stderr, "Error opening output file %s: %s(%d).\n", outfn, strerror(errno), errno);
    return(-1);
  }
  
  /* initialization */
  passwd[0]     = '\0';
  from_line[0]  = '\0';
  email_info[0] = '\0';
  done = src_seen = end_obj = in_obj = 0;
  regcomp(&mailfromre, mailfrom, (REG_EXTENDED|REG_ICASE));
  regcomp(&mailreplytore, mailreplyto, (REG_EXTENDED|REG_ICASE));
  regcomp(&mailfromncre, mailfromnc, REG_ICASE);
  regcomp(&messidre, messid, (REG_EXTENDED|REG_ICASE));
  regcomp(&subjre, subj, REG_EXTENDED|REG_ICASE);
  regcomp(&datere, date, REG_EXTENDED|REG_ICASE);
  regcomp(&blanklinere, blankline, REG_EXTENDED|REG_NOSUB);
  regcomp(&cookieinsre, cookieins, (REG_NOSUB|REG_EXTENDED|REG_ICASE));
  regcomp(&passwdre, password, (REG_EXTENDED|REG_ICASE));

  while (!done) {
    done = (NULL == fgets (curline, MAXLEN, infile)) ? 1 : 0;

    if (done) {
      if (memcmp (curline, "password:", 9)) {
	if (curline[strlen (curline) - 1] != '\n')
	  fputs ("\n", outfile);
	if (curline[0] == '\n')
	  break;
      }
      memcpy (curline, "\n", 2);
    }

    if (first_line && 0 != regexec (&mailfromncre, curline, 0, 0, 0)) {
      in_headers = 0;
      if (!daemon_mode) /* it's not a mail or tcp submission */
	unknown_user = 1;
    }
    /* The first line _has_ to match /^From/, or else it's not really a
       mail message. The syntax checker doesn't like the mail headers as
       an object. :) */
    first_line = 0;

    if (in_headers && regexec (&blanklinere, curline, 0, 0, 0) == 0) {
      in_headers = 0;
      if (seen_reply_to == 0 && from_line[0] != '\0')
	strcat (email_info, from_line);
    }

    /* have to special case /From(:?)/ because it can have stuff in either
       match position one or three. */

    /* pick off the 'From', 'msg-id', 'date', 'subject' from the
     * the email header (if there is one)
     */
    if (in_headers) {
      so = eo = -1; hdr = NULL; /* make the compiler happy */
      if (0 == regexec(&mailfromre, curline, 4, mailfromrm, 0)) {
	if (mailfromrm[3].rm_so != -1)/* <foo@bar> stuff */
	  whichoffset = 3;
	else
	  whichoffset = 1;

	*(curline + mailfromrm[whichoffset].rm_eo + 1) = '\0';
	*(curline + mailfromrm[whichoffset].rm_eo)     = '\n';
	strcpy (from_line, (char *) FROM);
	strcat (from_line, (char *) (curline + mailfromrm[whichoffset].rm_so));
      } /* header snarf */

      if (0 == regexec(&mailreplytore, curline, 4, mailfromrm, 0)) {
	seen_reply_to = 1;
	if (mailfromrm[3].rm_so != -1)/* <foo@bar> stuff */
	  whichoffset = 3;
	else
	  whichoffset = 1;

	so = mailfromrm[whichoffset].rm_so;
	eo = mailfromrm[whichoffset].rm_eo;
	hdr = (char *) FROM;
      } /* header snarf */
      else if (0 == regexec(&messidre, curline, 2, mailfromrm, 0)) {
	so = mailfromrm[1].rm_so;
	eo = mailfromrm[1].rm_eo;
	hdr = (char *) MSG_ID;
      }
      else if (0 == regexec(&subjre, curline, 2, mailfromrm, 0)) {
	so = mailfromrm[1].rm_so;
	eo = mailfromrm[1].rm_eo;
	hdr = (char *) SUBJECT;
      }
      else if (0 == regexec(&datere, curline, 2, mailfromrm, 0)) {
	so = mailfromrm[1].rm_so;
	eo = mailfromrm[1].rm_eo;
	hdr = (char *) DATE;
      }

      if (so != -1) {
	*(curline + eo + 1) = '\0';
	*(curline + eo)     = '\n';
	strcat (email_info, hdr);
	strcat (email_info, (char *) (curline + so));
      }
    }
    /* else not in headers */
    else {
      prev = in_obj;
      if (regexec (&blanklinere, curline, (size_t) 0, NULL, 0)) {
	if (in_obj == 0) {
	  src_seen = 0;
	  if (unknown_user) {
	    fputs ((char *) HDR_START, outfile);
	    fputs ("\n", outfile);
	    fputs ((char *) FROM, outfile);
	    fputs ((char *) UNKNOWN_USER_NAME, outfile);
	    fputs ("\n", outfile);
	    fputs ((char *) UNKNOWN_USER, outfile);
	    fputs ("\n", outfile);
	    fputs ((char *) HDR_END, outfile);
	    fputs ("\n", outfile);
	  }
	}
	in_obj = 1;
	end_obj = 0;
	count_lines++;
      }
      else {
	end_obj = in_obj;
	in_obj = 0;
      }

      if (in_obj && !src_seen)
	src_seen = (int) (0 == regexec (&cookieinsre, curline, (size_t) 0, NULL, 0));

      /*
      printf ("(end_obj, src_seen)=(%d,%d):%s", end_obj, src_seen, curline);
      */

      if (0 == regexec (&passwdre, curline, 3, mailfromrm, 0)) {
	if (mailfromrm[1].rm_so != -1) {
	  if (mailfromrm[2].rm_so != -1)
	    whichoffset = 2;
	  else
	    whichoffset = 1;

	  /* enclose passwd in '"'s; it can have spaces in it :( */
	  passwd[0] = '"';
	  *(curline + mailfromrm[whichoffset].rm_eo) = '"';
	  mailfromrm[whichoffset].rm_eo++;
	  *(curline + mailfromrm[whichoffset].rm_eo) = '\0';
	  memcpy ((passwd + 1), (char *) (curline + mailfromrm[1].rm_so),
		  (size_t) (mailfromrm[whichoffset].rm_eo - mailfromrm[1].rm_so + 1));
	}	
	in_obj = prev;
	continue;
      }

      if (end_obj) { 
	if (src_seen && passwd[0] != '\0') {
	  fprintf (outfile, "password: %s\n", passwd);
	  trace (NORM, default_trace, "Admincookies found password: (%s)\n", passwd);
	}
	if (email_info[0] != '\0')
	  fputs (email_info, outfile);
      }

      fputs (curline, outfile);
    }

  } /* fgets */

  /* empty submission, make a legal header for notify routine */
  if (count_lines == 0) {
    fputs ((char *) HDR_START, outfile);
    fputs ("\n", outfile);
    fputs (email_info, outfile);
    fputs ((char *) HDR_END, outfile);
    fputs ("\n", outfile);
  }

  regfree (&messidre);
  regfree (&mailfromre);
  regfree (&mailreplytore);
  regfree (&mailfromncre);
  regfree (&subjre);
  regfree (&datere);
  regfree (&blanklinere);
  regfree (&cookieinsre);
  regfree (&passwdre);

  fclose (outfile);
  fclose (infile);

  if (unknown_user)
    return 0;
  return count_lines;
}
    
  
