/* Copyright 1998, Merit Network, Inc. and the University of Michigan */
/* $Id: pgpchk.c,v 1.18 2002/10/17 20:16:14 ljb Exp $*/

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
#include <pgp.h>

/* local yokel's */
static void dump_pgpv_output (trace_t *, char *, char *, char *, FILE *);
static long detached_fcopy   (trace_t *, FILE *, char *);

/* Decode the PGP file, and copy to the output stream any cookies */

/* PGP verify a user submission file.  
 *
 * If there is no pgp signature then the function defaults to
 * a pass through function.  This function can process both
 * regular signatures and detached signatures.  It will not
 * process multiple detached signatures.
 *
 * The function will filter out the pgp parts of the submission 
 * (ie, the '-----BEGIN PGP ...', etc....).  If the submission
 * was signed properly then the signed submission objects are
 * appended with auth cookies.  The auth cookies are picked
 * up later by the parser and an entry in the pipeline header is
 * made.  Then the auth checking module uses the pipeline header
 * to perform it's check.  The cookie information is the hex ID
 * of the verified signer.  The auth checking function will compare
 * the hex ID of the signer with a corresponding 'auth: PGPKEY-...'
 * line of a maintainer.
 *
 * If the pgp submission was not signed properly or the signature 
 * was not registered in the local rings, the function will not
 * add the auth cookies.
 *
 * It is important for users to realize that a 'key-cert' object 
 * must be registered in the DB and a corresponding 'auth: PGPKEY-...'
 * attr in their maintainer must exist for their pgp submissions 
 * to work.
 *
 * Input:
 *   -an intial user input file, can be telent or email file (infile)
 *   -an output file to write the processed submission to (outfn)
 *   -a log file pointer to make a carbon copy of the submission (log_fd)
 *   -a fully qualified path to the pgp rings directory (pgp_dir)
 *
 * Return:
 *   -0 if the submission file was processed without any unexpected errors
 *    (eg, disk error, file error, ...) 
 *   --1 otherwise
 */   
