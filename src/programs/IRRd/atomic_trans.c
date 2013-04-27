
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config_file.h"
#include "irrd.h"

/* The functions in this file support atomic transactions for irrd.
 * For each !us...!ue transaction there is a transaction file built
 * which can be used to restore a DB to its state prior to a trans.
 * On bootstrap irrd looks for transaction files and if it finds one
 * that is complete it will re-apply the transaction. */

/* local yokel's */

static int     file_exists         (DIR *, char *);
static char    *get_ufname         (char *);
static int     active_trans        (irr_database_t *, char *);
static void    free_rbtrans_list   (rollback_t *);
static trans_t *create_rbtrans_obj (irr_database_t *, char *);
static void    add_rbtrans_obj     (rollback_t *, trans_t *);
static long    complete_trans_file (FILE *, char *, char *);
static int     active_trans        (irr_database_t *, char *);
static int     transaction_dels    (irr_database_t *, FILE *, FILE *);

/* Initialize the transaction update file with the file offset
 * and first three letters of the key attribute for the
 * operations in the !us...!ue update file.
 *
 * Function runs through the update file and for each 
 * object it does a lookup and records to (fout) the file offset of
 * the object and the first three letters of the key attribute.
 * In case of rollback, this information can be used to
 * restore the original DB as an '*xx' is written over the
 * first three letters of objects to signify they are deleted.
 *
 * The !us...!ue file has two operations, ADD and DEL with the
 * ADD operation being a replace operation if the object already
 * exists.  Therefore, ADD operation objects are checked and
 * if it exists an entry will be made in the transaction file we
 * are building.
 * 
 * Input:
 *  -pointer to the database struct (db)
 *  -file pointer to the update file, ie, !us...!ue (find)
 *  -file pointer to the transaction file we are writing to (fout)
 *
 * Return:
 *  -the number of upates in the transaction if there were no 
 *   errors and all DEL/REPLACE operations were successfully recorded in 
 *   the transaction update file.
 *  -0 otherwise
 */
int transaction_dels (irr_database_t *db, FILE *fin, FILE *fout) {
  int state = 0, i, first_lookup = 1, n = 0;
  u_long offset, len;
  u_short as;
  long save_fpos;
  enum IRR_OBJECTS type;
  char buf[BUFSIZE+1], pkey[BUFSIZE+1], attr[BUFSIZE+1];
  irr_connection_t irr;
  regex_t re_key, re_origin, re_bl;
  regmatch_t rm[5];
  char *key    = "^([^ \t\n]+):[ \t]*(([^ \t\n])|([^ \t\n].*[^ \t\n]))"
                 "[ \t]*\n$";
  char *origin = "^origin:[ \t]*AS([[:digit:]]+)\n$";
  char *bl     = "^[ \t]*\n?$";

  if ((save_fpos = ftell (fin)) < 0 ||
      fseek (fin, 0L, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "transaction_dels (): "
	   "Transaction file I/O error: (%s).\n", strerror (errno));
    return 0;
  }

  /* compile our regex's */
  regcomp (&re_key,    key,    REG_EXTENDED);
  regcomp (&re_origin, origin, REG_EXTENDED|REG_ICASE);
  regcomp (&re_bl,     bl,     REG_EXTENDED|REG_NOSUB);

  while (fgets (buf, BUFSIZE, fin) != NULL) {    
    /* ADD or DEL state */
    if (state == 0 &&
	(!strcmp (buf, "DEL\n") ||
	 !strcmp (buf, "ADD\n"))) {
      trace (NORM, default_trace, "JW t_dels: enter state 1\n");
      /* tally the total number of updates in this trans;
       * this will be our return value */
      n++; 
      state = 1;
      continue;
    }
    
    /* blank line state */
    if (state == 1) {
      if (regexec (&re_bl, buf, 0, NULL, 0)) {
	trace (ERROR, default_trace, "transaction_dels (): malformed update "
	       "file. Expect blank line but got (%s)\n", buf);
	n = 0;
	goto ABORT_TRANS;
      }
      state = 2;
      continue;
    }

    /* beginning of object state, pick off the primary key and look it up
     * to find the object offset */
    if (state == 2) {
      trace (NORM, default_trace, "JW t_dels: enter state 2\n");
      if (regexec (&re_key, buf, 5, rm, 0)) {
	trace (ERROR, default_trace, "transaction_dels (): malformed update "
	       "file.  Expect key line but got (%s)\n", buf);
	n = 0;
	goto ABORT_TRANS;
      }

      /* save the attribute ... */
      *(buf + rm[1].rm_eo) = '\0';
      snprintf (attr, BUFSIZE, "%s,", buf + rm[1].rm_so);

trace (NORM, default_trace, "JW: attr (%s)\n", attr);

      /* ... and key values */
      i = 4;
      if (rm[4].rm_so == -1)
	i = 3;
      *(buf + rm[i].rm_eo) = '\0';
      snprintf (pkey, BUFSIZE, "%s", buf + rm[i].rm_so);

trace (NORM, default_trace, "JW: key (%s)\n", pkey);

      /* now find the objects type (eg, route, maintainer, aut-num, ...) */
      for (i = 0; m_info[i].command &&
	          strncasecmp (attr, m_info[i].command,
			       strlen (m_info[i].command));
	          i++);

      /* did we find the object type? 
       * if yes then look it up and find its file offset and first
       * three attribute letters so we can restore delete operations */
      if (m_info[i].command) {
	/* non-route object processing */
        if ((type = m_info[i].type) != ROUTE) {
	  if (first_lookup) {
	    irr.ll_database = LL_Create (0);
	    LL_Add (irr.ll_database, db);
	    first_lookup = 0;
	  }

	  /* look it up and get the file offset in the DB */
	  len = 0;
	  irr_database_find_matches (&irr, pkey, PRIMARY, 
				     RAWHOISD_MODE|TYPE_MODE, type, 
				     &offset, &len);

	  /* save the offset and the first 3 attribute letters for rollback */
	  if (len > 0)
	    fprintf (fout, "%-10ld %.*s\n", (long) offset, 3, attr);

	  state = 0;
	}
	/* route object processing; have to get the origin to do the lookup */
	else
	  state = 3;

	continue;
      }
      /* oops!  couldn't locate the object type */
      else {
	trace (ERROR, default_trace, "transaction_dels (): couldn't identify "
	       "the key type: (%s) (%s)\n", pkey, attr);
	n = 0;
	goto ABORT_TRANS;
      }
    }

    /* route object processing */
    if (state == 3 &&
	regexec (&re_origin, buf, 2, rm, 0) == 0) {
trace (NORM, default_trace, "JW: route processing line (%s)\n", buf);
      *(buf + rm[1].rm_eo) = '\0';
      sscanf (buf + rm[1].rm_so, "%hu", &as);
trace (NORM, default_trace, "JW: route processing (%s, %hu)...\n", pkey, as);
      len = 0;
      seek_prefix_object (db, ROUTE, pkey, as, &offset, &len);
      /* save the offset and first 2 attribute letters for rollback */
      if (len > 0)
	fprintf (fout, "%-10ld %.*s\n", (long) offset, 3, attr);
      state = 0;
    }
  }

ABORT_TRANS:

  /* clean up */
  regfree (&re_key);
  regfree (&re_origin);
  regfree (&re_bl);
  if (!first_lookup)
    LL_Destroy (irr.ll_database);

  if (fseek (fin, save_fpos, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "transaction_dels (): Transaction "
	   "file pos restore I/O error: (%s).\n", strerror (errno));
    return 0;
  }

  /* if we didn't end up in state '0' then there were errors */
  return ((state == 0) ? n : 0);
}

