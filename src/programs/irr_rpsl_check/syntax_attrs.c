/* 
 * $Id: syntax_attrs.c,v 1.21 2002/10/17 20:22:10 ljb Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <irr_rpsl_check.h>
#include <irr_defs.h>
#include <pipeline_defs.h>
#include <pgp.h>

extern char *attr_name[];
extern regex_t re[];
extern char *countries[];

int xx_set_syntax (char *target, char *s) {
  char *p;
  char *q = strdup (s);
  int ret_val = 0;

  p = strtok (q, ":");
  while (p != NULL) {
    if (!strcasecmp (p, target)) {
      ret_val = 1;
      break;
    }
    p = strtok (NULL, ",");
  }
  free (q);

  return ret_val;
}

/* -------------individual field check functions ------------------*/

int country_syntax (char *country, parse_info_t *obj) {

  if (is_country (country, countries) >= 0)
    return 1;

  error_msg_queue (obj, "Unknown country", ERROR_MSG);  
  return 0;
}

/* check AS number for maximum value; regex already checks for basic correctness */

int asnum_syntax (char *asstr, parse_info_t *obj) {

  u_long val;
  char *endptr = NULL;
  char *asval;

  errno = 0;    /* To distinguish success/failure after call */
  asval = asstr + 2; /* skip AS part of string */
  val = strtoul(asval, &endptr, 10);

  /* Check for various possible errors */

  if ((errno == ERANGE && val == ULONG_MAX)
        || (errno != 0 && val == 0)) {
    error_msg_queue (obj, "AS number contains out-of-range value\n", ERROR_MSG);  
    return 0;
  }

  /* check for 64 bit architectures */
  if (sizeof (uint) != sizeof (u_long)) {
    if (val > UINT_MAX) {
      error_msg_queue (obj, "AS number contains out-of-range value\n", ERROR_MSG);  
      return 0;       /* exceeds max 32-bit uint */
    }
  }

  return 1;
}

/* Given an inetnum address range (ie, inetnum: a1 - a2 or a1 > a2)
 * determine if a1 is greater than a1.
 *
 * Return:
 *   1 if a1 is numerically <= a2 (ie, no errors)
 *   0 if a1 is numerically greater than a2 (ie, an error).
 */
int inetnum_syntax (parse_info_t *obj, char *prefix1, char *prefix2) {
  u_char dst1[4], dst2[4];
  u_int val1, val2;
  int i;

  if (irrd_inet_pton (AF_INET, prefix1, dst1) <= 0) {
    error_msg_queue (obj, "Malformed address in start of range", ERROR_MSG);
    return 0;
  }

  if (irrd_inet_pton (AF_INET, prefix2, dst2) <= 0) {
    error_msg_queue (obj, "Malformed address in end of range", ERROR_MSG);
    return 0;
  }
  
  val1 = val2 = 0;
  for (i = 0; i < 4; i++) {
    val1 |= dst1[i] << (24 - (i * 8));
    val2 |= dst2[i] << (24 - (i * 8));
  }

  if (val1 > val2) {
    error_msg_queue (obj, "Invalid address range", ERROR_MSG);
    return 0;
  }

  return 1;
}

/* ud - delete
 *
 * Return codes:
 *  0 means syntax failed, ie bad syntax
 *  1 means a warning was given
 *  2 means good syntax
 *  an empty attr generates a warning and
 *  is not an error.
 */
int delete_syntax (char *del, parse_info_t *obj) {

  if (obj->op == NULL)
    obj->op = strdup (DEL_OP);
  
  return 2;
}

/* password
 *
 * Return codes:
 *  0 means syntax failed, ie bad syntax
 *  1 means a warning was given
 *  2 means good syntax
 *  an empty attr is silently ignored.
 */
int password_syntax (char *del, parse_info_t *obj) {
  char *p, *q;

  if (del == NULL)
    return 1;

  if ((p = strchr (del, '\n')) != NULL)
    *p = '\0';

  p = q = del;

  if (irrcheck_find_token (&p, &q) < 0)
    return 1;
  
  if (obj->password == NULL)
    obj->password = strdup (p);
  
  return 2;
}

/*
 * so - source
 */