int pgpdecodefile_new (FILE *infile, char *outfn, FILE *log_fd, 
		       char *pgp_dir, trace_t *tr) {
  int n, fd, inpgpsig = 0;
  long fpos;
  enum PGPKEY_TYPE sig_type = NO_SIG;
  pgp_data_t pdat;
  regex_t pgpbegre, pgpendre, cookie_re, pgpbegdet_re;
  char pgpinfn[256], pgpoutfn[256], pgpcookie[128];
  char curline[MAXLINE + 8], *tmp;
  FILE *pipeline_file, *pgpinfile = NULL;

  /* open a pipeline file to place our output */
  pipeline_file = myfopen(tr, outfn, "w+", "pgpdecodefile(): output file");
  if (pipeline_file == NULL)
    return (-1);
 
  /* compile our regex's */
  regcomp (&pgpbegre,     pgpbegin,  REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpendre,     pgpend,    REG_EXTENDED|REG_NOSUB);
  regcomp (&cookie_re,    cookie,    REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpbegdet_re, pgpbegdet, REG_EXTENDED|REG_NOSUB);

  /* process the user submission file */
  while (fgets (curline, MAXLINE + 6, infile) != NULL) {
    /* get rid of those nasty '\r' char's which appear on telnet connections */
    if ((tmp = strchr (curline, '\r')) != NULL) {
      *tmp   = '\n';
      *++tmp = '\0';
    }

    /* telent/inetd connection quit */
    if (!strcmp (curline, "!q\n") || 
	!strcmp (curline, "!q"))
      break;
    
    /* log a carbon copy of the intial user submision */
    if (log_fd != NULL)
      fputs (curline, log_fd);

    /* line too long truncation */
    if (strlen (curline) >= MAXLINE) {
      trace (ERROR, tr, "pgpdecodefile () Line too long! Truncating...\n");
      curline[MAXLINE]     = '\0';
      curline[MAXLINE - 1] = '\n';
      
      /* Throw away the rest of the line */
      while ((n = fgetc (infile)) != EOF && 
	     n != (int) "\n");
    }
    
    /* If we don't have pgp installed then don't look for a signature */
#ifdef PGP
    if (!inpgpsig) {
      if (regexec (&pgpbegre, curline, 0, 0, 0) == 0) {
	sig_type = REGULAR_SIG;
	inpgpsig = 1;
      }
      else if (regexec (&pgpbegdet_re, curline, 0, 0, 0) == 0) {
	sig_type = DETACHED_SIG;
	inpgpsig = 1;
      }
    }
#endif

    switch (inpgpsig) {
 
      case 0: /* Normal, copy to pipeline output file mode, ie, no signature
	       * found yet */
	/* filter out 'COOKIE...' lines if the user supplies any 
	 * (you know those tricky users!) */
	if (regexec (&cookie_re, curline, 0, 0, 0) != 0)
	  fputs (curline, pipeline_file);
        break;

      case 1: /* got a PGP beginning line... */
	/* create a file name to put a copy of the regular signature
	 * and enclosed text or detached signature, file will be used as 
	 * input by pgpv */

	pgpinfile = myfopen(tr, pgpinfn, "w", "pgpdecodefile(): pgpinfile");
        if (pgpinfile == NULL)  {
        strcpy (pgpinfn, tmpfntmpl);
          fclose(pipeline_file);
          unlink(outfn);
          return (-1);
        }

	inpgpsig = 2;
	fputs (curline, pgpinfile);
        break;

      case 2: /* in the middle of a PGP regular sig or detached sig, 
	       * dump to file */
        fputs (curline, pgpinfile);

	/* found the end of the signature  */
        if (regexec (&pgpendre, curline, 0, 0, 0) == 0) {

	  /* close the file copy of the regular signature and enclosed text 
	   * or deteached signature */
	  fclose (pgpinfile);
	  pgpinfile = NULL;

	  /* create a temp file name for the pgpv output */
          strcpy (pgpoutfn, tmpfntmpl);
          fd = mkstemp (pgpoutfn);
          if (fd == -1) {
            trace (ERROR, tr, "pgpdecodefile: create pgp out tempfile error : %s.\n", 
	       strerror (errno));
            fclose(pipeline_file);
            unlink(outfn);
            return (-1);
          }
	  close(fd); /* pgp decode function will reopen the file */

	  /* regular signature processing */
	  if (sig_type == REGULAR_SIG) {
	    /* verify the submission */
	    if (pgp_verify_regular (tr, pgp_dir, pgpinfn, pgpoutfn, &pdat))
	      sprintf (pgpcookie, "%s%s\n", PGP_KEY, pdat.hex_first->key);
	    /* file was not signed properly */
	    else 
	      pgpcookie[0] = '\0'; 
	    
	    /* dump the pgp verify output, adding our auth cookies to each
	     * object and placing the stream onto the pipeline file */
	    dump_pgpv_output (tr, pgpinfn, pgpoutfn, pgpcookie, pipeline_file);
	  }
	  /* detached signature processing */
	  else {
	    trace (NORM, tr, "pgpdecodefile_new (): process detached sig...\n");
	    /* grab the submission objects to file (pgpoutfn), omit an email
	     * header if present, then verify with the sig file (pgpinfn) */
	    if ((fpos = detached_fcopy (tr, pipeline_file, pgpoutfn)) >= 0 &&
		pgp_verify_detached (tr, pgp_dir, pgpoutfn, pgpinfn, &pdat)) {
	      /* move the filepointer to the beginning of objects and rewrite
	       * them with 'cookies' appended */
	      sprintf (pgpcookie, "%s%s\n", PGP_KEY, pdat.hex_first->key);
	      fseek (pipeline_file, fpos, SEEK_SET);
	      dump_pgpv_output (tr, NULL, pgpoutfn, pgpcookie, pipeline_file);
	    }
	  }

	  /* clean up our temp files */
	  unlink (pgpinfn);
	  unlink (pgpoutfn);

	  inpgpsig  = 0;
        }
        break;

    } /* switch */
  } /* while !done */

  /* check for a mal-formed/truncated pgp submission, ie, we didn't find
   * the end of the signature */
  if (inpgpsig &&
      pgpinfile != NULL) {
    /* rewind */
    fseek (pgpinfile, 0L, SEEK_END);

    /* copy the truncated pgp submission to the pipeline output file */
    while (fread (curline, 1, MAXLINE, pgpinfile))
      fwrite (curline, 1, MAXLINE, pipeline_file);
    fclose (pgpinfile);
  }

  /* clean up */
  regfree (&pgpbegre);
  regfree (&pgpendre);
  regfree (&cookie_re);
  regfree (&pgpbegdet_re);
  fclose  (pipeline_file);

  return 0;
}

/* Copy the pgpv outfile with the signature removed to the pipeline
 * output file.  And if the signature is good then add cookies
 * to the pipeline stream with the hex ID of the signer.  The cookies
 * are appended to 'source:' attributes.
 *
 * Input:
 *   -fully qualified file name of copy of the regular signature
 *    and enclosed text (origfn)
 *   -fully qualified file name of the output from pgpv. (pgpvfn)
 *    ie, the regular signature stripped away.
 *   -an auth cookie string line to be appended to each object (auth_cookie)
 *   -a streams point to the pipeline output file (pipeline_file)
 *
 * Return:
 *   void
 *
 *   the orignal submission with the pgp signature stripped away and
 *   auth cookies added are dumped to the (pipeline_file) file stream.
 *
 *   routine also filters out 'cookie' lines the user might have in
 *   the submission (you know those tricky user :)
 */