/* Build an update transaction file to be used to restore the
 * DB in the event of a crash or other error in processing
 * the update.
 *
 * file format:
 * ts: cs + n            # long value in a field of 10; curr serial 
 *                         of this transaction (field width 14)
 * cs                    # original current serial; -1 means irrd is
 *                         not journaling (ie, !ms...) >= 0 otherwise
 * original journal size # length of journal before transaction; -1
 *                         means irrd is not journaling 
 *                         (ie, !ms...) >= 0 otherwise
 * transaction file name # full path name of the !us...!ue file
 * original file length  # length of DB before the transaction
 * fpos xx               # file position of deleted object and first 
 *  ...                    2 attr letters
 * ts: cs + n            # same as line one.  used to check for complete
 *                         transaction file
 *
 * A completly built transaction file is signified by the first and
 * last lines of the file being equal.
 *
 * See db_rollback () for an outline of the DB restoration procedure.
 * 
 * Input:
 *  -pointer to the database struct (db)
 *  -file pointer to the update file, ie, !us...!ue file (u_fd)
 *  -char template to return the name of the transaction file
 *   this function builds (fname).
 *  -integer pointer to the number of updates in the transaction
 *   
 * 
 * Return:
 *  -NULL if the transaction file was built without error.
 *  -otherwise an string indicating that the transaction file 
 *   could not be built.
 *
 *  function will return a transaction file name suitable for rollback.
 */
