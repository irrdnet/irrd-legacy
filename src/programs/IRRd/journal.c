/*
 * $Id: journal.c,v 1.9 2002/02/04 20:53:56 ljb Exp $
 * originally Id: journal.c,v 1.13 1998/06/23 23:30:44 gerald Exp 
 */



#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "trace.h"
#include <time.h>
#include <signal.h>
#include "config_file.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "irrd.h"

extern trace_t *default_trace;
void make_journal_name (char * dbname, int journal_ext, char * journal_name);

/* journal_irr_update
 * Record what we're doing to the database in the <DB>.journal file
 * if mode == IRR_UPDATE then an add
*/
/* JW this routine can be speeded up.  We know the offset and length
   of the object, lets read it and write it with two sys calls
*/
void journal_write_serial (int mode, FILE *src_fp, int offset, irr_database_t *database) {
  char buffer[BUFSIZE+1];
  char *sadd = "ADD\n\n";
  char *sdelete = "DEL\n\n";
  /*
  char *snomode = "\n\n";
  */
  
  journal_log_serial_number (database);

  if (mode == IRR_UPDATE) 
    write (database->journal_fd, sadd, strlen (sadd));
  else if (mode == IRR_DELETE) 
    write (database->journal_fd, sdelete, strlen (sdelete));
  else {
    trace (ERROR, default_trace,"ERROR journal.c: journal_write_serial(): unrecognized mode (%d) db-(%s)\n", mode, database->name);
    return;
    /*
    write (database->journal_fd, snomode, strlen (snomode));
    */
  }

  fseek (src_fp, offset, SEEK_SET);
    
  while (fgets (buffer, BUFSIZE, src_fp) != NULL) {
    if (strlen (buffer) < 2) break;
    write (database->journal_fd, buffer, strlen (buffer));
  }
  strcpy (buffer, "\n");
  write (database->journal_fd, buffer, strlen (buffer));

  /* whew! The change is on disk, so we can save the new serial number */
  write_irr_serial (database); 
  
  return;
}


void journal_irr_update (irr_database_t *db, irr_object_t *object, 
                         int mode, u_long db_offset) {

  db->serial_number++;

  /* until we have our syntax checker, just copy the serial as is */
  journal_write_serial (mode, object->fp, object->offset, db);
}

/* journal_log_serial_number
 * For crash recovery and when we act as a mirror server,
 * stamp the <DB>.journal file with the current serial 
 * number for the database
 */
void journal_log_serial_number (irr_database_t *database) {
  char buffer[512];
  sprintf (buffer, "%s SERIAL %ld\n", "%", database->serial_number);
  write (database->journal_fd, buffer, strlen (buffer));
  return;
}


/* if the journal file is too big, roll it over and create a <db>.JOURNAL.old
 * or something like that 
 */
void journal_maybe_rollover (irr_database_t *database) {
  struct stat buf;
  char file_old[BUFSIZE], file_new[BUFSIZE];

  fstat(database->journal_fd, &buf);
  if (buf.st_size > IRR_MAX_JOURNAL_SIZE) {

    trace (NORM, default_trace, "Rolling Journal file (> %d bytes) for %s\n",
	   IRR_MAX_JOURNAL_SIZE, database->name);
    
    make_journal_name (database->name, JOURNAL_NEW, file_new);
    make_journal_name (database->name, JOURNAL_OLD, file_old);

    close (database->journal_fd);
    rename(file_new, file_old);

    if ((database->journal_fd = open (file_new, O_RDWR | O_CREAT, 0664)) < 0) 
      trace (NORM, default_trace, "**** ERROR **** Could not open %s (%s)!\n", 
	     file_new, strerror (errno));
  }
  return;
}

/*
 * return 1 if *.JOURNAL file exists and a valid serial # header is found
 * otherwise return 0
 * return value of first good serial found if possible
 */
