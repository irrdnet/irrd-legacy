#include "rpsdist.h"
#include <stdarg.h>
#include <trace.h>
#include <regex.h>
#include <unistd.h>
#include <strings.h>
#include <irrd_ops.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>

extern trace_t * default_trace;

/*
 * Read from old, write to new
 */
char * fcopy(char * srcnm, char * dstnm){
  char buf[50000];
  FILE * src = fopen(srcnm, "r");
  FILE * dst = fopen(dstnm, "w");
  int total_read = 0, total_wrote = 0;
  int cur_read, cur_wrote;
  
  if( !src && dst){ /* source didn't exist, dst now empty, NOT AN ERROR */
    fclose(dst);
    return NULL;
  }
  
  if( !dst ){ /* good source, couldn't open dst */
    fclose(src);
    trace(ERROR, default_trace, "fcopy: while opening dest: %s\n", strerror(errno));
    return "fcopy: error destination opening file";
  }

  while( (cur_read = fread(buf, 1, 50000, src)) > 0 ){
    total_read += cur_read;
    do{
      cur_wrote = fwrite(buf, 1, cur_read, dst);
      total_wrote += cur_wrote;
    }
    while( (total_wrote < total_read) && cur_wrote);
  }
  
  /* make sure they're equal size */
  if(total_read != total_wrote){
     fclose(src);
     fclose(dst);
     return "fcopy: Error copying files (size error)";
  }
  
  fclose(src);
  fclose(dst);

  return NULL;
}

/*
 * Remove all crash files for a given db in the irr_dir
 *  return:
 *    0 error
 *    1 ok
 */