void source_syntax (char *source_name, parse_info_t *obj) {
  source_t *src;
  extern config_info_t ci;

  if ((src = is_sourcedb (source_name, ci.srcs)) != NULL) {
    if (!src->authoritative)
      error_msg_queue (obj, "Non-authoritative database", ERROR_MSG);      
  }
  else
    error_msg_queue (obj, "Unknown database", ERROR_MSG);      
}

/*
 * em - e-mail
 * mn - mnt-nfy
 * ny - notify
 * dt - upd-to
 */
int email_syntax (char *email_addr, parse_info_t *obj) {
  char *p;

  /*if (verbose) fprintf (dfile, "JW: email_syntax(%s)\n", email_addr);*/
  if ((p = strchr (email_addr, '@')) == NULL) {
    error_msg_queue (obj, "Missing '@' in email address", ERROR_MSG);
    return 0;
  }

  if (strchr (p + 1, '@') != NULL) {
    error_msg_queue (obj, "Multiple '@' found in email address", ERROR_MSG);
    return 0;
  }

  if (regexec (&re[RE_EMAIL3], email_addr, (size_t) 0, NULL, 0)  ||
      (regexec (&re[RE_EMAIL1], email_addr, (size_t) 0, NULL, 0) &&
       regexec (&re[RE_EMAIL2], email_addr, (size_t) 0, NULL, 0))) {
    error_msg_queue (obj, "Malformed RFC822 email address", ERROR_MSG);       
    return 0;
  }

  return 1;
}

void regex_syntax (char *reg_exp, parse_info_t *obj) {
  regex_t comp_re;
  
  if (regcomp (&comp_re, reg_exp, REG_EXTENDED|REG_NOSUB) != 0) {
    error_msg_queue (obj, "Illegal regular expression", ERROR_MSG);     
    return;
  }
  
  regfree(&comp_re);
}

void cryptpw_syntax (char *cryptpw, parse_info_t *obj) {
  
  if (regexec(&re[RE_CRYPT_PW], cryptpw, (size_t) 0, NULL, 0))
      error_msg_queue (obj, "Illegal encrypted password ([./0-9A-Za-z]{13})", ERROR_MSG);
}

/* Return today's date in integer format, 
 * eg, 19991122
 */
int todays_date () {
  time_t now;
  struct tm *tmptr;
  
  now = time (NULL);
  tmptr  = localtime (&now);

  return ((1900 + tmptr->tm_year) * 10000) + (++tmptr->tm_mon * 100) + tmptr->tm_mday;
}

/* Given a 'date', deterimine if it is in the future.
 * 
 * Return:
 *   A non-NULL string date with today's date if the input
 *   date is in the future.
 *
 *   Otherwise return NULL
 */
char *date_in_future (char *date) {
  int intdate1 = 0, intdate2;
  char strdate[10];

  sscanf (date, "%d", &intdate1);
  if (intdate1 == 0)
    return NULL;

  intdate2 = todays_date ();

  if (intdate1 > intdate2) {
    sprintf (strdate, "%d", intdate2);
    return strdup (strdate);
  }

  return NULL;
}

/* Given a 'date' (ie, YYMMDD) make sure the date is in proper
 * format and syntax.  YY must be >= 1988.  If the date is
 * in the future, change the date to today's date and return 
 * the correct date.  Skip the future check if 'skip_future_check' is set
 * (ie, withdrawn: attr)
 *
 * Return:
 *   NULL if no errors in date and date is not in the future
 *   today's date if no errors in date and date is in the future
 */