char *build_transaction_file (irr_database_t *db, FILE *u_fp, char *uname,
			      char *fname, int *n_updates) {
  int fd, n;
  long jsize, dbsize;
  FILE *fp;
  struct stat fstats;

  /* sanity check */
  if (db->db_fp == NULL) {
    trace (ERROR, default_trace, "DB (%s) is not open.  "
	   "Abort update op.\n", db->name);
    return "DB does not exist or is not open for updates!";
  }

  /* we need to find out how large the DB is */
  sprintf (fname, "%s/%s.db", IRR.database_dir, db->name);
  if ((fd = open (fname, O_RDONLY, 0)) < 0) {
    trace (ERROR, default_trace, "Abort update!  Cannot open (%s) "
	   "to determine how large it is: %s\n", db->name, strerror (errno));
    return "Cannot determine DB size.";
  }
  fstat (fd, &fstats);
  dbsize = fstats.st_size;
  close (fd);

  /* if we are journaling then we need the original journal file
   * size for rollback */
  jsize = -1;
  if ((db->flags & IRR_AUTHORITATIVE) ||
      db->mirror_prefix != NULL) {
    if (db->journal_fd > 0) {
      fstat (db->journal_fd, &fstats);
      jsize = fstats.st_size;
    }
    else
      jsize = 0;
  }

  /* open/create a transaction file */
  sprintf (fname, "%s/%s.trans", IRR.database_dir, db->name);
  if ((fp = fopen (fname, "w+")) == NULL) {
    trace (ERROR, default_trace, "Abort update!  Can't open atomic transaction"
	   " file for DB (%s): (%s).\n", db->name, strerror (errno));
    return "Cannot open atomic transaction file!";
  }

  /* log the cs of the transaction, the original cs,
   * the !us...!ue update file name and the original DB file size */
  fprintf (fp, "ts: %-10ld\n", 0L);
  fprintf (fp, "%ld\n", (long) ((jsize >= 0) ? db->serial_number : -1));
  fprintf (fp, "%ld\n", jsize);
  fprintf (fp, "%s\n", uname);
  fprintf (fp, "%ld\n", dbsize);

  /* build the DEL list, ie, the file positions
   * of deleted/replaced objects and the first 2 attr letters */
  if (!(n = transaction_dels (db, u_fp, fp))) {
    fclose (fp);
    remove (fname);
    return "Abort update!  transaction file operation error.";
  }

  /* write the cs at the end of the file ... */
  if (fseek (fp, 0, SEEK_END) < 0) {
    trace (ERROR, default_trace, "Abort update!  "
	   "Transaction rewind file op error (%s): %s.\n", 
	   db->name, strerror (errno));
    fclose (fp);
    remove (fname);
    return "Transaction file operation error.";
  }
  fprintf (fp, "ts: %-10ld\n", (long) (db->serial_number + n));

  /* ... now write the cs to at the beginning of the file to indicate a 
   * complete transaction file has been initialized */
  if (fseek (fp, 0L, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "Abort update!  "
	   "Transaction rewind file op error (%s): %s.\n", 
	   db->name, strerror (errno));
    fclose (fp);
    remove (fname);
    return "Transaction file operation error.";
  }

  trace (NORM, default_trace, "JW: serial_num (%lu) conv (%-10ld)\n", 
	 db->serial_number, (long) (db->serial_number + n));

  fprintf (fp, "ts: %-10ld\n", (long) (db->serial_number + n));
  fclose  (fp);
  *n_updates = n;

  trace (NORM, default_trace, "JW: transaction update file successfully created (%s) bye-bye!\n", fname);

  return NULL;
}

/* Rollback DB (db->name) to its original state after a
 * transaction.
 *
 * Rollback can occur after a system crash or after an
 * aborted transaction.  In the event of a system crash,
 * the function will check the integrity of the transaction
 * file which is used to roll back the trancsaction.  
 *
 * Function assumes that first a transaction file (fname) is
 * built which can be used to restore the DB to its original
 * state and then the transaction is applied to the DB.  
 * Therefore, if the transaction file is not complete then no 
 * changes were made to the DB and no repair is needed.
 *
 * This function is idempotent and can be called any number
 * of times and the final state of the DB will be the same.
 * This is a desirable property since it is possible for the 
 * system to crash again (or some other error) while the DB 
 * repair is taking place.
 *
 * See build_transaction_file () for an explanation of the
 * transaction file format used to restore the DB.
 *
 * Restoration procedure:
 * 1. see if transaction file was built completely
 * 2. truncate the DB its orignal length
 * 3. for each "fpos xx" line, go to byte offset "fpos" and write "xx"
 *    to undo the delete operation.
 * The DB should now be in its original state.
 * 
 * Input:
 *  -pointer to the database struct (db)
 *  -name of the transaction file which is used to restore the
 *   DB (fname).
 *
 * Return:
 *  -1 if it restored the DB to its original state without error
 *  -0 otherwise
 */
