/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: mailchk.c,v 1.6 2002/10/17 20:16:14 ljb Exp $
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

#define BIGSTRBUFLEN 2048	/* space for large strings */
#define SMALLSTRBUFLEN 256	/* space for small strings */

extern trace_t *default_trace;

/* Given a transaction file that has been stripped of PGP encoding
 * information (if there are any PGP signatures), append email header
 * information and 'password:' fields to each object in the transaction file.  
 * Email header information is the 'From', 'msg-id', 'date', and 'subject' 
 * email fields.
 *
 * Recall the RPSL password convention.  If a password field appears
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
  char curline[BIGSTRBUFLEN], passwd[SMALLSTRBUFLEN];
  char email_info[BIGSTRBUFLEN], from_line[SMALLSTRBUFLEN];
  char *hdr, *strptr;
  FILE *infile, *outfile;
  regex_t mailfromre, mailfromncre, messidre, subjre, datere;
  regex_t blanklinere, cookieinsre, passwdre, mailreplytore;
  regmatch_t mailfromrm[4];
  int in_headers = 1, count_lines = 0;
  int first_line = 1, seen_reply_to = 0;
  int unknown_user = 0;
  int whichoffset = 0, eo, so, prev, obj_len;
  int end_obj = 0, src_seen = 0, in_obj = 0, done = 0;

  infile = myfopen(tr, infn, "r", "addmailcookies(): input file");
  if (infile == NULL)
    return(-1);

  outfile = myfopen(tr, outfn, "w", "addmailcookies(): output file");
  if (outfile == NULL) {
    fclose(infile);
    return(-1);
  }
  
  /* initialization */
  passwd[0]     = '\0';
  from_line[0]  = '\0';
  email_info[0] = '\0';
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
    /* loop reading lines from message - allow room to append \n\0 */
    done = (NULL == fgets (curline, BIGSTRBUFLEN - 2, infile)) ? 1 : 0;

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
	if (strlen(email_info) + strlen(from_line) > BIGSTRBUFLEN) {
            trace(ERROR, tr, "Email headers exceeds maximum length of %d:\n %s%s", BIGSTRBUFLEN, email_info, from_line);
	    fclose(outfile);
    	    unlink(outfn);
	    return(-1);
	}
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

	obj_len = mailfromrm[whichoffset].rm_eo - mailfromrm[whichoffset].rm_so;
	if (obj_len >= SMALLSTRBUFLEN - strlen(FROM) - 2) {
            trace(ERROR, tr, "Mail From \"%s\" exceeds maximum length of %d.\n", curline, SMALLSTRBUFLEN - strlen(FROM) - 2);
	    fclose(outfile);
            unlink(outfn);
	    return(-1);
	}
	strcpy (from_line, (char *) FROM);
	strptr = from_line + strlen(FROM);
	memcpy (strptr, (char *) (curline + mailfromrm[whichoffset].rm_so),
		  (size_t) (obj_len));
	strptr += obj_len;
	*strptr++ = '\n';
	*strptr = '\0';
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
	if (strlen(email_info) + strlen(hdr) + strlen((char *) (curline + so)) > BIGSTRBUFLEN) {
            trace(ERROR, tr, "Email headers exceeds maximum length of %d:\n %s%s%s", BIGSTRBUFLEN, email_info, hdr, (char *) (curline + so));
	    fclose(outfile);
            unlink(outfn);
	    return(-1);
	}
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
	  obj_len = mailfromrm[whichoffset].rm_eo - mailfromrm[1].rm_so;
	  if (obj_len >= SMALLSTRBUFLEN - 3) {
            trace(ERROR, tr, "Passord \"%s\" exceeds maximum length of %d.\n", curline, SMALLSTRBUFLEN - 3);
	    fclose(outfile);
            unlink(outfn);
	    return(-1);
	  }
	  passwd[0] = '"';
	  memcpy ((passwd + 1), (char *) (curline + mailfromrm[1].rm_so),
		  (size_t) (obj_len));
	  passwd[obj_len + 1] = '"';
	  passwd[obj_len + 2] = '\0';
	}	
	in_obj = prev;
	continue;
      }

      if (end_obj) { 
	if (src_seen && passwd[0] != '\0') {
	  fprintf (outfile, "password: %s\n", passwd);
	  trace (NORM, tr, "Admincookies found password: (%s)\n", passwd);
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