char *date_syntax (char *date, parse_info_t *obj, int skip_future_check) {
  char year[5];
  char month[3];
  char day[3];
  char strdate[9];
  char *now;
  int errors;

  /* ensure the date is 6 char's long and all numbers */
  if (regexec(&re[RE_DATE], date, (size_t) 0, NULL, 0)) {
      error_msg_queue (obj, "Malformed date (/^YYYYMMDD$/)", WARN_OVERRIDE_MSG);
      error_msg_queue (obj, "Changing to today's date", WARN_OVERRIDE_MSG);
      sprintf (strdate, "%d", todays_date ());
      return strdup (strdate);

    /*
	error_msg_queue (obj, "Malformed date (/^YYYYMMDD$/)", ERROR_MSG);
	return NULL;
    */
  }

  errors = 0;

  /* check year part */
  strncpy (year, date, 4);
  year[4] = '\0';

  if (strncmp (year, "1988", 4) < 0) {
      error_msg_queue (obj, "Year part of date is too old (YYYYMMDD)", ERROR_MSG);    
      errors++;
  }

  /* check month part */
  strncpy (month, &date[4], 2);
  month[2] = '\0';

  if ((strncmp (month, "01", 2) < 0) ||
      (strncmp (month, "12", 2) > 0)) {
      error_msg_queue (obj, "Syntax error in month part of date (YYYYMMDD)", ERROR_MSG);
      errors++;
  }

  /* check day part */
  strncpy (day, &date[6], 2);
  day[2] = '\0';

  if ((strncmp (day, "01", 2) < 0) ||
      (strncmp (day, "31", 2) > 0)) {
      error_msg_queue (obj, "Syntax error in day part of date (YYMMDD)", ERROR_MSG);
      errors++;
  }

  if (errors || skip_future_check)
    return NULL;

  if ((now = date_in_future (date)) != NULL) {
    /*
    error_msg_queue (obj, "Date is in the future, changed to today's date", WARN_OVERRIDE_MSG);
    */
    return now;
  }

  return NULL;
}

/* Return a time interval component adding or removing leading 0's as
 * necessary.
 *
 * Function will return the time value component in (outs) if no (ins)
 * can be represented in a field of (width) characters.  Otherwise
 * (outs) is not changed and an error message will be registered.
 * 
 * Input:
 *  -data struct to allow error messages to be created (obj)
 *  -string length of the time component (width)
 *  -error message to be diplayed if the time component cannot
 *   be trimmed to the proper length (err_msg)
 *  -input time component (ins)
 *  -return value time component in a string (width) characters long (outs)
 *
 * Return:
 *  -time component in a string (width) characters long if there were no errors
 *  -void (ie, do not change the (outs) value)
 */
void ti_check (parse_info_t *obj, int width, char *err_msg, char *ins, char *outs) {
  int i, j;
  char *p;

  /* time value is too short, pad with '0's */
  if ((i = strlen (ins)) < width) {
    sprintf (outs, "%.*s%s", (width - i), "0000", ins);
    return;
  }

  /* time value is too long,
   * see if removing leading '0's will solve */
  if (i > width) {
    for (p = ins; *p == '0'; p++);
    j = (int) (p - ins);
    if ((i - j) <= width)
      sprintf (outs, "%.*s%s", width - i + j, "0000", p);
    else
      error_msg_queue (obj, err_msg, ERROR_MSG);

    return;
  }

  /* good */
  strcpy (outs, ins);
}

/* Canonicalize and check for errors a time interval value.
 *
 * prescibed syntax:
 * 'dddd hh:mm:ss  eg, '0001 01:00:00'  
 *
 * 'dddd' are the day's, 'hh' are hours, 'mm' are minutes and 'ss' are seconds.
 * Function checks for 'hh' < 24, 'mm' and 'ss' < 60.  Function will attempt
 * to fix good values by adding or removing leading 0's as necessry. 
 *
 * Input:
 *  -data struct to allow error messages to be created (obj)
 *  -days value (days)
 *  -hours value (hours)
 *  -minutes value (mins)
 *  -seconds value (secs)
 *
 * Return:
 *  -a canonicalized time interval if no errors were found
 *  -NULL otherwise
 */