void dump_pgpv_output (trace_t *tr, char *origfn, char *pgpvfn, 
		       char *auth_cookie, FILE *pipeline_file) {
  int io_errors = 0;
  regex_t cookie_re, cookieins_re;
  char curline[MAXLINE];
  FILE *pgpvfile;

  /* open the pgpv outfile with the signature removed ... */
  if ((pgpvfile = fopen (pgpvfn, "r")) == NULL) {
    trace (ERROR, tr, 
	   "dump_pgpv_output () open pgpv error (%s): %s\n", pgpvfn, strerror (errno));

    /* try using the initial file.  this will cause some syntax
     * errors but the submission object will be appeded with 
     * the cookies */
    if ((pgpvfile = fopen (origfn, "r")) == NULL) {
     trace (ERROR, tr, 
	   "dump_pgpv_output() open origfn error (%s): %s\n", origfn, strerror(errno)); 
     exit (0);
    }

    io_errors = 1;
  }

  /* compile our regex's */
  regcomp (&cookie_re,    cookie,    REG_EXTENDED|REG_NOSUB);
  regcomp (&cookieins_re, cookieins, REG_EXTENDED|REG_NOSUB|REG_ICASE);

  /* ... copy it to our pipeline outfile */
  while (fgets (curline, MAXLINE, pgpvfile) != NULL) {
    /* strip out any 'COOKIE...' lines from the users submission */
    if (regexec(&cookie_re, curline, 0, 0, 0) == 0)
      continue;
    
    /* dump the user lines to the pipeline file */
    fputs (curline, pipeline_file);    

    /* append our cookie lines when we see a 'source' line.
     * the cookie line causes the parser to make a pipeline
     * header field of the hex ID of the signer.  Then the auth
     * routine will use the hex ID header field for it's checking */
    if (!io_errors   &&
	*auth_cookie &&
	regexec (&cookieins_re, curline, 0, 0, 0) == 0) {
      
      /* make sure our cookie does not concat with the prior line */
      if (curline[strlen (curline) - 1] != '\n')
	fputs ("\n", pipeline_file);
      
      /* append our authorization cookie line */
      fputs (auth_cookie, pipeline_file);
    }
  }
  
  /* make sure there is a blank line seperator */
  fputs("\n", pipeline_file);
  
  fclose  (pgpvfile);
  regfree (&cookie_re);
  regfree (&cookieins_re);
}

/* Given a file, extract all objects from it and write them
 * to (outfn).  There maybe an email header in (infile) which should 
 * be filtered out.  Also return the file position of the start of submission
 * objects in (infile).
 *
 * This function is used in detached pgp submission processing.  It's purpose 
 * is to pull the submission objects (minus any email header) from the 
 * pipeline file to a seperate file.  Then this file (outfn) can be verified 
 * using the associated signature file.  The function return value (fpos) 
 * tells the calling routine where the submission objects begin.  Then the 
 * calling routine can overwrite the objects with auth cookies inserted to 
 * the output pipeline file.
 *
 * Input:
 *   -an initial submission file (infile)
 *   -a file to write the output to (outfn)
 *    (outfn) and (infn) may turn out to be identical.
 *    (outfn) is guaranteed to have email header information
 *    stripped away.
 *
 * Return:
 *   -a file position value in (infile) of the start of submission objects (fpos)
 *   --1 otherwise
 */
long detached_fcopy (trace_t *tr, FILE *infile, char *outfn) {
  FILE *fout;
  long fpos = -1;
  int first_line = 1, in_headers = 1;
  char curline[MAXLINE];
  regex_t blankline_re, mailfrom_re;
  char *blankline  = "^[ \t]*[\r]?[\n]?$";
  char *mailfrom   = "^From[ \t]";

  /* need to copy the initial file */
  if ((fout = fopen (outfn, "w")) == NULL) {
    trace (ERROR, tr, "detached_fcopy () Output file open error (%s):(%s)\n", 
	   outfn, strerror (errno)); 
    return -1;
  }
  
  /* compile our regex */
  regcomp (&blankline_re, blankline, REG_EXTENDED|REG_NOSUB);
  regcomp (&mailfrom_re,  mailfrom,  REG_EXTENDED|REG_NOSUB);

  /* rewind */
  fseek (infile, 0L, SEEK_SET);

  /* loop through the input file (infile) and write the objects submission
   * minus the email header (if present) to the output file (outfn) */
  while (fgets (curline, MAXLINE - 1, infile) != NULL) {
    if (first_line && regexec (&mailfrom_re, curline, 0, 0, 0) != 0) {
      in_headers = 0;

      /* we have a telnet submission */
      fpos = 0;
    }
    
    first_line = 0;
    
    /* skip email header */
    if (in_headers) {
      /* wait till we come out of the email header and get
       * the file postion.  note that the first blank line
       * after the email header is not considered part of
       * the submission */
      if (!(in_headers = regexec (&blankline_re, curline, 0, 0, 0)))
	/* we have an email submission */
	fpos = ftell (infile);
    }
    /* write a line to the outfile */
    else
      fputs (curline, fout);
  }
  
  fclose (fout);

  return fpos;
}