int db_rollback (irr_database_t *db, char *fname) {
  int reopenf = -1, ret_code = 0;
  FILE *fin;
  char buf[BUFSIZE+1], attr[256];
  long fpos;

  /* sanity check */
  if (fname == NULL) {
    trace (ERROR, default_trace, "db_rollback (): NULL transcation file "
	   "name.\n");
    return 0;
  }

  trace (NORM, default_trace, "JW: rollback fname (%s)\n", fname);
  
  /* open the transaction file which is used to restore the DB */  
  if ((fin = fopen (fname, "r")) == NULL) {
    trace (ERROR, default_trace, "db_rollback ():  Can't open atomic "
	   "transaction file (%s) for DB (%s): (%s): exit (0)\n", 
	   fname, db->name, strerror (errno));
    return 0;
  }

  /* was the transaction file built completely? 
   * if not then no changes were made to the DB 
   * and there is nothing to roll back */
  switch (complete_trans_file (fin, fname, db->name)) {
  case -1: /* trans file I/O error */
    trace (NORM, default_trace, "db_rollback (): transaction file (%s) "
	   "error.\n", fname);
    return 0;
  case 0: /* incomplete trans file */
    trace (NORM, default_trace, "db_rollback (): transaction file (%s) "
	   "was not complete; nothing to do, returning.\n", fname);
    goto CLEAN_UP;
  }

  /* read past the cs, original current serial, journal file length,
   * and !us...!ue update file name */
  if (fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL) {
    trace (ERROR, default_trace, "db_rollback ():  Can't read atomic "
	   "transaction file (%s) for DB (%s), fgets () error: (%s)\n", 
	   fname, db->name, strerror (errno));
    goto CLEAN_UP;
  } 

  /* read the DB original file length before the transaction */
    if (fscanf (fin, "%ld\n", &fpos) != 1) {
    trace (ERROR, default_trace, "db_rollback ():  Can't read the atomic "
	   "transaction file (%s, original size) for DB (%s): (%s)\n", 
	   fname, db->name, strerror (errno));
    goto CLEAN_UP;
  }

  /* we need to have the DB closed for the file truncation operation.
   * so if the DB is open (ie, transacton abort) then we need to re-open
   * it after we truncate */
  reopenf = 0;
  if (db->db_fp != NULL) {
    reopenf = 1;
    fclose (db->db_fp);
  }

  /* truncate the DB to its original length;
   * this effectively undoes any ADD operations to the DB */
  sprintf (buf, "%s/%s.db", IRR.database_dir, db->name);
  if (truncate (buf, fpos) < 0) {
    trace (ERROR, default_trace, "db_rollback ():  Can't truncate DB "
	   "(%s) to (%ld) bytes: (%s)\n", buf, fpos, strerror (errno));
    goto CLEAN_UP;
  }

  /* open the DB */
  if ((db->db_fp = fopen (buf, "r+")) == NULL) {
    trace (ERROR, default_trace, "db_rollback (): Can't re-open DB (%s):"
	   "(%s)\n", strerror (errno));
    goto CLEAN_UP;
  }

  trace (NORM, default_trace, "JW: rollback original size (%ld)\n", fpos);


  /* Undo the delete operations */
  while (fgets (buf, BUFSIZE, fin) != NULL) {
    /* last line is 'ts: <cs>' */
    if (buf[0] == 't')
      break;

    /* read the file position of the DEL and the attribute
     * letters that were overwritten with '*xx' */
    if (sscanf (buf, "%ld %s", &fpos, attr) != 2) {
      trace (ERROR, default_trace, "db_rollback (): Can't read an 'fpos attr' "
	     "line for file (%s) for DB (%s)\n", fname, db->name);
      goto CLEAN_UP;
    }

    trace (NORM, default_trace, "JW: rollback buf (%s) (%ld,%s)\n", buf, fpos, attr);

    /* go to the DEL operation file position ... */
    if (fseek (db->db_fp, fpos, SEEK_SET) < 0) {
      trace (ERROR, default_trace, "db_rollback (): fseek DEL error for trans "
	     "file (%s) for DB (%s) to fpos (%ld): (%s)\n", 
	     fname, db->name, fpos, strerror (errno));
      goto CLEAN_UP;
    }
    
    /* ... and reverse the operation */
    if (!fwrite (attr, 3, 1, db->db_fp)) {
      trace (ERROR, default_trace, "db_rollback (): undo DEL fwrite error for "
	     "trans file (%s) for DB (%s) attr string (%s)): (%s)\n", 
	     fname, db->name, attr, strerror (errno));
      goto CLEAN_UP;
    }
  }
  
  /* if we get here then there were no errors and we rolled back the DB */
  ret_code = 1;

CLEAN_UP:

  /* maintain original file status */
  if (reopenf == 0 && 
      db->db_fp != NULL) {
    fclose (db->db_fp);
    db->db_fp = NULL;
  }
  fclose (fin);

  return ret_code;
#ifdef notdef
trace (NORM, default_trace, "JW: rollback bye-bye! ret_code (%d)\n", ret_code);
#endif
}


/* Control the process of checking for transactions that did 
 * not complete and then rolling back the transaction.
 *
 * Input:
 *  -pointer to a struct of any DB's in which an incomplete
 *   transaction was detected and was rolled back.
 *
 * Return:
 *  -1 no errors occured in the transaction check process.
 *     if there were active transactions they are returned
 *     in the (ll) linked list. 
 *  -0 no determination can be made as an error occured during
 *     the process.  The error condition is written to log.
 */
int rollback_check (rollback_t *ll) {
  int ret_code = 0;
  char fname[BUFSIZE+1], *ufname;
  irr_database_t *db;
  DIR *dirp;

  /* sanity checks */

  /* is there a DB cache directory defined? */
  if (IRR.database_dir == NULL) {
    trace (NORM, default_trace, "rollback_check (): no DB cache defined.  "
	   "Nothing to do.\n");
    return 1;
  }
 
  /* can we read the DB cache dir? */
  if ((dirp = opendir (IRR.database_dir)) == NULL) {
    trace (ERROR, default_trace, "rollback_check (): Can't read the DB cache "
	   "dir (%s): (%s)\n", IRR.database_dir, strerror (errno));
    return 0;
  }

  /* loop through our DB list and look for a *.trans file.
   * if a *.trans file exists then roll back the DB if 
   * necessary. */
  LL_Iterate (IRR.ll_database, db) {
    /* does a transaction file exist for this DB? */
    sprintf (fname, "%s.trans", db->name);
    if (file_exists (dirp, fname)) {
      /* this is the transaction file name */
      sprintf (fname, "%s/%s.trans", IRR.database_dir, db->name);

      /* is this transaction active? ie, is the trans file complete and
       * was the trans interupted before finishing?  if yes, add the DB 
       * to our linked list and roll the DB back to its state before the 
       * transaction. */
      switch (active_trans (db, fname)) {
      case 1: /* active transaction */
	trace (NORM, default_trace, "JW: rollback_check (): active_trans () "
	       "calling db_rollback...\n");
	add_rbtrans_obj (ll, create_rbtrans_obj (db, fname));

	/* rollback the DB to its original state */
	if (!db_rollback (db, fname)) {
	  trace (ERROR, default_trace, "rollback_check (): db_rollback "
		 "error.  Aborting check process.\n");
	  goto CLEAN_UP;
	}

	/* roll back the journal if we are doing IRRd style mirroring */
	if (((db->flags & IRR_AUTHORITATIVE) || db->mirror_prefix != NULL) &&
	    !journal_rollback (db, fname)) {
	  trace (ERROR, default_trace, "rollback_check (): journal_rollback "
		 "error.  Aborting check process.\n");
	  goto CLEAN_UP;
	}
	break;
      case -1: /* I/O error, couldn't determine trans outcome */
	goto CLEAN_UP;
      default: /* transaction not active */
	if ((ufname = get_ufname (fname)) == NULL) {
	  trace (ERROR, default_trace, "rollback_check (): get_ufname "
		 "error.  Aborting reapply process.\n");
	  goto CLEAN_UP;
	}
	
	/* clean up old transaction files */
	remove (ufname);
	remove (fname);
	free   (ufname);
	break;
      }
    }
  }
  
  /* if we get here there were no errors */
  ret_code = 1;

CLEAN_UP:

  closedir (dirp);
trace (NORM, default_trace, "JW: exiting rollback_check (return 1)\n");
  return ret_code;
}