char *time_interval_syntax (parse_info_t *obj, char *days,
			    char *hours, char *mins, char *secs) {
  int j;
  char buf[16], d[5], h[3], m[3], s[3];

  /* days: '0000' */
  d[0] = '\0';
  ti_check (obj, 4, "Time interval (days) too long.  "
	    "Must be exactly 4 digits.  eg, '0010'", days, d);

  /* hours: '00' and < 24 */
  h[0] = '\0';
  sscanf (hours, "%d", &j);
  if (j > 23)
    error_msg_queue (obj, "Time inverval (hours) to large > 23", ERROR_MSG);
  else
    ti_check (obj, 2, "Time interval (hours) too long.  "
	      "Must be exactly 2 digits.  eg, '01'", hours, h);

  /* minutes: '00' and < 60 */
  m[0] = '\0';
  sscanf (mins, "%d", &j);
  if (j > 59)
    error_msg_queue (obj, "Time inverval (minutes) to large > 59", ERROR_MSG);
  else
    ti_check (obj, 2, "Time interval (minutes) too long.  "
	      "Must be exactly 2 digits.  eg, '30'", mins, m);

  /* seconds: '00' and < 60 */
  s[0] = '\0';
  sscanf (secs, "%d", &j);
  if (j > 59)
    error_msg_queue (obj, "Time inverval (seconds) to large > 59", ERROR_MSG);
  else
    ti_check (obj, 2, "Time interval (seconds) too long.  "
	      "Must be exactly 2 digits.  eg, '45'", secs, s);

  /* mal-formed time interval */
  if (*d == '\0' ||
      *h == '\0' ||
      *m == '\0' ||
      *s == '\0')
    return NULL;

  /* good time interval */
  sprintf (buf, "%s %s:%s:%s", d, h, m, s);

  /* clean up */
  free (days);
  free (hours);
  free (mins);
  free (secs);

  return strdup (buf);
}

void name_syntax (parse_info_t *obj, char *hdl) {
  char *p, *q, *r = NULL, c;
  int i;

  p = q = hdl;
  for (i = 0; irrcheck_find_token (&p, &q) > 0; i++, *q = c) {
    c = *q;
    *q = '\0';
    r = NULL;
    if (verbose) fprintf (dfile, "JW: field[%d]-(%s)\n", i, p);

    /* email names not allowed */
    if (strchr (p, '@') != NULL)
      error_msg_queue (obj, "Syntax error.  Looks like an email address", ERROR_MSG);
    /* check the basic structure and legal characters */
    else if (regexec(&re[RE_NAME], p, (size_t) 0, NULL, 0)) {
      error_msg_queue (obj, "Syntax error in person name.  Non-alphanumeric or other formating error.", ERROR_MSG);       
    }
    /* check for abbreviation on first and last name */
    else {
      r = p + strlen (p) - 1;
      if (i == 0 && *r == '.')
	error_msg_queue (obj, "Abbreviated first names not allowed", ERROR_MSG);
    }
  }

  if (r != NULL && *r == '.')
    error_msg_queue (obj, "Abbreviated last names not allowed", ERROR_MSG);

  /* legal names must have at least two components */
  if (i < 2)
      error_msg_queue (obj, "Names must have at least two components or bad handle", 
		       ERROR_MSG);
}

int is_nichdl (char *nichdl) {
  extern config_info_t ci;

  if (verbose) fprintf (dfile, "\n---JW: enter is_nichdl(%s): short circut, nichdl() always true!\n", nichdl);

  /* sanity check only */
  return !regexec(&re[RE_SANITY_HDL], nichdl, (size_t) 0, NULL, 0);

/* more extensive checks below -- not currently done */
#ifdef notdef  
  /* lower case alpha not allowed */
  if (!regexec(&re[RE_LCALPHA], nichdl, (size_t) 0, NULL, 0)) {
    if (verbose) fprintf (dfile, "JW: found lower case, is not a handle (%s)!\n", nichdl);
    return 0;
  }
  
  /* check for auto-RIPE nic handle */
  if (!regexec(&re[RE_RIPE_HDL], nichdl, (size_t) 0, NULL, 0)) {
    if (verbose) fprintf (dfile, "JW: is a auto RIPE handle (%s)!\n", nichdl);
    return 1;
  }
  
  /* check for ARIN handles */
  if (!regexec(&re[RE_ARIN_HDL], nichdl, (size_t) 0, NULL, 0)) {
    if (verbose) fprintf (dfile, "JW: is a ARIN handle (%s)!\n", nichdl);
    return 1;
  }
  
  /* check for APNIC handle */
  if (!regexec(&re[RE_APNIC_HDL], nichdl, (size_t) 0, NULL, 0)) {
    if (verbose) fprintf (dfile, "JW: is a jpnic handle (%s)!\n", nichdl);
    return 1;
  }
  
  /* check for standard handle */
  if (!regexec(&re[RE_STD_HDL], nichdl, (size_t) 0, NULL, 0)) {
    if (verbose) fprintf (dfile, "JW: is a std handle (%s)!\n", nichdl);
    if ((p = strchr (nichdl, '-')) != NULL) {
      if (verbose) fprintf (dfile, "JW: checking suffix of std hdl (%s)...\n", p);
      p++;
      if (is_sourcedb (p, ci.srcs) != NULL) {
	if (verbose) fprintf (dfile, "JW: hdl-ret (is a sourcedb!\n");
        return 1;
      }
      
      if (is_country (p, countries) >= 0) {
	if (verbose) fprintf (dfile, "JW: hdl-ret (is a country!\n");
        return 1;
      }
      
      if (is_special_suffix (p)) {
	if (verbose) fprintf (dfile, "JW: hdl-ret (is a spec suffix!\n");
        return 1;
      }

      /* JMH - special case - see if it ends with -NIC */
      if (!strcmp (p, "NIC") && (p[3] == '\0')) {
	if (verbose) fprintf (dfile, "JMH: -NIC override\n");
	return 1;
      }
      
      /* JMH - we allow a handle with two '-'s in them so long as the
	 last one is -NIC */
      if ((p = strchr (p, '-')) != NULL) {
	p++;
	if (!strcmp (p, "NIC") && (p[3] == '\0')) {
	  if (verbose) fprintf (dfile, "JMH: -NIC override\n");
	  return 1;
	}
      }

    }
    else
      return 1;
  }

  return 0;
#endif /* notdef */
}