int find_oldest_serial (char *dbname, int journal_ext, u_long *oldestserial) {
  char file[BUFSIZE], buf[BUFSIZE];
  FILE *fp;

  make_journal_name (dbname, journal_ext, file);

  if ((fp = fopen (file, "r")) != NULL) {
    while (fgets (buf, sizeof (buf) - 1, fp) != NULL) { 
      if (sscanf (buf, "%% SERIAL %s", file) == 1) {
	fclose (fp);
        if (convert_to_lu (file, oldestserial) < 0)
          return (0);
	return (1);
      }
    }
  }

  if (fp != NULL)
    fclose (fp);

  return (0);
}


/*
 * return 1 if *.CURRENTSERIAL is read without error
 * else return -1
 */
int get_current_serial (char *dbname, u_long *currserial) {
  char tmp[BUFSIZE], file[BUFSIZE];
  int ret_val = -1;
  FILE *fp;

  strcpy (tmp, dbname);
  convert_toupper(tmp);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, tmp);

  if ((fp = fopen (file, "r")) != NULL) {
    memset (tmp, 0, sizeof (tmp));
    if (fgets (tmp, sizeof (tmp) - 1, fp) != NULL && 
        convert_to_lu (tmp, currserial) > 0)
      ret_val = 1;
    fclose (fp);
  }

  return (ret_val);
}

void make_journal_name (char * dbname, int journal_ext, char * journal_name) {
  
 if (journal_ext == JOURNAL_NEW)
   sprintf (journal_name, "%s/%s.%s", IRR.database_dir, dbname, SJOURNAL_NEW); 
 else
   sprintf (journal_name, "%s/%s.%s", IRR.database_dir, dbname, SJOURNAL_OLD); 
}


#define FLS_BUFSIZE 2048
#define FLS_OVERFLOW 12 /* 99 billion + NL */

/*
 * return 0 if unable to find a serial number, or there are problems opening
 * or reading JOURNAL file.
 * return 1 if found
 */

int find_last_serial (char *dbname, int journal_ext, u_long *last_serial) {
  char file[BUFSIZE], sz_serialno[BUFSIZE];
  char buf [FLS_BUFSIZE + FLS_OVERFLOW + 1]; 
  off_t offset;
  ssize_t bytes_read, bytes_to_read = FLS_BUFSIZE;
  int off_by_one = 0;
  int i, fd = -1;
  FILE *fp;

  make_journal_name (dbname, journal_ext, file);

  if ((fp = fopen (file, "r")) != NULL) {
    fd = fileno (fp);

    memset(buf, 0, FLS_BUFSIZE + FLS_OVERFLOW + 1);

    /* Find end of file */
    offset = lseek (fd, 0L, SEEK_END);
    if (offset == -1) {
      goto FAIL;
    }

    while (offset > 0) {
      offset = MAX((offset - FLS_BUFSIZE), 0);

      /* We should really check return here */
      lseek(fd, offset, SEEK_SET);

      bytes_read = read (fd, buf, bytes_to_read);
      if (bytes_read <= 0) {
	/* We probably should throw a trace here */
        goto FAIL;
      }

      /* Start at end of buffer, work towards front looking for % SERIAL */
      for (i = bytes_read + off_by_one; (i >= 0) && (buf[i] != '%'); i--);
      off_by_one = 0;
      if ((i > 0) || ((i == 0) && (offset == 0))) {
        /* We've found the % */
	/* It either at start of file or has a preceding NL */
        if ((i == 0) || (buf[i-1] == '\n')) {
	  sscanf(buf+i, "%% SERIAL %s\n", sz_serialno);
	  if (convert_to_lu (sz_serialno, last_serial) < 0)
	    goto FAIL;
          else {
	    fclose (fp); 
	    return (1);
	  }
	  break;
        }
      }
      else if (i == 0) { /* And offset != 0 */
	off_by_one = 1;
      }

      if (offset <= bytes_to_read) { bytes_to_read = offset; }

      memcpy(buf + bytes_to_read, buf, ((FLS_OVERFLOW+FLS_BUFSIZE)-bytes_to_read));
    }
  }

FAIL:
  if (fd >= 0) { fclose (fp); }

  return (0);
}