/* Control the process of re-applying any transactions which
 * did not fully complete.
 *
 * Function should be called on bootstrap.
 * 
 * Input:
 *  -pointer to a struct of any DB's in which an incomplete
 *   transaction was detected and should be re-applied.
 *
 * Return:
 *  -1 no errors occured in the transaction re-application process.
 *  -0 an error occured such that the re-application process 
 *     could not be carried out.
 */
int reapply_transaction (rollback_t *ll) {
  char *p, *ufname;
  trans_t *t;
  FILE *ufin;

  /* look through the linked list of DB's for which a transaction
   * did not complete */
  for (t = ll->first; t != NULL; t = t->next) {
    if ((ufname = get_ufname (t->tfile)) == NULL) {
      trace (ERROR, default_trace, "reapply_transaction (): get_ufname "
	     "error.  Aborting reapply process.\n");
      return 0;
    }

      /* open up the !us...!ue update file */
    if ((ufin = fopen (ufname, "r+")) == NULL) {
      trace (ERROR, default_trace, "reapply_transaction (): "
             "Can't re-open update file (%s) for DB (%s): (%s)\n", 
	     ufname, t->db->name, strerror (errno));
      return 0;
    }

    trace (NORM, default_trace, "JW: reapply_trans (): calling scan_irr_file "
	   "(%s)\n", ufname);

    /* reapply the !us...!ue update file and journal file (if necessary) */
    if ((p = scan_irr_file (t->db, "update", 1, ufin)) != NULL)
      trace (ERROR, default_trace, "reapply_transaction (): scan_irr_file () "
	     "error, couldn't re-apply transaction: (%s)\n", p);
    /* update the journal for pre-rpsdit mirror and authoritative DB's */
    else if (((t->db->flags & IRR_AUTHORITATIVE) || 
	      t->db->mirror_prefix != NULL) &&
	     !update_journal (ufin, t->db, -1)) {
      trace (ERROR, default_trace, "error in updating journal for DB (%s) "
	     "rolling back transaction\n", t->db->name);
      p = "";
    }

    fclose (ufin);

    /* cleanup */
    if (p == NULL) {
      remove (t->tfile);
      remove (ufname);
      free   (ufname);
      trace (NORM, default_trace, "reapply_transaction (): DB (%s) "
	     "reapplied transaction successfully.\n", t->db->name);
    }
    else
      return 0;
  }

  free_rbtrans_list (ll);

  /* no errors, successful reapply transaction(s) */
  return 1;
}

/* For DB (db->name) determine if a transaction was in 
 * progress at the time of a system crash and return
 * the name of the transaction file.
 *
 * The definition of an "active transaction" is a transaction
 * for which there was a complete transaction file built and
 * the transaction was interupted before it could complete.
 * 
 * The transaction steps are like this:
 * 1. build a transaction file, the first and last lines
 *    are the current serial number for the transaction.
 *    The last step is to write the current serial number
 *    in the last line of the file and so a complete trans
 *    file can be determined by comparing the first and last
 *    lines of the file.
 * 2. irrd updates the DB with the trans (ie, call scan_irr_file ()).
 * 3. Update the journal (if necessary; for rpsdist files irrd does
 *    not keep a journal) and the last step is to update the
 *    current serial file.  So the current serial number in the 
 *    transaction file can be compared with the current serial
 *    number in the DB current serial to determine if a transaction
 *    completed.
 * 
 * The presence of a transaction file does not mean
 * the DB is in an inconsistent state.  There are 3 cases,
 * 1 of which action needs to be taken:
 *
 * Case 1.  Transaction file is present but is not complete.
 *  This means the system crashed while the transaction file
 *  was being built so there were no changes made to the DB.  
 *  For this case we remove the incomplete transaction file and no
 *  further action is taken.
 *
 * Case 2. Transaction file is present and is complete and the
 *  transaction current serial matches the DB current serial.
 * 
 *  Then the system crashed after the transaction had been
 *  fully applied and the transaction file should be removed.
 *  No other action is necessary.
 *
 * Case 3. Transaction file is present and is complete and the
 *  transaction current serial does not match the DB current serial.
 *
 *  Then the system crashed during the DB update.  For this case we 
 *  rollback the DB (and journal file if necessary) and re-apply the 
 *  update file.
 *
 * Input:
 *  -pointer to the DB struct for the DB we are checking (db)
 *  -char template to return the name of the transaction file
 *   if the transaction should be re-applied.
 *
 * Return:
 *  -1 if the transaction should be re-applied (Case 3 above)
 *     [Note: transaction file name is returned in (fname).]
 *  -0 Case 1 and 2
 *  -1 an error occured and the result could not be determined
 *
 *  Function will remove transaction files in cases 1 and 2 above.
 */