/*
 * ac - admin-c
 * ph - person
 * ro - role
 * ah - author
 * tc - tech-c
 * zc - zone-c
 */
void hdl_syntax (parse_info_t *obj, char *hdl) {
  char *tok_s, *p, *q;

  p = q = hdl;
  if (irrcheck_find_token (&p, &q) > 0) {
    if (verbose) fprintf (dfile, "JW: enter hdl_syntax(): %s %s\n", attr_name[obj->curr_attr], hdl);

    tok_s = p;
    /* see if we have a nic-hdl, illegal for role and person objs */
    if (irrcheck_find_token (&p, &q) < 0) {
      if (is_nichdl (tok_s)) {
	if (obj->curr_attr == F_RO || obj->curr_attr == F_PN)
	  error_msg_queue (obj, 
			   "Handles not allowed for \"person\" and \"role\" objects",
			   ERROR_MSG);
	return; /* nic-hdl's legal for admin-c, tech-c, zone-c, author */
      }
    }

    /* It's not a nic handle, so check for legal name syntax */
    name_syntax (obj, hdl);
  }
  else /* should never get here */
    error_msg_queue (obj, "Empty attribute removed", EMPTY_ATTR_MSG);
}

 /*
  * nh - nic-hdl
  */
void nichdl_syntax (char *nic_hdl, parse_info_t *obj) {

  if (!is_nichdl (nic_hdl))
    error_msg_queue (obj, "Syntax error in NIC handle", ERROR_MSG);
}

/* Given a maintainer reference, '*mntner' (ie, a list member from a 'mnt-by'
 * attribute), check for names that begin with a reserved prefix (eg, 'rs-').
 *
 * Return:
 *    void
 *    side effect: if a name begins with a reserved prefex, send an error msg.
 */
void mb_check (parse_info_t *o, char *mntner) {
  char buf[MAXLINE];

  if (mntner == NULL)
    return;

  if (has_reserved_prefix (mntner)) {
    snprintf (buf, MAXLINE, "Maintainer reference begins with a reserved prefix (%s)\n", mntner);
    error_msg_queue (o, buf, ERROR_MSG);
  }
}


