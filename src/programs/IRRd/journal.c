/*
 * $Id: journal.c,v 1.11 2002/10/17 20:02:30 ljb Exp $
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
void journal_irr_update (irr_database_t *db, irr_object_t *object, 
                         int mode, int skip_obj) {
  char buffer[BUFSIZE+1];
  char *sadd = "ADD\n\n";
  char *sdelete = "DEL\n\n";
  int len_to_read, read_bytes, fread_len;

  db->serial_number++;

  /* until we have our syntax checker, just copy the serial as is */
#ifdef notdef /* serial numbers will get messed up if we actually skip */
  if (!skip_obj)	/* don't put bogus/filtered objects in journal file */
#endif
  {
    journal_log_serial_number (db);

    if (mode == IRR_UPDATE) 
      write (db->journal_fd, sadd, strlen (sadd));
    else if (mode == IRR_DELETE) 
      write (db->journal_fd, sdelete, strlen (sdelete));
    else {
      trace (ERROR, default_trace,"ERROR journal.c: journal_write_serial(): unrecognized mode (%d) db-(%s)\n", mode, db->name);
      return;
      /*
      write (db->journal_fd, snomode, strlen (snomode));
      */
    }

    fseek (object->fp, object->offset, SEEK_SET);

    len_to_read = object->len;    
    while (len_to_read > 0) {
      if  (len_to_read > BUFSIZE)
	read_bytes = BUFSIZE;
      else
	read_bytes = len_to_read;
      fread_len = fread (buffer, 1, read_bytes, object->fp);
      len_to_read -= fread_len;
      write (db->journal_fd, buffer, fread_len);
    }
    fread_len = fread (buffer, 1, 1, object->fp); /* read terminal newline */
    if (fread_len !=1)
      trace (ERROR, default_trace,"journal_irr_update(): Error reading final terminating newline\n");
    if (buffer[0] != '\n') 
      trace (ERROR, default_trace,"journal_irr_update(): Terminating newline characacter incorrect, value = %d\n",buffer[0]);
    write (db->journal_fd, "\n", 1); /* write the final newline */
  }

  write_irr_serial (db);
  return;
}

/* journal_log_serial_number
 * For crash recovery and when we act as a mirror server,
 * stamp the <DB>.journal file with the current serial 
 * number for the database
 */
void journal_log_serial_number (irr_database_t *database) {
  char buffer[512];
  sprintf (buffer, "%s SERIAL %u\n", "%", database->serial_number);
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
int find_oldest_serial (char *dbname, int journal_ext, uint32_t *oldestserial) {
  char file[256], buf[BUFSIZE];
  FILE *fp;

  make_journal_name (dbname, journal_ext, file);

  if ((fp = fopen (file, "r")) != NULL) {
    while (fgets (buf, BUFSIZE, fp) != NULL) { 
      if (!strncmp (buf, "% SERIAL", 8)) {
	fclose (fp);
        if (convert_to_32 (buf+9, oldestserial) < 0)
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
int get_current_serial (char *dbname, uint32_t *currserial) {
  char tmp[BUFSIZE], file[256];
  int ret_val = -1;
  FILE *fp;

  strcpy (tmp, dbname);
  convert_toupper(tmp);

  sprintf (file, "%s/%s.CURRENTSERIAL", IRR.database_dir, tmp);

  if ((fp = fopen (file, "r")) != NULL) {
    memset (tmp, 0, sizeof (tmp));
    if (fgets (tmp, sizeof (tmp) - 1, fp) != NULL && 
        convert_to_32 (tmp, currserial) > 0)
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

int find_last_serial (char *dbname, int journal_ext, uint32_t *last_serial) {
  char file[256];
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
	  if (strncmp(buf+i, "% SERIAL",8) != 0)
	    goto FAIL;
	  if (convert_to_32 (buf+i+9, last_serial) < 0)
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
