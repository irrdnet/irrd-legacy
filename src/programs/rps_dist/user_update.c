
#include <stdio.h>
#include "rpsdist.h"
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <irrd_ops.h>
#include <errno.h>
#include <sys/time.h>
#include <pgp.h>

extern trace_t *default_trace;
extern char tmpfntmpl[];
FILE * dfile;
extern FILE * ofile;
extern struct select_master_t mirror_floods;           /* the select master for the user sockets */
extern char * sign_name, * sign_pass, * sign_hexid;

/* 
 *  We've checked in parse_input, irr->tmp starts with a !us
 *  now check that the db is valid, and handle the submission.
 *  The submission will be as follows:
 *    !us<dbname>
 *    <!us file name>
 *    <pgp file name or "NULL">
 *    <update from user file name>
 *    !ue
 *   RETURN:
 *      1 successful
 *      0 failure (a.k.a. socket needs to be flushed)
 */
int handle_user_submit(rps_connection_t * irr){
  char * db_name = irr->tmp + 3;
  rps_database_t * db = find_database(db_name);
  char * ret_code;
  char buf[MAXLINE];
  irr_submit_info_t irr_info = {NULL, NULL, NULL};

  /* if we find it and we're auth, ok */
  if( db && db->authoritative ){
    
    /* read the !us...!ue */
    if( (ret_code = read_irr_submit_info(irr, &irr_info)) != NULL)
      return user_cleanup(irr, db, ret_code);
    
    /* lock */
    pthread_mutex_lock(&db->lock);
    
    /* rename files */
    if( (ret_code = rename_irr_submit_files(&irr_info, db)) != NULL)
      return user_cleanup(irr, db, ret_code);

    /* build db_trans_xxx_file */
    if( (ret_code = build_db_trans_xxx_file(db, irr_info.pgp_name)) != NULL)
       return user_cleanup(irr, db, ret_code);

    /* fill in the sequence number and sign */
    if( (ret_code = process_jentry_file(db)) != NULL )
      return user_cleanup(irr, db, ret_code);

    /* append the jentry to the journal */
    if( (ret_code = append_jentry_to_db_journal(db, irr_info.upd_name)) != NULL)
	return user_cleanup(irr, db, ret_code);
    
    /* send serial to irr_submit */
    sprintf(buf, "%ld\n", db->current_serial+1);
    safe_write(irr->sockfd, buf, strlen(buf));

    /* send <db>.<cs+1> file to irrd */
    if( (ret_code = send_db_xxx_to_irrd(db)) == NULL ) { /* success */
      journal_write_end_serial(db, db->current_serial + 1);
      db->current_serial++;
      /* send the result to irr_submit */
      safe_write(irr->sockfd, "C\n", 2);
    } else {                                             /* failure */
      trans_rollback(db);
       /* send the result to irr_submit */
      safe_write(irr->sockfd, ret_code, strlen(ret_code));
    }

    /* send onward */
    forward_file_to_peers(irr_info.upd_name, &mirror_floods, 0);

    /* remove crash files, pgp files */
    remove_db_crash_files(db->name, 0);
    remove_db_pgp_copies(db->name);
    
    /* unlock */
    pthread_mutex_unlock(&db->lock);
  } else {                                             /* unauthoritative db */
    trace(ERROR, default_trace, "Attempted update of a %s (non-authoritative)", db->name);
    snprintf(buf, MAXLINE-1, "Frps_dist: Not authorized to update DB\n");
    /*send_irr_submit_buffer(irr->sockfd, buf);*/
    safe_write(irr->sockfd, buf, strlen(buf));
    return 0;
  }
  
  return 1;
}

/*
 *  Something failed somewhere, nlock the DB and send the response to irr_submit
 */
int 
user_cleanup(rps_connection_t * irr, rps_database_t * db, char * ret_code){
  pthread_mutex_unlock(&db->lock);
  safe_write(irr->sockfd, ret_code, strlen(ret_code));
  return 0;
}

/*
 *  irr_submit sends us very little info:
 *    three file names basically
 */