/* Get the key fingerprint from the ring pointed to by the environment
 * variable 'PGPPATH' which should be set before calling this function.  
 * This function assumes that a new 'key-cert' object has been added to 
 * a temporary ring (ie, so there is a ring with a single key).  If there 
 * is no fingerprint or more than one then return an error return code.
 *
 * DSS/Diffie-Hellman keys require special processing since there is
 * a fingerprint for the DSS key (which is used for signing and the 
 * one we want) and the Diffie-Hellman key.  Here is an example:
 *

% pgpk +batchmode=1 -ll
Type Bits KeyID      Created    Expires    Algorithm       Use
pub   768 0x32ED1200 2000-06-30 ---------- DSS             Sign & Encrypt 
f20    Fingerprint20 = B512 5B8A BF23 EEE2 A10A  0A93 5451 7218 32ED 1200
sub   768 0x5B2AE285 2000-06-30 ---------- Diffie-Hellman                 
f20    Fingerprint20 = 39AA 19FC A6C7 03D3 0078  B368 257E D59F 5B2A E285
uid  Gerald A. Winters <gerald@merit.edu>
sig       0x32ED1200 2000-06-30 Gerald A. Winters <gerald@merit.edu>

 * We want the fingerprint after the 'pub', which is the first one.  After
 * the 'sub' we don't care and ignore any more fingerprints.
 *
 * Note for RSA keys we do not have this sitution.
 *
% pgpk -ll
Type Bits KeyID      Created    Expires    Algorithm       Use
pub   768 0x8938AA1B 2000-06-30 ---------- RSA             Sign & Encrypt 
f16    Fingerprint16 = 6A E5 51 2B F8 67 F6 72  A6 8E 1E 05 13 6F BD AB
uid  Gerald A. Winters <gerald@merit.edu>
sig       0x8938AA1B 2000-06-30 Gerald A. Winters <gerald@merit.edu>

 *
 * Return:
 *
 *    0 if a single key fingerprint was extracted from the ring with no errors
 *    1 otherwise
 */
#ifdef PGP
int get_fingerprint (parse_info_t *o, char *PGPPATH, pgp_data_t *pdat) {
  int pgp_ok = 1;

  if (pgp_fingerpr (default_trace, PGPPATH, pdat->hex_first->key, pdat)) {
    if (pdat->fingerpr_cnt > 1) {
      error_msg_queue (o, "Too many fingerprints in certificate", ERROR_MSG);
      pgp_ok = 0;
    }
    else
      o->u.kc.fingerpr = strdup (pdat->fp_first->key);
  }
  else {
    error_msg_queue (o, "Couldn't find fingerprint in certificate", ERROR_MSG);
    pgp_ok = 0;
  }

  return pgp_ok;
}

#endif

/* We have parsed a 'certif:' attribute from a 'key-cert' object.
 * Now perform the check to make sure the hexid specified in the
 * 'key-cert:' attribute matches the hexid from the 'certif:'
 * attribute.  We perform this check (and others) by adding the
 * 'certif:' attribute/PGP key block to a temp ring and parsing
 * the output from PGP.  Additionally, we check the following:
 *
 *  1. make sure there is only 1 hexid in the key
 *  2. make sure there is at least 1 owner/uid specified
 *  3. make sure we get a 'Successful' return string from PGP
 *  4. call get_fingerprint () to get the key fingerprint
 *     which checks to make sure there is only 1 fingerprint
 *
 * Return:
 *
 *    file name of key certificate file if there were no errors found
 *      - the file can then be used to easily add the PGP key to the
 *        local ring
 *      - the key fingerprint and owner(s)/uid(s) are saved to be
 *        used for the corresponding 'key-cert' auto-generated fields
 *    NULL otherwise
 */
