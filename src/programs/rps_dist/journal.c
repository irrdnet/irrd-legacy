#include <stdio.h>
#include "rpsdist.h"
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <irrd_ops.h>

extern trace_t *default_trace;
extern config_info_t ci;

/*
 *  Given the name of a DB, see if there's a journal.  If so, fill in the
 *  current serial.  If not, make a new one, cs = 0
 */
void serial_from_journal(rps_database_t * db){
  char curline[MAXLINE];
  int need_new = 0;

  /* sanity check, avoid seg faults */
  if(!db || !db->name){
    trace(ERROR, default_trace, "serial_from_journal: Corrupted db_list (NULL)\n");
    return;
  }
  
  /* open <db_name>.JOURNAL for reading */
  db->journal = open_db_journal(db->name, "r+");

  /* if file opened successfully */
  if(db->journal){
    /* get the first line, create new if we hit EOF */
    if( !fgets(curline, MAXLINE, db->journal) ){
      trace(NORM, default_trace, "serial_from_journal: Journal file for %s, first line was empty, creating new\n", db->name);
      need_new = 1;
    }
    
    if( !strncasecmp(curline, "#start-serial: ", 15) ){
      newline_to_null(curline);
      if( ((db->first_serial = atol(curline+15)) == 0) && (errno == ERANGE) ){
	trace(ERROR, default_trace, "serial_from_journal: %s serial was out of range\n", db->name);
	db->first_serial = -1;
      }
    }
     
    /* get the second line */
    if( !fgets(curline, MAXLINE, db->journal) ){
      trace(ERROR, default_trace, "serial_from_journal: Journal file for %s, second line was empty\n", db->name);
      need_new = 1;
    } 

    if( !strncasecmp(curline, "#end-serial: ", 13) ){
      newline_to_null(curline);
      if( ((db->current_serial = atol(curline+13)) == 0) && (errno == ERANGE) ){
	trace(ERROR, default_trace, "serial_from_journal: %s serial was out of range\n", db->name);
	db->current_serial = -1;
      }
    }
  } 
  else   /* if error opening file */
    need_new = 1;
  
  if(need_new)
    start_new_journal(db, 0);
}

/* create a new journal containing:
 * #start-serial: <start>
 * #end-serial: <start>
 *
 * #EOF
 */
void start_new_journal(rps_database_t * db, long start){
  db->journal = open_db_journal(db->name, "w+");
  if(db->journal){
    journal_write_start_serial(db, start);
    journal_write_end_serial(db, start);
    journal_write_eof(db);
    db->first_serial = db->current_serial = start;
  }
  else{
    trace(ERROR, default_trace, "start_new_journal: Error opening journal for db %s\n", db->name);
    fprintf(stderr, "FATAL: error creating journal for %s\n", db->name);
    exit(EXIT_ERROR);
  }
}

/* 
 * To start from the beginning and go line by line looking for something
 * that should be near the end is wasteful.  Start from the end and work
 * backward.
 */
long find_eof(FILE * fp){
  char buf[4096];
  long old_pos = ftell(fp);      /* save old position */
  long end;
  long cur, found = 0;
  char * eof;
  int full_times, i;
  
  fseek(fp, 0, SEEK_END);        /* move to end */
  end = ftell(fp);               /* note end */
  full_times = end/4095;
  cur = end;
  for( i = 0; i < full_times; i++){
    cur = cur - 4095;
    fseek(fp, cur, SEEK_SET);
    fread(buf, 1, 4095, fp);
    buf[4096] = '\0';
    if( (eof = strstr(buf, "\n#EOF\n")) != NULL ){
      cur += (eof - buf);
      found = cur;
    }
  }
  if(!found){
    fseek(fp, 0, SEEK_SET);
    fread(buf, 1, 4095, fp);
    buf[4096] = '\0';
    if( (eof = strstr(buf, "\n#EOF\n")) != NULL ){
      cur = (eof - buf);
      found = cur;
    }
  }
  fseek(fp, old_pos, SEEK_SET);
  
  return found+1; /* +1 to account for leading newline */
}

/*
 * For a given DB's journal, write the given serial number to the
 * field in the beginning of the file labelled: "end-serial:"
 */
int journal_write_end_serial(rps_database_t * db, long serial){
  long old_position;

  if(!db->journal) {
    if( (db->journal = open_db_journal(db->name, "r+")) == NULL){
      trace(ERROR, default_trace, "journal_write_end_serial: Could not open journal for %s: %s\n", db->name, strerror(errno));
      return -1;
    }
  }
  
  /* save the old position */
  old_position = ftell(db->journal);
  
  /* seek to beginning, write entire string to be safe */
  fseek(db->journal, 26, SEEK_SET);
  fprintf(db->journal, "#end-serial: %-10ld\n", serial);
  fflush(db->journal);

  /* restore old position */
  fseek(db->journal, old_position, SEEK_SET);
  return 1;
}
  
/*
 * for the given DB, write the given serial to the field
 * near the start of the file labelled "start-serial:"
 */
int journal_write_start_serial(rps_database_t * db, long serial){
  long old_position;
  
  if(!db->journal) {
    if( (db->journal = open_db_journal(db->name, "r+")) == NULL){
      trace(ERROR, default_trace, "journal_write_start_serial: Could not open journal for %s: %s\n", db->name, strerror(errno));
      return -1;
    }
  }
  
  /* save the old position */
  old_position = ftell(db->journal);
  
  /* seek to beginning, write entire string to be safe */
  fseek(db->journal, 0, SEEK_SET);
  fprintf(db->journal, "#start-serial: %-10ld\n", serial);
  fflush(db->journal);
  
  /* restore old position */
  fseek(db->journal, old_position, SEEK_SET);
  return 1;
}

/*
 * wherever the journal currently is, write an "#EOF" at the end
 */
int journal_write_eof(rps_database_t * db){
  if( !db ){
    trace(ERROR, default_trace, "journal_write_eof: Write EOF: FILE * == NULL\n");
    return 0;
  }
  if( !db->journal ){
    if( (db->journal = open_db_journal(db->name, "r+")) == NULL){
      trace(ERROR, default_trace, "journal_write_eof: Could not open journal for %s: %s\n", db->name, strerror(errno));
      return -1;
    }
  }
  fseek(db->journal, 0, SEEK_END);
  fprintf(db->journal, "\n#EOF\n");
  fflush(db->journal);
  
  return 1;
}     

/*
 * append the files named in tmp_nam and sig to the journal.
 * tmp_nam is the transaction, sig is usually the detached repository signature
 */
char *
append_jentry_to_db_journal(rps_database_t * db, char * jname){
  char * ret_code;
  if( !fseek(db->journal, -5, SEEK_END) ){
    fprintf(db->journal, "#serial %ld\n\n", db->current_serial+1);
    write_trans_begin(db->journal, jname);
    if( (ret_code = append_file_to_file(db->journal, jname, NULL)) != NULL)
      return ret_code;
    fprintf(db->journal, "\n#EOF\n");
    fflush(db->journal);
    return NULL;
  }
  else
    return "append_file_to_db_journal: fseek error";
}