int active_trans (irr_database_t *db, char *fname) {
  long ts;
  FILE *fin;

  /* sanity check */
  if (fname == NULL) {
    trace (ERROR, default_trace, "active_trans ():  NULL transcation file "
	   "name.\n");
    return -1;
  }

  trace (NORM, default_trace, "JW: rollback fname (%s)\n", fname);
  
  /* open the transaction file which is used to restore the DB */  
  if ((fin = fopen (fname, "r")) == NULL) {
    trace (ERROR, default_trace, "active_trans ():  Can't open atomic "
	   "transaction file (%s) for DB (%s): (%s)\n", 
	   fname, db->name, strerror (errno));
    return -1;
  }
  
  /* Case #1: was the transaction file built completely? 
   * if not then no changes were made to the DB 
   * and the trans was not active */
  if ((ts = complete_trans_file (fin, fname, db->name)) <= 0) {
    /* Error: trans file I/O error */
    if (ts < 0)
      trace (NORM, default_trace, "active_trans (): transaction file (%s) "
	     "error.\n", fname);
    /* Case #1: incomplete trans file */
    else
      trace (NORM, default_trace, "active_trans (): transaction file (%s) "
	     "was not complete.\n", fname);
    
    return ts;
  }

  trace (NORM, default_trace, "JW: active_trans ts (%ld)\n", ts);

  /* Case #3: if there is no serial then it's probably a rpsdist DB;
   * if it's not an rpsdist DB we can re-apply the trans
   * anyway to try and fix the DB */
  if (!scan_irr_serial (db))
    return 1;

  /* Case #2 or Case #3: trans is active if the DB cs is less than 
   * the trans cs */
trace (NORM, default_trace, "JW: active_trans returns (ts, db->cs)=(%ld, %ld) (%d)\n", ts, (long) db->serial_number, ts > ((long) db->serial_number));
  return (ts > ((long) db->serial_number));
}


/* Determine if transaction file (fname) is complete or not.
 *
 * Input:
 *  -stream file pointer to the transaction file (fin)
 *  -file name of the transaction file (fname)
 *  -DB name associated with the transaction file (dbname)
 *
 * Return:
 *  -serial of the transaction if the transaction file is compelete
 *  -0 if the transaction file is not complete
 *  --1 if there was a file I/O error
 */
long complete_trans_file (FILE *fin, char *fname, char *dbname) {
  int ret_code = -1;
  long cs1 = 0, cs2 = 0, save_pos;

  /* restore original file position on exit */
  save_pos = ftell (fin);

  if (fseek (fin, 0L, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "complete_trans_file (): trans file I/O "
	   "rewind file op error on trans file (%s) for DB (%s): %s.\n",
	   fname, dbname, strerror (errno));
    return -1;
  }
  
  /* read the current serial from the first line ... */    
  if (fscanf (fin, "ts: %ld\n", &cs1) != 1) {
    trace (NORM, default_trace, "complete_trans_file ():  Can't read line 1 "
	   "cs (%ld) for trans file (%s) for DB (%s)\n", cs1, fname, dbname);
    ret_code = 0;
    goto CLEAN_UP;
  }
  
trace (NORM, default_trace, "JW: complete_trans_file (): ts from first line of file (%ld)\n",
       cs1);

  /* ... seek the last line ... */
  if (fseek (fin, 0, SEEK_END) < 0 ||
      (cs2 = ftell (fin)) < 0) {
    trace (ERROR, default_trace, "complete_trans_file (): trans file I/O "
	   "seek EOF file op error on trans file (%s) for DB (%s): %s.\n",
	   fname, dbname, strerror (errno));
    goto CLEAN_UP;
  }

  /* if file is too small it cannot be complete */
  if (cs2 < 42) {
    ret_code = 0;
    goto CLEAN_UP;
  }

  /* read the last line */
  if (fseek (fin, cs2 - 15L, SEEK_SET)) {
    trace (ERROR, default_trace, "complete_trans_file ():  File I/O seek EOF "
	   "error on trans file (%s) for DB (%s): (%s)\n", 
	   fname, dbname, strerror (errno));
    goto CLEAN_UP;
  }

  /* ... read the current serial from the last line */    
  if (fscanf (fin, "ts: %ld\n", &cs2) != 1) {
    trace (NORM, default_trace, "complete_trans_file ():  Can't read last "
	   "line for trans file (%s) for DB (%s)\n", fname, dbname);
    ret_code = 0;
    goto CLEAN_UP;
  }

  trace (NORM, default_trace, "JW: complete_trans_file (): ts from last line of "
	 "file (%ld)\n", cs2);

  /* if we get here then we got cs1 and cs2 successfully
   * if they are equal then we have a complete transaction file */
  ret_code = (cs1 == cs2);

CLEAN_UP:

  /* restore the original file position */
  if (fseek (fin, save_pos, SEEK_SET) < 0)
    trace (ERROR, default_trace, "complete_trans_file (): trans file I/O "
	   "restore file pos  error on trans file (%s) for DB (%s): %s.\n", 
	   fname, dbname, strerror (errno));
  
  /* if they are the same we have a complete file */
  trace (NORM, default_trace, "JW: complete_trans_file (): return complete trans file? (%ld)\n", ((ret_code == 1) ? cs1 : (long) ret_code));

  /* return the serial # of the transaction ... */
  if (ret_code == 1)
    return cs1;

  /* ... otherwise return incomplete trans or I/O error */
  return ret_code;
}


