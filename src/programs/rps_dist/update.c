#include <stdio.h>
#include "rpsdist.h"
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <irrd_ops.h>
#include <errno.h>
#include <arpa/inet.h>  /* inet_ntoa */

extern trace_t *default_trace;
extern char tmpfntmpl[];
FILE * dfile;
extern FILE * ofile;

/*
 * Build a file that contains
 *   serial
 *   PGP name
 *   journal size
 *   serial
 * Used whenever we start recovering from crashes
 */
char *  build_db_trans_xxx_file(rps_database_t * db, char * pgp_name){
  FILE * fptr_trans;
  char * ret_code;

  if( (fptr_trans = open_db_trans_xxx(db->name, "w+", db->current_serial+1)) ){
    long old_position = ftell(db->journal);
    if( strcmp(pgp_name, "NULL") ){
      FILE * fin = open_db_pgp_xxx(db->name, "r", db->current_serial+1);
      if( !fin ){
	trace(ERROR, default_trace, "build_db_trans_xxx_file: Error opening pgp file: %s\n", strerror(errno));
	return "build_db_trans_xxx_file: Error opening pgp file";
      }
      if( (ret_code = copy_pgp_rings(db)) == NULL ){
	char pgpdir[MAXLINE];
	snprintf(pgpdir, MAXLINE, "%s/%s/", ci.pgp_dir, db->name);
	ret_code = rpsdist_update_pgp_ring(default_trace, fin, pgpdir);
      }
      fclose(fin);
      if(ret_code){
	fclose(fptr_trans);
	return ret_code;
      }
    }
    fseek(db->journal, 0, SEEK_END);
    fprintf(fptr_trans, "%ld\n%s\n%ld\n%ld\n", db->current_serial+1, pgp_name,
	    ftell(db->journal), db->current_serial+1);
    db->jsize = ftell(db->journal);
    fclose(fptr_trans);
    fseek(db->journal, old_position, SEEK_SET);
  }
  else
    return "build_db_trans_xxx_file: Error creating file";

  return NULL;
}

/*
 * Before we operate on our PGP rings, make backup copies
 */
char * copy_pgp_rings(rps_database_t * db){
  char src[MAXLINE], dst[MAXLINE];
  char * ret_code;

  snprintf(src, MAXLINE, "%s/%s/pubring.pkr", ci.pgp_dir, db->name);
  snprintf(dst, MAXLINE, "%s/%s/pubring.pkr.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  if( (ret_code = fcopy(src, dst)) != NULL )
    return ret_code;

  snprintf(src, MAXLINE, "%s/%s/secring.pkr", ci.pgp_dir, db->name);
  snprintf(dst, MAXLINE, "%s/%s/secring.pkr.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  if( (ret_code = fcopy(src, dst)) != NULL )
    return ret_code;

  snprintf(src, MAXLINE, "%s/%s/randseed.bin", ci.pgp_dir, db->name);
  snprintf(dst, MAXLINE, "%s/%s/randseed.bin.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  if( (ret_code = fcopy(src, dst)) != NULL )
    return ret_code;
  
  return NULL;
}

/*
 * we screwed up our original, luckily we made copies
 */
void restore_pgp_rings(rps_database_t * db){
  char src[MAXLINE], dst[MAXLINE];
  
  snprintf(dst, MAXLINE, "%s/%s/pubring.pkr", ci.pgp_dir, db->name);
  snprintf(src, MAXLINE, "%s/%s/pubring.pkr.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  rename(src, dst);

  snprintf(dst, MAXLINE, "%s/%s/secring.pkr", ci.pgp_dir, db->name);
  snprintf(src, MAXLINE, "%s/%s/secring.pkr.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  rename(src, dst);

  snprintf(src, MAXLINE, "%s/%s/randseed.bin", ci.pgp_dir, db->name);
  snprintf(dst, MAXLINE, "%s/%s/randseed.bin.%ld", ci.pgp_dir, db->name, db->current_serial+1);
  rename(src, dst);
}
/*
 * Given a DB, send the <db>.<serial> to IRRd
 */
char * send_db_xxx_to_irrd(rps_database_t * db){
  FILE * fptr_db;
  char * ret_code;

  if( (fptr_db = open_db_xxx(db->name, "r", db->current_serial+1)) ){  
    trace(NORM, default_trace, "send_db_xxx_to_irrd: about to send\n");
    ret_code = send_rpsdist_trans(default_trace, fptr_db, ci.irrd_host, ci.irrd_port);
    trace(NORM, default_trace, "send_db_xxx_to_irrd: sent\n");
  } else{
    trace(ERROR, default_trace, "send_db_xxx_to_irrd: could not open db_xxx file: %s\n", strerror(errno));
    return "Could not open db_xxx file to send to irrd";
  }

  fclose(fptr_db);
  return ret_code;
}

/*
 *  Given a file, send it to everyone we know
 */
char * forward_file_to_peers(char * flnm, struct select_master_t * sm, unsigned long not_host){
  rps_connection_t * tmp;
  int file_size = 0;
  struct sockaddr_in addr;

  pthread_mutex_lock(&sm->mutex_lock);

  tmp = sm->active_links;

  for(tmp = sm->active_links; tmp; tmp = tmp->next){
    if( tmp->host == not_host )    /* don't send to the host that sent to us */
      continue;
    addr.sin_addr.s_addr = tmp->host;
    printf("Sending to peer: %s\n", inet_ntoa(addr.sin_addr));
    pthread_mutex_lock(&tmp->write);
    /* write the transaction-begin: ... */
    file_size = send_trans_begin(tmp, flnm);

    /* write the contents */
    filen_to_sock(tmp, flnm, file_size);
    pthread_mutex_unlock(&tmp->write);
  }

  pthread_mutex_unlock(&sm->mutex_lock);

  return NULL;
}
   
/*
 * Send a "transaction begin" object to the repository irr.  flnm
 * is the name of the file we'll be sending (we send it's size).
 */
int send_trans_begin(rps_connection_t * irr, char * flnm){
  struct stat fsize;
  char send_buf[MAXLINE*2];
  int sent;

  irr->time_last_sent = time(NULL);

  if( stat(flnm, &fsize) < 0 ){
    trace(ERROR, default_trace, "send_trans_begin: stat error: %s\n", strerror(errno));
    return 0;
  }
  
  snprintf(send_buf, MAXLINE*2, "transaction-begin: %ld\ntransfer-method: %s\n\n",
	   fsize.st_size, "plain");
  
  if( (sent = safe_write(irr->sockfd, send_buf, strlen(send_buf))) != strlen(send_buf) ){
    trace(ERROR, default_trace, "send_trans_begin: error sending\n");
    return 0;
  }

  return fsize.st_size;;
}

int write_trans_begin(FILE * fp, char * flnm){
   struct stat fsize;

   if( stat(flnm, &fsize) < 0 ){
    trace(ERROR, default_trace, "send_trans_begin: stat error: %s\n", strerror(errno));
    return 0;
   }
    
   fprintf(fp, "transaction-begin: %ld\ntransfer-method: %s\n\n",
	   fsize.st_size, "plain");
   
   return 1;
}   
   
  