char * read_irr_submit_info(rps_connection_t * irr, irr_submit_info_t * irr_info){
  /* we've already read the !us */
  read_buffered_line(default_trace, irr);
  newline_to_null(irr->tmp);
  irr_info->us_name = strdup(irr->tmp);

  read_buffered_line(default_trace, irr);
  newline_to_null(irr->tmp);
  irr_info->pgp_name = strdup(irr->tmp);

  read_buffered_line(default_trace, irr);
  newline_to_null(irr->tmp);
  irr_info->upd_name = strdup(irr->tmp);

  read_buffered_line(default_trace, irr);
   if( !strncmp(irr->tmp, "!ue", 3) )
     return NULL;
   else
     return "Fread_irr_submit_info: Invalid header, no !ue";

}

/*
 * Given the original names from irr_submit (irr_submit.XXXX), rename them to the expected
 * names so we can check them upon bootstrap should we crash
 */
char * rename_irr_submit_files(irr_submit_info_t * irr_info, rps_database_t * db){
  char buf[MAXLINE];
  
  snprintf(buf, MAXLINE, "%s/%s.%ld",ci.db_dir, db->name, db->current_serial+1);
  if( rename(irr_info->us_name, buf) == -1 )
    return strerror(errno);
  else{
    free(irr_info->us_name);
    irr_info->us_name = strdup(buf);
  }
    
  if( strcmp(irr_info->pgp_name, "NULL") ){
    snprintf(buf, MAXLINE, "%s/%s.pgp.%ld", ci.db_dir, db->name, db->current_serial+1);
    if( rename(irr_info->pgp_name, buf) == -1)
      return strerror(errno);
    else{
      free(irr_info->pgp_name);
      irr_info->pgp_name = strdup(buf);
    }
  } 
  
  snprintf(buf, MAXLINE, "%s/%s.jentry.%ld", ci.db_dir, db->name, db->current_serial+1);
  if( rename(irr_info->upd_name, buf) == -1 )
    return strerror(errno);
  else{
    free(irr_info->upd_name);
    irr_info->upd_name = strdup(buf);
  }
  
  return NULL;
}

/*
 * irr_submit creates the jentry with the transaction-label object,
 * we just need to go to the second line and insert the serial.  Then
 * we can sign it.
 */
char * process_jentry_file(rps_database_t * db){
  FILE * infile = open_db_jentry_xxx(db->name, "r+", db->current_serial+1);
  int found;
  long fpos = 0;
  char buf[MAXLINE];
  char pgppath[MAXLINE];
  char * detsigfile, *s;

  if(infile){
    while( fgets(buf, MAXLINE, infile) ){
      if( !strncmp(buf, "sequence:", 9) ){
	found = 1;
	break;
      }
      fpos = ftell(infile);
    }
    
    if(!found){
      fclose(infile);
      return "FInvalid jentry file, no sequence field";
    }
    
    fseek(infile, fpos, SEEK_SET);
    fprintf(infile, "sequence: %-10ld\n", db->current_serial+1);
    fseek(infile, 0, SEEK_END);
    fprintf(infile, "\nrepository-signature: %s\n", db->name); /* XXX - integrity? */
    fclose(infile);

    snprintf(buf, MAXLINE, "%s/%s.jentry.%ld", ci.db_dir, db->name, db->current_serial+1);
    snprintf(pgppath, MAXLINE, "%s/%s/", ci.pgp_dir, db->name);
    detsigfile = tempnam(ci.db_dir, "mtr.");

    if( !pgp_sign_detached(default_trace, pgppath, db->pgppass, db->hexid, buf, detsigfile) ){
      trace(ERROR, default_trace, "process_jentry_file: error signing user file\n");
      return "Fprocess_jentry_file: error signing file\n";
    }
    
    /* append the signature */
    if( (infile = open_db_jentry_xxx(db->name, "a", db->current_serial+1)) == NULL ){
      trace(ERROR, default_trace, "process_jentry_file: error opening jentry for append: %s\n", strerror(errno));
      return "FError opening jentry file";
    }
    
    fprintf(infile,"signature:\n");
    if( (s = append_file_to_file(infile, detsigfile, "+ ")) != NULL ){
      trace(ERROR, default_trace, "process_jentry_file: Error appending sig\n");
      return "FError appending signature";
    }
    
    remove(detsigfile);
    free(detsigfile);
    fclose(infile);
  }
  else
    return "FError opening jentry file for processing";
  
  return NULL;
}