/* Create a transaction object.
 *
 * Object will be part of a linked list of transactions
 * that should be re-applied because of a system crash.
 * 
 * Input:
 *  -pointer to the DB struct of the transaction.
 *  -full path name of the transaction file (fname)
 *
 * Return:
 *  -pointer to a trans_t record
 */
trans_t *create_rbtrans_obj (irr_database_t *db, char *fname) {
  trans_t *obj;

  obj = (trans_t *) malloc (sizeof (trans_t));
  obj->db    = db;
  obj->tfile = strdup (fname);

  return (obj);
}

/* Add (obj) to the end of the rollback transaction linked list.
 *
 * This list will be used by reapply_transction () to repply a transaction
 * which interupted by a system crash.
 *
 * Input:
 *  -pointer to the start of the transaction linked list (start)
 *  -object to be added to the list (obj)
 *
 * Return:
 *  -void
 */
void add_rbtrans_obj (rollback_t *start, trans_t *obj) {

  if (obj == NULL)
    return;

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

/* Free the rollback memory linked list.
 *
 * Input:
 *   -pointer to the start of the linked list (start)
 *
 * Return:
 *  void
 */
void free_rbtrans_list (rollback_t *start) {
  trans_t *obj, *temp;

  obj = start->first;
  while (obj != NULL) {
    /*fprintf (dfile, "trans object: %s %s %s state (%d) \n", 
      obj->key, obj->type, obj->source, obj->state);*/
    temp = obj->next;
    free (obj->tfile);
    free (obj);
    obj = temp;
  }
} 

/* Determine if file (fname) exists in the directory
 * pointed to by (dirp).
 *
 * Function expects (dirp) to be an open directory pointer.
 *
 * Input:
 *  -pointer to the directory to be searched (dirp)
 *  -name of the file we are looking for (fnam)
 *   [Note: fname should be unqualified.]
 *
 * Return:
 *  -1 if fname is found in the directory
 *  -0 otherwise
 */
int file_exists (DIR *dirp, char *fname) {
  struct dirent *dp;

  rewinddir (dirp);
  while ((dp = readdir (dirp)) != NULL) {
    if (!strcmp (dp->d_name, fname)) {
trace (NORM, default_trace, "JW: yes file_exists (%s)\n", fname);
      return 1;
    }
  }
  
trace (NORM, default_trace, "JW: no file_exists (%s)\n", fname);

  return 0;
}

/* Rollback the journal file and currentserial file for (db) to 
 * their original state before the last transaction.
 *
 * Function assumes the transaction file is complete.  Function
 * will restore the journal file to its original size and
 * reset the current serial file.
 *
 * Input;
 *  -pointer to the db struct for the journal we are rolling back (db)
 *  -fully qualified transaction file name (fname)
 *   [see build_transaction_file () for an explanation of the trans file.]
 *
 * Return:
 *  -1 if we restored the journal and current serial file to their
 *     original states without error
 *  -0 otherwise
 */
int journal_rollback (irr_database_t *db, char *fname) {
  FILE *fin;
  char buf[BUFSIZE+1];
  long cs, jsize;
  int ret_code = 0;

  /* sanity check */
  if (fname == NULL) {
    trace (ERROR, default_trace, "journal_rollback ():  NULL transcation file "
	   "name.\n");
    return 0;
  }

  /* is there a DB cache? */
  if (IRR.database_dir == NULL) {
    trace (ERROR, default_trace, "journal_rollback ():  NULL transcation file "
	   "name.\n");
    return 0;
  }

  trace (NORM, default_trace, "JW: journal_rollback fname (%s)\n", fname);
  
  /* open the transaction file which is used to restore the journal */  
  if ((fin = fopen (fname, "r")) == NULL) {
    trace (ERROR, default_trace, "journal_rollback (): Can't open atomic "
	   "transaction file (%s) for DB (%s): (%s)\n", 
	   fname, db->name, strerror (errno));
    return 0;
  }

  /* function assumes the transaction file built completely */

  /* read past the original current serial, journal file length,
   * and !us...!ue update file name */
  if (fgets  (buf, BUFSIZE, fin)    == NULL ||
      fscanf (fin, "%ld\n", &cs)    != 1    ||
      fscanf (fin, "%ld\n", &jsize) != 1) {
    trace (ERROR, default_trace, "journal_rollback ():  Can't read atomic "
	   "transaction file (%s) for DB (%s) the orig CS/update file: (%s):"
	   "\n", fname, db->name, strerror (errno));
    goto CLEAN_UP;
  } 

  trace (NORM, default_trace, "JW: journal_rollback cs (%ld) jsize (%ld)\n", 
	 cs, jsize);
  

  /* truncate the journal to its original size */
  make_journal_name (db->name, JOURNAL_NEW, buf);
  if (truncate (buf, jsize) < 0) {
    trace (ERROR, default_trace, "journal_rollback ():  Can't truncate journal"
	   "file (%s) to (%ld) bytes: (%s)\n", buf, jsize, strerror (errno));
    goto CLEAN_UP;
  }
  
  /* set the original current serial */
  db->serial_number = cs;
  if (!write_irr_serial (db)) {
    trace (ERROR, default_trace, "journal_rollback ():  write_irr_serial () "
	   "error.\n");
    goto CLEAN_UP;
  }

  /* if we get here then there were no errors and we 
   * rolled back the journal and current serial file */
  ret_code = 1;

CLEAN_UP:

  fclose (fin);
  return ret_code;
#ifdef notdef
trace (NORM, default_trace, "JW: journal_rollback () bye-bye!\n");
#endif
}

/* Get the update file name (ie, the !us...!ue name) from the transaction
 * file.
 *
 * Input:
 *  -full path name of the transaction file (tfname)
 *
 * Return:
 *  -the name of the update file
 *  -NULL if there was an error and the update file name could
 *   not be obtained.
 */
char *get_ufname (char *tfname) {
  int n;
  char buf[BUFSIZE+1];
  FILE *fin;

  /* sanity check */
  if (tfname == NULL) {
    trace (ERROR, default_trace, "get_ufname (): NULL transcation file "
	   "name.\n");
    return NULL;
  }

  trace (NORM, default_trace, "JW: get_ufname (%s)\n", tfname);
  
  /* open the transaction file  */  
  if ((fin = fopen (tfname, "r")) == NULL) {
    trace (ERROR, default_trace, "get_ufname ():  Can't open atomic "
	   "transaction file (%s): (%s)\n", tfname, strerror (errno));
    return NULL;
  }

  /* read past the cs, original current serial, journal file length,
   * and get the !us...!ue update file name */
  if (fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL ||
      fgets (buf, BUFSIZE, fin) == NULL) {
    trace (ERROR, default_trace, "get_ufname ():  Can't read atomic "
	   "transaction file (%s) fgets () error: (%s)\n", 
	   tfname, strerror (errno));
    buf[0] = '\0';
    goto CLEAN_UP;
  } 

  /* remove the '\n' */
  if ((n = strlen (buf)) > 0)
    buf[n - 1] = '\0';

CLEAN_UP:

  fclose (fin);

  if (buf[0] == '\0')
    return NULL;

  return strdup (buf);
}

/* Update the journal file for DB (db->name) with the updates from
 * a transaction.
 *
 * Function writes the update entries to the journal and updates
 * the current serial file.
 *
 * Input:
 *  -file descriptor to the transaction file (!us...!ue) (fin)
 *  -pointer to the DB struct (db)
 *  -number of updates in this transaction, used for debug purposes (n_updates)
 *
 * Return:
 *  -1 if the transaction was successfully written to the journal
 *  -0 otherwise
 */
int update_journal (FILE *fin, irr_database_t *db, int n_updates) {
  char buf[BUFSIZE+1], tag[64];
  int n = 0, bl = 0;
  long save_fpos;

  /* sanity check */
  if (fin == NULL) {
    trace (ERROR, default_trace, "update_journal (): "
	   "NULL update file pointer.  Can't update the journal for DB (%s)", 
	   db->name);
    return 0;
  }

  /* rewind the update file */
  if ((save_fpos = ftell (fin)) < 0 ||
      fseek (fin, 0L, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "update_journal (): "
	   "rewind file I/O error for DB (%s): (%s).\n", 
	   db->name, strerror (errno));
    return 0;
  }

  while (fgets (buf, BUFSIZE, fin) != NULL) {
    /* "%END" tells up we're done */
    if (!strncmp ("%END", buf, 4))
      break;
    
    /* write the serial stamp line */
    if (!memcmp (buf, "ADD", 3) ||
	!memcmp (buf, "DEL", 3)) {
      n++;
      sprintf (tag, "%% SERIAL %u\n", ++db->serial_number);
      if (write (db->journal_fd, tag, strlen (tag)) < 0) {
	trace (ERROR, default_trace, "update_journal (): file I/O write tag "
	       "error: (%s)\n", strerror (errno));
	return 0;
      }
    }
    
    /* insure's we are not dumping extra blank lines at the end of
     * the update file or before/after an update */
    if (buf[0] == '\n')
      bl++;
    else
      bl = 0;

    /* write a journal data line */
    if (bl < 2 &&
	write (db->journal_fd, buf, strlen (buf)) < 0) {
      trace (ERROR, default_trace, "update_journal (): file I/O write data "
	     "error: (%s)\n", strerror (errno));
      return 0;
    }
  }

  /* restore the original file position */
  if (fseek (fin, save_fpos, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "update_journal (): "
	   "restore file pos I/O error: (%s).\n", strerror (errno));
    return 0;
  }

  /* sanity check */
  if (n_updates > 0 && 
      n != n_updates) {
    trace (ERROR, default_trace, "update_journal (): number of updates "
	   "written to  journal (%d) does not match expected value (%d)\n", 
	   n, n_updates);
    return 0;    
  }


  /* the last step is to update the current serial file */
  return write_irr_serial (db); 
}