char *hexid_check (parse_info_t *o) {
#ifdef PGP
  char pgpinfn[256], tmp_pgp_dir[256], ebuf[1024];
  FILE *pgpin;
  char_list_t *p;
  pgp_data_t pdat;
  int pgp_errors = 0, fd;
#endif

  /* If we don't have PGP installed then there is nothing to do. */
#ifndef PGP
    error_msg_queue (o, "PGP is not installed on the system.  Cannot perform operation.", ERROR_MSG);
    return NULL;
#else

  /* make a tmp directory for the pgp rings */
  umask (0022);
  strcpy (tmp_pgp_dir, "/var/tmp/pgp.XXXXXX");
  if (mkdtemp (tmp_pgp_dir) == NULL) {
    error_msg_queue (o, "Internal error.  Couldn't create temp directory for PGP\n", ERROR_MSG);
    return NULL;
  }

  /* create a file and put the key certificate into it */
  strcpy (pgpinfn, "/var/tmp/irrsyntax.XXXXXX");
  fd = mkstemp (pgpinfn);
  if ((pgpin = fdopen (fd, "w")) == NULL) {
    error_msg_queue (o, 
"Internal error.  Could not check 'key-cert:' temp file creation error", ERROR_MSG);
    goto pgp_errors_jump;
  }
  fwrite (o->u.kc.certif, sizeof (char), strlen (o->u.kc.certif), pgpin);
  fclose (pgpin);

  /* add the certificate to a temp ring */
  if (!pgp_add (default_trace, tmp_pgp_dir, pgpinfn, &pdat)) {
    error_msg_queue (o, "Couldn't add key-cert to ring.  Didn't get successful reply from PGP", ERROR_MSG);
    goto pgp_errors_jump;
  }

  /* certificate checks */

  /* do we have more than 1 hex ID? */
  if (pdat.hex_cnt > 1) {
    error_msg_queue (o, "Too many public keys in certificate", ERROR_MSG);
    pgp_errors = 1;
  }
  /* does the hex ID of the 'key-cert' attr match the certificate hex ID ? */
  else if (strcmp (pdat.hex_first->key, o->u.kc.kchexid)) {
    snprintf (ebuf, 1024, "Hex-ID mistmatch: 'key-cert:' (%s)  'certif:' (%s)\n",
	     o->u.kc.kchexid, pdat.hex_first->key);
    error_msg_queue (o, ebuf, ERROR_MSG);
    pgp_errors = 1;
  }
  /* owner check */
  else if (pdat.owner_first == NULL) {
    error_msg_queue (o, "No uid's/owners found in certificate", ERROR_MSG);
    pgp_errors = 1;
  }
  /* grab all the owners */
  else {
    o->u.kc.owner_count = pdat.owner_cnt;
    for (p = pdat.owner_first; p != NULL; p = p->next)
      o->u.kc.owner = my_concat (o->u.kc.owner, p->key, 0);
  }

  /* get the key fingerprint */
  if (!pgp_errors && 
      !get_fingerprint (o, tmp_pgp_dir, &pdat)) 
    pgp_errors = 1;

  if (o->u.kc.owner != NULL)
    if (verbose) fprintf (dfile, 
	     "owner count (%d) owner (%s)\n", o->u.kc.owner_count, o->u.kc.owner);
  if (o->u.kc.fingerpr != NULL)
    if (verbose) fprintf (dfile, "fingerpr (%s)\n", o->u.kc.fingerpr);

  /* end debug */

  /* clean up */
  pgp_free (&pdat);

  /* if we have errors then we won't need the key file */
  if (pgp_errors) {
pgp_errors_jump:
    rm_tmpdir (tmp_pgp_dir);  /* can get rid of temporary directory */
    remove (pgpinfn); 
    return NULL;
  }

  rm_tmpdir (tmp_pgp_dir);  /* can get rid of temporary directory */

  /* leave pgp key for possible addition to local ring */
  return strdup (pgpinfn);
#endif
}

/* Given a comma seperated sources is (ie, rx-in: IRROrder(radb,ans,...)),
 * return an error if 'new_source' is already in the list 'sources_list'.
 *
 * Return
 *
 *   0 if new_source is already a member of 'sources_list'
 *     or DB is now defined in irrd.conf file (ie, is now known)
 *   1 otherwise
 *
 */
int irrorder_syntax (parse_info_t *obj, char *sources_list, char *new_source) {
  char buf[MAXLINE], *p;

  /* See if we know of this DB 
  if (strcasecmp (new_source, "radb")  &&
      strcasecmp (new_source, "mci")   &&
      strcasecmp (new_source, "canet") &&
      strcasecmp (new_source, "bell") &&
      strcasecmp (new_source, "ripe")) {
    sprintf (buf, "Unknown database \"%s\"", new_source);
    error_msg_queue (obj, buf, ERROR_MSG);
    return 1;
    }*/

  /* See if we know of this DB 
  if (is_sourcedb (new_source, ci.srcs) == NULL) {
    sprintf (buf, "Unknown database \"%s\"", new_source);
    error_msg_queue (obj, buf, ERROR_MSG);
    return 1;   
  }
  */

  /* Make sure the DB isn't duplicately defined */
  if (sources_list != NULL && strlen (sources_list) < MAXLINE) {
    strcpy (buf, sources_list);
    p = strtok (buf, ",");
    while (p != NULL) {
      if (*p == ' ')
	p++;
      if (!strcmp (p, new_source)) {
	snprintf (buf, MAXLINE, "Duplicate database \"%s\"", new_source);
	error_msg_queue (obj, buf, ERROR_MSG);
	return 1;
      }
      p = strtok (NULL, ",");
    }
  }

  return 0;
}