int remove_db_crash_files(char * db_name, long serial){
  regex_t db_file, db_trans, db_jentry, db_pgp;
  struct dirent *dp;
  DIR *dirp;
  char buf[200];
  int ret = 1;
  
  /* sanity check...no seg faults */
  if(!db_name){
    trace(ERROR, default_trace, "remove_db_crash_files:Corrupt db, NULL name value\n");
    return 0;
  }
  
  /* print our pattern to buf, then compile it */
  if(serial == 0){
    snprintf(buf, 200, "%s\.[[:digit:]]+", db_name);
    regcomp(&db_file, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.trans\.[[:digit:]]+", db_name);
    regcomp(&db_trans, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.jentry\.[[:digit:]]+", db_name);
    regcomp(&db_jentry, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.pgp\.[[:digit:]]+", db_name);
    regcomp(&db_pgp, buf, REG_EXTENDED|REG_NOSUB);
  } else {
    snprintf(buf, 200, "%s\.%ld", db_name, serial);
    regcomp(&db_file, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.trans\.%ld", db_name, serial);
    regcomp(&db_trans, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.jentry\.%ld", db_name, serial);
    regcomp(&db_jentry, buf, REG_EXTENDED|REG_NOSUB);
    snprintf(buf, 200, "%s\.pgp\.%ld", db_name, serial);
    regcomp(&db_pgp, buf, REG_EXTENDED|REG_NOSUB);
  }

  /* check all the files in the directory */
  if ((dirp = opendir (ci.db_dir)) != NULL) {

    while ((dp = readdir (dirp)) != NULL){
      if( !regexec(&db_file, dp->d_name, 0, 0, 0) ||
	  !regexec(&db_trans, dp->d_name, 0, 0, 0) ||
	  !regexec(&db_jentry, dp->d_name, 0, 0, 0) ||
	  !regexec(&db_pgp, dp->d_name, 0, 0, 0) ){
	char dir[MAXLINE];
	snprintf(dir, MAXLINE, "%s/%s", ci.db_dir, dp->d_name);
	trace(NORM, default_trace, "remove_crash_files: removing file %s\n", dir);
	remove(dir);
      }
    }
    closedir(dirp);
  }
  else {
    trace(ERROR, default_trace, "remove_db_crash_files: Could not open directory %s\n", ci.db_dir);
    ret = 0;
  }
  
  regfree(&db_file);
  regfree(&db_trans);
  regfree(&db_jentry);
  regfree(&db_pgp);
  
  return ret;
}

int remove_db_pgp_copies(char * db_name){
  regex_t pgp_skr, pgp_pkr, pgp_rand;
  struct dirent *dp;
  DIR *dirp;
  char buf[200];
  int ret = 1;

  /* sanity check...no seg faults */
  if(!db_name){
    trace(ERROR, default_trace, "remove_db_pgp_copies: Corrupt db, NULL name value\n");
    return 0;
  }
  
  regcomp(&pgp_pkr,  "pubring\.pkr\.[[:digit:]]+",  REG_EXTENDED);
  regcomp(&pgp_skr,  "secring\.pkr\.[[:digit:]]+",  REG_EXTENDED);
  regcomp(&pgp_rand, "randseed\.bin\.[[:digit:]]+", REG_EXTENDED);
  
  snprintf(buf, 200, "%s/%s", ci.pgp_dir, db_name);

  if ((dirp = opendir (buf)) != NULL) {
    while ((dp = readdir (dirp)) != NULL){
      if(!regexec(&pgp_pkr, dp->d_name, 0, 0, 0) ||
	 !regexec(&pgp_skr, dp->d_name, 0, 0, 0) ||
	 !regexec(&pgp_rand, dp->d_name, 0, 0, 0) ){
	char dir[MAXLINE];
	snprintf(dir, MAXLINE, "%s/%s", buf, dp->d_name);
	trace(NORM, default_trace, "remove_db_pgp_copies: removing %s\n", dir);
	remove(dir);
      }
    }
    closedir(dirp);
  }
  else {
    trace(ERROR, default_trace, "remove_db_pgp_copies: Could not open directory %s\n", ci.db_dir);
    ret = 0;
  }
  
  regfree(&pgp_pkr);
  regfree(&pgp_skr);
  regfree(&pgp_rand);

  return ret;
}

/*
 * Removes irr_submit files older than IRR_FILE_LIFETIME
 * Removes all mirror_submit files.
 */
void remove_update_files(){
  regex_t irr_submit_xxx, mtr_xxx;
  struct dirent *dp;
  DIR *dirp;
  char buf[MAXLINE];
  struct timeval now;
  
  regcomp(&irr_submit_xxx, "irr_submit\.[[:digit:]]+", REG_EXTENDED);
  regcomp(&mtr_xxx, "mtr\.[[:alnum:]]+", REG_EXTENDED);

  if ((dirp = opendir (ci.db_dir)) != NULL) {
    while ((dp = readdir (dirp)) != NULL){
      if(!regexec(&irr_submit_xxx, dp->d_name, 0, 0, 0) ){
	struct stat stats;
	snprintf(buf, MAXLINE, "%s/%s", ci.db_dir, dp->d_name);
	if( lstat(buf, &stats) == 0 ){
	  gettimeofday(&now, NULL);
	  if( (now.tv_sec - stats.st_mtime) > IRR_FILE_LIFETIME ){
	    trace(NORM, default_trace, "remove_update_files: removed %s\n", buf);
	    remove(buf);
	  }
	}
      }
      if(!regexec(&mtr_xxx, dp->d_name, 0, 0, 0) ){
	snprintf(buf, MAXLINE, "%s/%s", ci.db_dir, dp->d_name);
	trace(NORM, default_trace, "remove_update_files: removed %s\n", buf);
	remove(buf);
      }
    }
    closedir(dirp);
  }
  else 
    trace(ERROR, default_trace, "remove_update_files: Could not open directory %s\n", ci.db_dir);
  
  regfree(&irr_submit_xxx);
  regfree(&mtr_xxx);
  
  return;
}


/*
 * Open the journal file for a given db
 * return the file pointer, may be NULL
 */
FILE * open_db_journal(char * db_name, char * mode){
  char buf[1024];
  snprintf(buf, 1024, "%s/%s.RPS.JOURNAL", ci.db_dir, db_name);
   
  return (fopen(buf, mode));
}

/*
 * open the <db>.trans.<serial> file for a given db and serial
 * return the file pointer, which may be NULL
 */
FILE * open_db_trans_xxx(char * db_name, char * mode, long serial){
  char buf[1024];
  snprintf(buf, 1024, "%s/%s.trans.%ld", ci.db_dir, db_name, serial);
   
  return (fopen(buf, mode));
}

/*
 *  open the <db>.<serial> for the given db and serial.
 * return the file pointer, which may be NULL
 */
FILE * open_db_xxx(char * db_name, char * mode, long serial){
  char buf[1024];
  snprintf(buf, 1024, "%s/%s.%ld", ci.db_dir, db_name, serial);
 
  return (fopen(buf, mode));
}

/* open the <db>.<jentry>.<serial> for the given DB and serial
 * return may be NULL 
 */
FILE * open_db_jentry_xxx(char * db_name, char * mode, long serial){
  char buf[1024];
  snprintf(buf, 1024, "%s/%s.jentry.%ld", ci.db_dir, db_name, serial);
 
  return (fopen(buf, mode));
}

/*
 * open the <db>.<pgp>.<serial> file 
 */
FILE * open_db_pgp_xxx(char *db_name, char * mode, long serial){
  char buf[1024];
  snprintf(buf, 1024, "%s/%s.pgp.%ld", ci.db_dir, db_name, serial);

  return (fopen(buf, mode));
}

/*
 * Given an open file and a file name, open the filename
 * and append it to the already open file
 */
char *
append_file_to_file(FILE * dest, char * fname, char * start_str){
  FILE * fp = fopen(fname, "r");
  char buf[MAXLINE];
  int start_off = 0;
  
  if(!fp){
    trace(ERROR, default_trace, "append_file_to_file: Error opening temp user file %s: %s\n", fname, strerror(errno));
    return "append_file_to_file: Error opening temp user file";
  }
  if(start_str){
    sprintf(buf, "%s", start_str); 
    start_off = strlen(buf);
  }

  while(fgets(buf+start_off, MAXLINE, fp))
    fputs(buf, dest);
  fclose(fp);

  return NULL;
}

/*
 * Dump somthing of the form:
 *  repository-signature: <db>
 *  signature:
 *  + ------PGP Begin ----
 *  + AAAH9028ASnbAaNSDH..
 *
 * to file, removing the RPSL line continuation characters
 */
int dump_pgp_block(FILE * src, char * dest_file){
  FILE * fptr = NULL;
  regmatch_t args[5];
  char buf[MAXLINE];
  regex_t line, delimit_line;
  int delimit = 0;

  if( dest_file == NULL ){
    trace(ERROR, default_trace, "dump_pgp_block: Name of destination file was NULL\n");
    return 0;
  }

  if( (fptr = fopen(dest_file, "w")) == NULL ){
    trace(ERROR, default_trace, "dump_pgp_block: Error opening destination file %s: %s\n", dest_file, strerror(errno));
    return 0;
  }
  
  regcomp(&line, "^[+ \t][ \t]*(.*)[ \t]*[\n]?$", REG_EXTENDED);  /* eat ONE line continuation char, and ALL whitespace */
  regcomp(&delimit_line, "^[ \t]+------", REG_EXTENDED); /* start and end dump */

  while(fgets(buf, MAXLINE, src) && delimit < 2){
    if( !regexec(&rep_sig, buf, 5, args, 0) ||
	!regexec(&signature, buf, 5, args, 0) )
      continue;
    if( !regexec(&blank_line, buf, 5, args, 0))
      break;
    if( !regexec(&delimit_line, buf, 5, args, 0))
      delimit++;
    if( !regexec(&line, buf, 5, args, 0) ){
      buf[args[1].rm_eo] = '\0';
      fputs((buf+args[1].rm_so), fptr);
    }
  }

  fclose(fptr);

  regfree(&line);
  regfree(&delimit_line);

  return 1;
}

/*
 * Query IRRd for the db's key-cert, dump the certif portion to file
 * return the name of that file
 */
char *
dump_keycert_from_db_to_file(char * db_name, char * cert, char * irrd_host, int irrd_port){
  char irrd_out_name[256] = "mtr.XXXXXX";
  int conn_open, sockfd;
  FILE * result;
  char * tmp_file;

  if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
			       "key-cert", db_name, cert, MAX_IRRD_OBJ_SIZE, irrd_host, irrd_port)) != NULL){
    tmp_file = tempnam(ci.db_dir, "mtr.");
    rewind(result);
    dump_pgp_block(result, tmp_file);
    fclose(result);
    remove(irrd_out_name);
    return tmp_file;
  }

  return NULL;
}
