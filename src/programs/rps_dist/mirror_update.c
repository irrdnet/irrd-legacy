#include "rpsdist.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pgp.h>
#include <unistd.h>
#include <irrd_ops.h>
#include <arpa/inet.h>
#include <sys/time.h>

extern trace_t * default_trace;
extern struct select_master_t mirror_floods;
extern pthread_mutex_t poll_mutex;
extern pthread_cond_t need_poll;
extern pthread_mutex_t db_list_mutex;

/*
 * determine if this is a mirror transaction, if so, process is completely
 */
int handle_mirror_flood(mirror_transaction * mtrans){
  FILE * fp;
  int ret = 0;

  printf("Transaction in file: %s\n", mtrans->trans_begin.pln_fname);

  /* unzip if need be */
  if(mtrans->trans_begin.encoding == GZIP){
    char zip_name[strlen(mtrans->trans_begin.pln_fname) + 3];
    sprintf(zip_name, "%s.gz", mtrans->trans_begin.pln_fname);
    unzip_file(zip_name);
  }
  
  /* open the socket dumped file, plaintext */
  if( (fp = fopen(mtrans->trans_begin.pln_fname, "r")) == NULL){
    trace(ERROR, default_trace, "handle_mirror_flood: Error opening file %s\n", mtrans->trans_begin.pln_fname);
    return 0;
  }
 
  /* be sure to cleanup memory if needed */
  if( parse_trans_label(fp, mtrans)){
    rps_database_t * db = find_database(mtrans->trans_label.label);

    /* once we get the label, see if we've done it or are holding it */
    if(db){
      pthread_mutex_lock(&db->lock);

      if( (mtrans->trans_label.serial <= db->current_serial) ||
	  currently_holding(db, mtrans->trans_label.serial) ){
	trace(NORM, default_trace, "handle_mirror_flood: Dropping transaction we've "
	      "done or are holding (%s:%ld)\n", mtrans->trans_label.label, mtrans->trans_label.serial);
	pthread_mutex_unlock(&db->lock);
	return 0;
      }
       
      pthread_mutex_unlock(&db->lock);
    }
    
    if(parse_objects(fp, mtrans)      &&
       parse_timestamp(fp, mtrans)    &&
       parse_user_sigs(fp)            &&
       parse_auth_dep(fp, mtrans)     &&
       parse_rep_sigs(fp, mtrans) ){
      
      if( from_trusted_repository(mtrans) )
	ret = process_mirror_transaction(mtrans);
      
      if(ret == 0){
      /*just send on it's merry way unmolested*/
	trace(NORM, default_trace, "handle_mirror_flood: Transaction received from %s, but failed trust\n", 
	      mtrans->trans_label.label);
	forward_file_to_peers(mtrans->trans_begin.pln_fname, &mirror_floods, mtrans->from_host);
      }
    }
  }

  fclose(fp);

  //remove(mtrans->trans_begin.pln_fname);
  if( ret != -1 )
    free_mirror_trans(mtrans, 0);

  return ret;
}

/* 
 * we've parsed every relevant header, and checked the signatures, we trust this
 * transaction, now process it
 */
int process_mirror_transaction(mirror_transaction * mtrans){
  /* if here, everything was formed properly, make our crash files */
  char jentry_nm[MAXLINE], pgp_nm[MAXLINE];
  rps_database_t * db = find_database(mtrans->trans_label.label);
  char * ret_code;
   
  /* sanity check */
  if(!db){  
    trace(ERROR, default_trace, "handle_mirror_flood: db %s not found (this shouldn't happen)\n", mtrans->trans_label.label);
    return 0;
  }
  
  /* lock */
  pthread_mutex_lock(&db->lock);
  
  db->time_last_recv = time(NULL);
    
  /* rename the jentry file, it had a temp name */
  snprintf(jentry_nm, MAXLINE, "%s/%s.jentry.%ld", ci.db_dir, db->name, mtrans->trans_label.serial);
  rename(mtrans->trans_begin.pln_fname, jentry_nm);
  
  /* rename the <db>.pgp.<cs+1> file, already created in parse_objects with a temp name */
  snprintf(pgp_nm, MAXLINE, "%s/%s.pgp.%ld", ci.db_dir, db->name, mtrans->trans_label.serial);
  if(mtrans->db_pgp_xxx_name)
    rename(mtrans->db_pgp_xxx_name, pgp_nm);
  
  /* check that this is the next serial */
  if( db->current_serial+1 == mtrans->trans_label.serial){
    /* build the db_trans_xxx_file */
    if( (ret_code = build_db_trans_xxx_file(db, mtrans->has_pgp_objs? pgp_nm : "NULL")) != NULL)
      return 0;
    
    /* sign the jentry file */
    if( (ret_code = rpsdist_auth(db, jentry_nm)) == NULL)
      if( (ret_code = sign_jentry(db, jentry_nm)) != NULL )
	return 0;
    
    /* append jentry to journal */
    if( (ret_code = append_jentry_to_db_journal(db, jentry_nm)) != NULL)
      return 0;
    
    /* send <db>.<cs+1> to irrd */
    if( (ret_code = send_db_xxx_to_irrd(db)) == NULL ) { /* success */
      journal_write_end_serial(db, db->current_serial + 1);
      db->current_serial++;
    } 
    else                                                /* failure */
      trans_rollback(db);
    
    /* send to peers */
    forward_file_to_peers(jentry_nm, &mirror_floods, mtrans->from_host);
    
    /* remove crash files, pgp files */
    remove_db_crash_files(db->name, mtrans->trans_label.serial);
    remove_db_pgp_copies(db->name);
    
    /* unlock */
    pthread_mutex_unlock(&db->lock);
    
    process_held_transactions(db);
    
    return 1;
  }
  else if( mtrans->trans_label.serial > db->current_serial+1 ){
    /* out of order object */
    int ret = 0;
    if( hold_transaction(db, mtrans) ){       /* held ok, don't free the mem or del files */
      ret = -1;
      trace(NORM, default_trace, "parse_mirror_transaction: transaction held: %s: %ld\n", 
	    mtrans->trans_label.label, mtrans->trans_label.serial);
    }
    else{                                    /* hold cache full, dump it all */
      char db_xxx_file[MAXLINE];
      ret = 1;
      snprintf(db_xxx_file, MAXLINE, "%s/%s.%ld", ci.db_dir, mtrans->trans_label.label, mtrans->trans_label.serial);
      remove(db_xxx_file);                   /* don't call remove_db_crash_files because it will remove our held ones too! */
    }
    pthread_mutex_unlock(&db->lock);
    return ret;
  }
  
  else /* a looping object, goodbye */
    return 0;
}

/*
 *  free our alloced memory
 */
void free_mirror_trans(mirror_transaction * mtrans, int free_this) {
  if(mtrans->trans_begin.pln_fname)
    free(mtrans->trans_begin.pln_fname);
  if(mtrans->trans_label.label)
    free(mtrans->trans_label.label);
  free_auth_dep_list(mtrans->auth_dep.next);
  free_rep_sig_list(mtrans->rep_sig.next);
  if(free_this)
    free(mtrans);
  return;
}  

void free_auth_dep_list(auth_dep_h * adp){
  auth_dep_h * tmp = adp;
  while(adp){
    if(adp->name)
      free(adp->name);
    tmp = adp;
    adp = adp->next;
    free(tmp);
  }
}

void free_rep_sig_list(rep_sig_h * rsp){
  rep_sig_h * tmp = rsp;
  while(rsp){
    if(rsp->repository)
      free(rsp->repository);
    tmp = rsp;
    rsp = rsp->next;
    free(rsp);
  }
}

/*
 * A transaction has arrived to us out of order.
 * Add it to our list of held transactions.
 */
int hold_transaction(rps_database_t * db, mirror_transaction * mtrans) {
  mirror_transaction * tmp = db->held_transactions;
  mirror_transaction * before_tmp = NULL;
  int index = 0;

  while(tmp && (mtrans->trans_label.serial > tmp->trans_label.serial) ){
    before_tmp = tmp;
    tmp = tmp->next;
    index++;
    
    if(index >= MAX_HELD_TRANS){
      trace(INFO, default_trace, "hold_transaction: transaction queue full for db %s\n", db->name);
      pthread_mutex_lock(&poll_mutex);
      pthread_cond_signal(&need_poll);
      pthread_mutex_unlock(&poll_mutex);
      pthread_mutex_unlock(&db->lock);
      return 0;
    }
  }

  if(db->held_transactions == NULL)             /* start new list */
    db->held_transactions = mtdupe(mtrans);
  else{                                         /* insert into list */
    if( !before_tmp ){
      tmp = db->held_transactions;
      db->held_transactions = mtdupe(mtrans);
      db->held_transactions->next = tmp;
    }
    else{
      tmp = before_tmp->next;
      before_tmp->next = mtdupe(mtrans);
      before_tmp->next->next = tmp;
    }
  }  
  pthread_mutex_lock(&poll_mutex);             /* this should not have to wait long */
  pthread_cond_signal(&need_poll);
  pthread_mutex_unlock(&poll_mutex);

  return 1;
} 

/* 
 * transactions that occur completely properly will never call this, they
 * are allocated in the callback thread.  This is used to copy transactions
 * to be inserted into the holding queue
 */
mirror_transaction * mtdupe(mirror_transaction * mtrans){
  mirror_transaction * tmp = (mirror_transaction *) malloc (sizeof(mirror_transaction));

  if( !tmp ){
    trace(ERROR, default_trace, "mtdupe: malloc error\n");
    return NULL;
  }

  memcpy(tmp, mtrans, sizeof(mirror_transaction) );
  return tmp;
}

/*
 * We call this after every update to check that we don't have some trans
 * to process.
 * this call is inherently recursive (by calling process_mirror_transaction)
 */
void process_held_transactions(rps_database_t * db){
  mirror_transaction * tmp; 

  pthread_mutex_lock(&db->lock);
  
  tmp = db->held_transactions;

  if(tmp){
    if(tmp->trans_label.serial == db->current_serial+1){
      /* this list is ordered by sequence number */
      db->held_transactions = tmp->next;
      pthread_mutex_unlock(&db->lock);
      process_mirror_transaction(tmp);
      free_mirror_trans(tmp, FREE_MTRANS_NODE);
    }
  }
  
  pthread_mutex_unlock(&db->lock);

  return;
}
  
/*
 * Strip transaction begin
 *  if irr->tmp has a "transaction-begin: <db> on it, process it
 *  Return 
 *    1 was a trans begin, and was handled
 *    0 wasn't a trans begin
 */
int strip_trans_begin(rps_connection_t * irr, mirror_transaction * mtrans){
  trans_begin_h * hdr = &mtrans->trans_begin;
  regmatch_t args[5];
  int ret = 0, good_var = 0;
  
  do{
     if( !regexec(&trans_begin, irr->tmp,  5, args, 0) ){
       char size[(args[1].rm_eo - args[1].rm_so) + 1];
       memcpy(size, irr->tmp + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
       size[args[1].rm_eo - args[1].rm_so] = '\0';
       hdr->length = atol(size);
       good_var++;
     }
     else if( !regexec(&trans_method, irr->tmp, 5, args, 0) ){
       char method[(args[1].rm_eo - args[1].rm_so) + 1];
       memcpy(method, irr->tmp + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
       method[args[1].rm_eo - args[1].rm_so] = '\0';
       if(!strcasecmp(method, "gzip"))
	 hdr->encoding = GZIP;
       else
	 hdr->encoding = PLAIN;
       good_var++;
     }
     else if( !regexec(&blank_line, irr->tmp, 0, 0, 0) ){
       ret = 1;
       break;
     }
     else{
       ret = 0;
       break;
     }
  } while ( read_buffered_line(default_trace, irr) > 0 );
    
  if( good_var < 1 ) /* nothing...oops */
    return 0;

  if( good_var == 2 )  /* if two, we're good */
    return 1;

  if( (good_var == 1) && hdr->length == 0 )/* if one, check to see it was the size */
    return 0;
      
  return ret;
}

/*
 * Fill in the trans_label_h struct inside the given mtrans
 */
int parse_trans_label(FILE * fp, mirror_transaction * mtrans){
  regmatch_t args[5];
  char curline[MAXLINE];
  int ret = 1, good_var = 0;

  while( fgets(curline, MAXLINE, fp) ){
    if( !regexec(&trans_label, curline, 5, args, 0) ){
      char db_name[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(db_name, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      db_name[args[1].rm_eo - args[1].rm_so] = '\0';
      if( mtrans->trans_label.label ){
	trace(ERROR, default_trace, "parse_trans_label: Label has more than one db_name!\n");
	return 0;
      }
      mtrans->trans_label.label = strdup(db_name);
      good_var++;
    }
    else if( !regexec(&sequence, curline, 5, args, 0) ){
      char seqno[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(seqno, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      seqno[args[1].rm_eo - args[1].rm_so] = '\0';
      if( mtrans->trans_label.serial ){
	trace(ERROR, default_trace, "parse_trans_label: Label has more than one sequence!\n");
	return 0;
      }
      mtrans->trans_label.serial = atol(seqno);
      good_var++;
    }
    else if( !regexec(&timestamp, curline, 5, args, 0) ){
      char ts[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(ts, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      ts[args[1].rm_eo - args[1].rm_so] = '\0';
      if( mtrans->trans_label.timestamp ){
	trace(ERROR, default_trace, "parse_trans_label: Label has more than one timestamp!\n");
	return 0;
      }
      mtrans->trans_label.timestamp = tstol(ts);
      good_var++;
    }
    else if( !regexec(&integrity, curline, 5, args, 0) ){
      char integ[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(integ, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      integ[args[1].rm_eo - args[1].rm_so] = '\0';
      mtrans->trans_label.integrity = integrity_strtoi(integ);
      good_var++;
    }
    else if( !regexec(&blank_line, curline, 0, 0, 0) ){
      ret = 1;
      break;
    }
    else{
      trace(ERROR, default_trace, "parse_trans_label: No blank line before objects\n");
      ret = 0;
      break;
    }
  }

  if(good_var < 3) /* we need atleast three */
    ret = 0;
  
  if(good_var >= 4 && mtrans->trans_label.integrity == NONE) /* we had 4 things, one WASN'T integrity, error */
    ret = 0;

  if( ret )
    return check_trans_label(mtrans);
  
  return 0;
}

/*
 * check the db_name authority
 * XXX - Later check that we haven't processed this serial already
 * and that we don't need to hold it.  The file in mtrans->trans_begin.pln_f_nm
 * will be renamed to <db>.jentry.<cs+1>
 */
int check_trans_label(mirror_transaction * mtrans){
  rps_database_t * db = find_database(mtrans->trans_label.label);

  /* if we find it and we're not auth, ok */
  if( db )
    return !db->authoritative;
  else
    return 1;
    
  return 0;
}

/*
 * build list of auth dependency objects inside the given
 * mtrans 
 */
int parse_auth_dep(FILE * fp, mirror_transaction * mtrans){
  regmatch_t args[5];
  char curline[MAXLINE];
  int good_var = 0;
  long last_blankline = ftell(fp);
  auth_dep_h * auth_dep_p = &mtrans->auth_dep;

  while( fgets(curline, MAXLINE, fp) ){
    if( !regexec(&auth_dep, curline, 5, args, 0) ){
      char db_name[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(db_name, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      db_name[args[1].rm_eo - args[1].rm_so] = '\0';
      if( auth_dep_p->name ){
	trace(ERROR, default_trace, "parse_auth_dep: Dep object has more than one db_name\n");
	return 0;
      }
      auth_dep_p->name = strdup(db_name);
      good_var++;
    }
    else if( !regexec(&sequence, curline, 5, args, 0) ){
      char seqno[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(seqno, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      seqno[args[1].rm_eo - args[1].rm_so] = '\0';
      if( auth_dep_p->serial ){
	trace(ERROR, default_trace, "parse_auth_dep: Label has more than one sequence!\n");
	return 0;
      }
      auth_dep_p->serial = atol(seqno);
      good_var++;
    }
    else if( !regexec(&timestamp, curline, 5, args, 0) || !regexec(&signature, curline, 5, args, 0) ){
      char ts[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(ts, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      ts[args[1].rm_eo - args[1].rm_so] = '\0';
      if( auth_dep_p->timestamp ){
	trace(ERROR, default_trace, "parse_trans_label: Label has more than one timestamp!\n");
	return 0;
      }
      auth_dep_p->timestamp = tstol(ts);
      good_var++;
    }
    else if( !regexec(&blank_line, curline, 0, 0, 0) ){
      if( good_var != 3 ){
	trace(ERROR, default_trace, "parse_auth_dep: Incomplete dependency object found\n");
	return 0;
      }
      if( (auth_dep_p->next = (auth_dep_h *)malloc(sizeof(auth_dep_h))) == NULL){
	trace(ERROR, default_trace, "parse_auth_dep: Malloc error\n");
	return 0;
      }
      memset(&auth_dep_p->next, 0, sizeof(auth_dep_h));
      auth_dep_p = auth_dep_p->next;
      good_var = 0;
      last_blankline = ftell(fp);  /* next read will be one line after blank line */
      break;
    }
    else
      break;
  }

  fseek(fp, last_blankline, SEEK_SET);

  return 1;
}

/*
 *  The objects here will be the redistributed versions.  We need to go through and
 *  put in the operations before sending to IRRd
 */
int parse_objects_orig(FILE * fp, mirror_transaction * mtrans){
  char curline[MAXLINE];
  regmatch_t args[5];
  long last_blankline = 0, last_obj = 0, old_pos = ftell(fp);
  FILE * db_xxx_file = NULL;
  char add_str[] = "\nADD\n\n";
  char del_str[] = "\nDEL\n\n";
  char db_xxx_name[1024];
  
  snprintf (db_xxx_name, 1024, "%s/%s.%ld", ci.db_dir, mtrans->trans_label.label, mtrans->trans_label.serial);
  if( (db_xxx_file = fopen(db_xxx_name, "w")) == NULL ){
    trace(ERROR, default_trace, "parse_objects: error dumping to file %s: %s\n", db_xxx_name, strerror(errno));
    return 0;
  }
  
  /* first line is the first object */
  fprintf(db_xxx_file, "!us%s\n", mtrans->trans_label.label);
  last_blankline = ftell(db_xxx_file);
  fputs(add_str, db_xxx_file);

  while( fgets(curline, MAXLINE - 1, fp) ){
    /* normal objects (all objects) */
    if( !regexec(&blank_line, curline, 0, 0, 0) ){
      last_blankline = ftell(db_xxx_file);
      old_pos = ftell(fp);
      fputs(add_str, db_xxx_file);
    }
    else if( !strncmp(curline, "delete:", 7) ){
      long tmpPos = ftell(db_xxx_file);
      fseek(db_xxx_file, last_blankline, SEEK_SET);
      fputs(del_str, db_xxx_file);
      fseek(db_xxx_file, tmpPos, SEEK_SET);
    }
    else if( !regexec(&signature, curline, 0, 0, 0) )
      break;
    else {
      fputs(curline, db_xxx_file);
      last_obj = ftell(db_xxx_file);        
    }
    
    /* key certs */
    if( !regexec(&key_cert, curline, 5, args, 0) ){
      char hexId[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(hexId, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      hexId[args[1].rm_eo - args[1].rm_so] = '\0';
      if( process_keycert(fp, hexId, mtrans) )
	mtrans->has_pgp_objs++;
    }
    
  }

  fseek(db_xxx_file, last_obj, SEEK_SET);
  fprintf(db_xxx_file, "\n!ue\n!q\n");
  fclose(db_xxx_file);

  fseek(fp, old_pos, SEEK_SET);

  return 1;
}

int parse_objects (FILE *fp, mirror_transaction *mtrans) {
  char curline [MAXLINE+1];
  regmatch_t args[5];
  long add_pos, last_blankline = ftell(fp);
  int in_obj = 0;
  FILE *db_xxx_file = NULL;
  char add_str[] = "ADD\n\n";
  char del_str[] = "DEL\n\n";
  char db_xxx_name[1024];
  
  snprintf (db_xxx_name, 1024, "%s/%s.%ld", ci.db_dir, mtrans->trans_label.label, 
	    mtrans->trans_label.serial);
  if ((db_xxx_file = fopen(db_xxx_name, "w")) == NULL ) {
    trace(ERROR, default_trace, "parse_objects: error dumping to file %s: %s\n", 
	  db_xxx_name, strerror(errno));
    return 0;
  }
  
  fprintf (db_xxx_file, "!us%s\n", mtrans->trans_label.label);

  while (fgets (curline, MAXLINE, fp)) {
    /* Are we in an object? */
    if (regexec (&blank_line, curline, 0, 0, 0) == 0) {
      last_blankline = ftell (fp);
      in_obj  = 0;
      continue;
    }
    else if (!in_obj) {
      /* exit on the 'signature:' meta-object */
      if (regexec (&signature, curline, 0, 0, 0) == 0)
	break;

      /* blank line seperator between objects */
      fputs ("\n", db_xxx_file);
      in_obj  = 1;
      add_pos = ftell (fp);
      /* assume it's an ADD op */
      fputs (add_str, db_xxx_file);
    }

    /* if it's a DEL op, then go back and re-set the op label */
    if (!strncasecmp (curline, "delete:", 7)) {
      fseek (db_xxx_file, add_pos, SEEK_SET);
      fputs (del_str, db_xxx_file);
      fseek (db_xxx_file, 0L, SEEK_END);
    }
    /* write the line verbatim */
    else
      fputs (curline, db_xxx_file);
    
    /* key certs, build the pgp file to update the local rings
     * (necessary for *remote* rpsdist's) */
    if (!regexec (&key_cert, curline, 5, args, 0)) {
      char hexId[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy (hexId, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      hexId[args[1].rm_eo - args[1].rm_so] = '\0';
      if (process_keycert (fp, hexId, mtrans))
	mtrans->has_pgp_objs++;
    }
  }

  /* end the trans with a !ue and reset the input file */
  fprintf (db_xxx_file, "\n!ue\n!q\n");
  fclose (db_xxx_file);
  fseek (fp, last_blankline, SEEK_SET);

  return 1;
}

/* process_keycert 
 * 
 * We know the hexid, strip the certif into a file, create (or append) to
 * the db_pgp_xxx_file.  That's it.  During process_mirror_transaction()
 * we'll actually use the db_pgp_xxx_file to pass to the pgp_lib function
 * that actually operates on the keys.
 */
  
int process_keycert(FILE * fp, char * hexid, mirror_transaction * mtrans){
  char curline[MAXLINE];
  regmatch_t args[5];
  long certif_pos, old_pos = ftell(fp);
  int del_op = 0;
  FILE * db_pgp_xxx = NULL;
  char * temp_pgp_nm = tempnam(ci.db_dir, "mtr.");
  
  /* determine operation, this can save us creating a file. On the other hand
   * if this is a huge object, it may cost us.  Tough call...
   */
  while( fgets(curline, MAXLINE, fp) ){
    if( !strncasecmp(curline, "delete:", 7)){
      del_op = 1;
      break;
    }
    else if( !strncasecmp(curline, "certif:", 7) )
      certif_pos = ftell(fp);
    else if( !regexec(&blank_line, curline, 5, args, 0) )
      break;
  }

  /* see if a pgp_xxx file already exists, else create a name */
  if( mtrans->db_pgp_xxx_name == NULL )
    mtrans->db_pgp_xxx_name = tempnam(ci.db_dir, "mtr.");

  /* open the db_pgp_xxx file, which for now has a temporary name */
  if( (db_pgp_xxx = fopen(mtrans->db_pgp_xxx_name, "a+")) == NULL ){
    trace(ERROR, default_trace, "process_keycert: Error opening file %s\n", mtrans->db_pgp_xxx_name);
    return 0;
  }

  /* see if the keycert's a delete, it saves us some time */
  if( del_op )
    fprintf(db_pgp_xxx, "DEL\n%s\n\n", hexid);
  else{
    /* otherwise, we have to dump the certif portion to a file */
    fseek(fp, certif_pos, SEEK_SET);
    dump_pgp_block(fp, temp_pgp_nm);
    fprintf(db_pgp_xxx, "ADD\n%s\n\n", temp_pgp_nm);
  }
  
  fclose(db_pgp_xxx);
  fseek(fp, old_pos, SEEK_SET);
  return 1;
}
  
  
/*
 * read a single timestamp (and line after)
 */
int parse_timestamp(FILE * fp, mirror_transaction * mtrans){
  char curline[MAXLINE];
  regmatch_t args[5];
  long old_pos = ftell(fp);
  int good_var = 0;

  if( fgets(curline, MAXLINE, fp) ){
    if( !regexec(&timestamp, curline, 5, args, 0) ){
      char ts[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(ts, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      ts[args[1].rm_eo - args[1].rm_so] = '\0';
      mtrans->timestamp = tstol(ts);
      good_var++;
    }
    else{
      trace(INFO, default_trace, "parse_timestamp: No timestamp on user submission %ld\n", mtrans->trans_label.serial);
      fseek(fp, old_pos, SEEK_SET);
      return 1;
    }
  }
  else{
    trace(ERROR, default_trace, "parse_timestamp: error reading file\n");
    return 0;
  }
  if(good_var)
    fgets(curline, MAXLINE, fp);
  else
    fseek(fp, old_pos, SEEK_SET);

  return 1;
}

/*
 * Parse the user signatures
 */
int parse_user_sigs(FILE * fp){
  /* for now, just skip em */
  char curline[MAXLINE];
  regmatch_t args[5];
  long last_blankline = ftell(fp);

  while(fgets(curline, MAXLINE, fp) ){
    if( !regexec(&rep_sig, curline, 5, args, 0) ){
      fseek(fp, last_blankline, SEEK_SET);
      break;
    }
    else if( !regexec(&blank_line, curline, 5, args, 0) )
      last_blankline = ftell(fp);   
  }

  return 1;
}

/*
 * Just build a list of the repositories that have signed to search later,
 * and where in the file it begins and ends
 */
int parse_rep_sigs(FILE * fp, mirror_transaction * mtrans){
  char curline[MAXLINE];
  regmatch_t args[5];
  rep_sig_h * rep_sig_p = &mtrans->rep_sig;
  int good_var = 0;
  long last_pos = ftell(fp);

  while( fgets(curline, MAXLINE, fp) ){
    if( !regexec(&rep_sig, curline, 5, args, 0) ){
      char db_name[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(db_name, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      db_name[args[1].rm_eo - args[1].rm_so] = '\0';
      rep_sig_p->repository = strdup(db_name);
      good_var++;
    }
    else if( !regexec(&integrity, curline, 5, args, 0) ){
      char integ[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(integ, curline + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      integ[args[1].rm_eo - args[1].rm_so] = '\0';
      rep_sig_p->integrity = integrity_strtoi(integ);
      good_var++;
    }
    else if( !regexec(&signature, curline, 5, args, 0) ){
      rep_sig_p->fpos_start = last_pos;
      good_var++;
    }
    else if( !regexec(&blank_line, curline, 5, args, 0) ){
      rep_sig_p->fpos_end = ftell(fp);
      if( (good_var < 2) || (good_var >= 3 && rep_sig_p->integrity == NONE) ){
	trace(ERROR, default_trace, "parse_rep_sigs: Incomplete signature found\n");
	return 0;
      }
      if( (rep_sig_p->next = (rep_sig_h *)malloc(sizeof(rep_sig_h))) == NULL){
	trace(ERROR, default_trace, "parse_rep_sigs: malloc error\n");
	return 0;
      }
      memset(rep_sig_p->next, 0, sizeof(rep_sig_h));
      rep_sig_p = rep_sig_p->next;
      good_var = 0;
      break;
    }
    last_pos = ftell(fp);
  }

  if( (good_var < 2) || (good_var >= 3 && rep_sig_p->integrity == NONE) ){
    trace(ERROR, default_trace, "parse_rep_sigs: Incomplete signature found\n");
    return 0;
  }
  
  return 1;
}

/*
 * try to strip a heartbeat off the socket.
 *  if we get one process it
 */
int strip_heartbeat(rps_connection_t * irr){
  char * rep_name;
  long seqno, ts;
  regmatch_t args[5];
  int good_var = 0;
  
  do{
    if( !regexec(&heartbeat, irr->tmp, 5, args, 0) ){ 
      char tmp_nm[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_nm, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_nm[args[1].rm_eo - args[1].rm_so] = '\0';
      rep_name = strdup(tmp_nm);
      good_var++;
    }
    else if( !regexec(&sequence, irr->tmp, 5, args, 0) ){ 
      char tmp_no[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_no, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_no[args[1].rm_eo - args[1].rm_so] = '\0';
      seqno = atol(tmp_no);
    }
    else if( !regexec(&timestamp, irr->tmp, 5, args, 0) ){
      char tmp_no[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_no, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_no[args[1].rm_eo - args[1].rm_so] = '\0';
      ts = atol(tmp_no);
    }
    else
      break;
  } while( read_buffered_line(default_trace, irr) > 0);

  if( good_var == 3 ){
    /*XXX - process_heartbeat(rep_name, seqno, ts);*/
    return 1;
  }

  else
    return 0;
}

/*
 * utility functions, convert a string to it's
 * corresponding enum value
 */
int integrity_strtoi(char * integ){
  if( !strcasecmp(integ, "legacy"))
    return LEGACY;
  else if( !strcasecmp(integ, "no-auth"))
    return NO_AUTH;
  else if( !strcasecmp(integ, "auth-failed"))
    return AUTH_FAIL;
  else if( !strcasecmp(integ, "authorized"))
    return AUTH_PASS;
  else
    return NONE;
}

/*
 * try to strip a transaction request, if we can,
 * process it too.
 */
int strip_trans_request(rps_connection_t * irr){
  char * db_name = NULL;
  long first = 1, last = 0;
  regmatch_t args[5];
  int good_var = 0;

  do{
    if( !regexec(&trans_request, irr->tmp, 5, args, 0) ){ 
      char tmp_nm[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_nm, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_nm[args[1].rm_eo - args[1].rm_so] = '\0';
      db_name = strdup(tmp_nm);
      good_var++;
    }
    else if( !regexec(&seq_begin, irr->tmp, 5, args, 0) ){ 
      char tmp_no[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_no, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_no[args[1].rm_eo - args[1].rm_so] = '\0';
      first = atol(tmp_no);
    }
    else if( !regexec(&seq_end, irr->tmp, 5, args, 0) ){
      char tmp_no[(args[1].rm_eo - args[1].rm_so) + 1];
      memcpy(tmp_no, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
      tmp_no[args[1].rm_eo - args[1].rm_so] = '\0';
      last = atol(tmp_no);
    }
    else
      break;
  } while( read_buffered_line(default_trace, irr) > 0);

  if( db_name != NULL ){
    trace(NORM, default_trace, "Received a transaction request: %s: %ld - %ld\n", db_name, first, last);
    process_trans_request(irr, db_name, first, last);
    return 1;
  }
  
  return 0;
}

/*
 * We received a valid transaction request, now send a response.
 * Run through the journal, looking for: #serial: <start>
 * Send each "object(or group of objects)" until we get to last.
 */
void process_trans_request(rps_connection_t * irr, char * db_name, long first, long last){
  char curline[MAXLINE];
  regmatch_t args[5];
  rps_database_t * db = find_database(db_name);
  FILE * journal;
  int length;
  
  /* don't need to lock the db, the journal is read only up until the current_serial.
   * the process of responding is completely transparent (except we'll have a lock on the sending
   * socket).
   */
  if( (journal = open_db_journal(db_name, "r")) == NULL){
    trace(ERROR, default_trace, "process_trans_request: Error opening DB journal: %s\n", strerror(errno));
    return;
  }

  if( first < db->first_serial ){
    trace(ERROR, default_trace, "process_trans_request: Cannot satisfy request, serial too early\n");
    return;
  }

  if( (last == 0) || (last > db->current_serial) )
    last = db->current_serial;

  pthread_mutex_lock(&irr->write);

  while(fgets(curline, MAXLINE, journal)){
    if( !strncmp(curline, "#serial", 7) ){
      /* are we at the start? */
      if( first == atol(curline+7) ){
	/* if so, read the trans-begin: <length> */
	while(fgets(curline, MAXLINE, journal)){
	  if( !regexec(&trans_begin, curline, 5, args, 0) ){
	    length = atol(curline+args[1].rm_so);
	    safe_write(irr->sockfd, curline, strlen(curline));     /* trans-begin */
	    fgets(curline, MAXLINE, journal);
	    safe_write(irr->sockfd, curline, strlen(curline));     /* method */
	    fgets(curline, MAXLINE, journal);
	    safe_write(irr->sockfd, curline, strlen(curline));     /* newline */
	    _filen_to_sock(irr, journal, length);
	  }
	  if( !strncmp(curline, "#serial", 7) &&
	      (atol(curline+7) > last ) )
	    goto DONE;
	}
      }
    }
  }
 
 DONE:
  fclose(journal);
  /* transactions sent, send response */
  snprintf(curline, MAXLINE, "transaction-response: %s\nsequence-begin: %ld\nsequence-end: %ld\n\n",
	   db_name, first, last);
  safe_write(irr->sockfd, curline, strlen(curline));
  
  pthread_mutex_unlock(&irr->write);
}      
  
 
/*
 * Used by the polling thread, step through and see if each DB has any
 * held transactions, if so fire off a transaction request for the missing
 * object(s)
 */
int process_db_queues(void){
  rps_database_t *db;

  pthread_mutex_lock(&db_list_mutex); /* XXX really want to lock node by node, but that's 
					 hard to avoid race conditions */

  for(db = db_list; db; db = db->next){
    if( db->held_transactions ){
      int send_ok;
      rps_connection_t * irr = rps_select_find_fd_by_host(db->held_transactions->from_host, &mirror_floods);

      if(irr){
	/* send the request for serials one more than current serial and one less than the first held 
	 * to the repository that sent us the first held transaction (it's guaranteed to have the missing one)
	 */
	trace(NORM, default_trace, "process_db_queues: Sending transaction-request:%s:%ld - %ld\n", db->name, 
	      db->current_serial+1, db->held_transactions->trans_label.serial-1);
	send_ok = send_trans_request(irr, db->held_transactions->trans_label.label,
				     db->current_serial+1,
				     db->held_transactions->trans_label.serial-1);
	if( !send_ok )
	  trace(ERROR, default_trace, "process_db_queues: error sending transaction-request for %s\n", db->name);
      }
    }
  }
  
  pthread_mutex_unlock(&db_list_mutex);

  return 1;
}

int check_mirror_connections(){
  /* go through db_list, any DB's we should be mirroring but not connected to, open
   * connections to
   */
  rps_database_t *db;
  
  for(db = db_list; db; db = db->next){
    if( db->host && db->port ){
      if( !already_connected_to(db->host) ){
	struct in_addr addr;
	int newsock;
	addr.s_addr = db->host;
	newsock = open_connection(default_trace, inet_ntoa(addr), db->port);
	if(newsock < 0)
	  trace(NORM, default_trace, "check_mirror_connections: Error opening connection to %s(%d)\n", 
		inet_ntoa(addr), db->port);
	else{
	  rps_connection_t * peer;
	  /* this may be useless here an in init_utils...maybe it avoids spoofing? */
	  if( (peer = rps_select_add_peer(newsock, &mirror_floods)) != NULL){
	    peer->host = db->host;
	    peer->db_name = strdup(db->name);
	    synch_databases(peer, db);
	  }
	  trace(NORM, default_trace, "check_mirror_connections: Re-opened connection to: %s (%d)\n", 
		inet_ntoa(addr), db->port);
	}
      }
    }
  }

  return 1;
}
	

/*
 * When requesting transactions, use this to send the request 
 */
int send_trans_request(rps_connection_t * irr, char * db_name, long start_serial, long end_serial){
  
  char buf[MAXLINE];
  int sent;

  irr->time_last_sent = time(NULL);
  
  snprintf(buf, MAXLINE, "transaction-request: %s\nsequence-begin: %ld\nsequence-end: %ld\n\n",
	   db_name, start_serial, end_serial);

  if( (sent = send_mirror_buffer(irr, buf, strlen(buf))) != strlen(buf) ){
    trace(ERROR, default_trace, "send_trans_request: error sending\n");
    return 0;
  }
  
  return 1;
}
 
/*
 * Wrapper to assure proper communication with mirrors
 */
int send_mirror_buffer(rps_connection_t * irr, char * buf, int len){
  int sent = 0;

  irr->time_last_sent = time(NULL);

  pthread_mutex_lock(&irr->write);
  sent = safe_write(irr->sockfd, buf, len);
  pthread_mutex_unlock(&irr->write);
  
  return sent;
}

/*
 * Given an IP, find the DB that connects or allows connections from
 * the given IP.
 */
char *
find_host_db(unsigned long host){
  rps_database_t * db;
  /* XXX - proper locking please... */

  for(db = db_list; db; db = db->next){
    if( db->host == host )
      return db->name;    
  }
  
  return NULL;
}

int
already_connected_to(unsigned long host){
  rps_connection_t * irr;

  pthread_mutex_lock(&mirror_floods.mutex_lock);
  for( irr = mirror_floods.active_links; irr; irr = irr->next ){
    if( host == irr->host ){
      pthread_mutex_unlock(&mirror_floods.mutex_lock);
      return 1;
    }
  }
  pthread_mutex_unlock(&mirror_floods.mutex_lock);

  return 0;
}

/* 
 * Check that the first rep sig object is that of the
 * originating repository, there's two cases:
 *   1.  We know of the source, so we have it's repository object and keycert
 *       we can verify it as normal.
 *   2.  This is a new source, so we don't have it's repository object or keycert.
 */
int from_originating_repository(mirror_transaction * mtrans){
  rep_sig_h * rep_sig = &mtrans->rep_sig;
  rps_database_t * db = NULL;
  
  if(rep_sig){
    db = find_database(rep_sig->repository);
    if(db && check_rep_sig(rep_sig->repository, mtrans))   /* we know about the DB */
      return 1;
    if(!db){                                               /* it's a new DB */
      char reseed_addr[MAXLINE];
      char hexid[10];
      char * cert_file = get_new_source_keycert(rep_sig->repository, 
						mtrans->from_host, reseed_addr, hexid);
      
      if( cert_file ){
	/* make a tmp directory for the pgp rings */
	char tmp_pgp_dir[256];
	pgp_data_t pdat;
	
	umask (0022);
	strcpy (tmp_pgp_dir, "/var/tmp/pgp.XXXXXX");
	if (mkdtemp (tmp_pgp_dir) == NULL) {
	  trace(ERROR, default_trace,"Internal error.  Couldn't create temp directory for PGP\n");
	  return 0;
	}
	
	/* add the certificate to a temp ring */
	if (!pgp_add (default_trace, tmp_pgp_dir, cert_file, &pdat)) {
	  trace(ERROR, default_trace, "Couldn't add key-cert to ring.  Didn't get successful reply from PGP");
	  return 0;
	}
	
	/* check the sig on the transaction (finally) */
	if( _check_rep_sig(rep_sig->repository, tmp_pgp_dir, mtrans)){	  
	  /* It's good! send snapshot request */
	  add_new_db(rep_sig->repository, reseed_addr, hexid);
	  return 1;
	}
	remove(cert_file);
	free(cert_file);
      } 
      else{
	trace(ERROR, default_trace, "from_originating_repository: Could not locate the keycert for new DB %s\n",
	      rep_sig->repository);
	return 0;
      }
    }
  }
  else{ /* should never get here */
    trace(ERROR, default_trace, "from_originating_repository: No Repository signatures\n");
    return 0;
  }
  
  return 0;
}

/*
 * Given a mirror transaction, determine if a trusted repository has signed it.
 * And if so, if that signature is valid
 */
int from_trusted_repository(mirror_transaction * mtrans){
  /* step through the list of repository signatures, see if we know about it
   *  and we trust it
   */
  rep_sig_h * rep_sig = &mtrans->rep_sig;
  rps_database_t * db = NULL;

  while(rep_sig){
    db = find_database(rep_sig->repository);
    if( db && db->trusted && check_rep_sig(rep_sig->repository, mtrans) )
      return 1;
    rep_sig = rep_sig->next;
  }

  return 0;
}

int check_rep_sig(char * rep, mirror_transaction * mtrans){
  char pgppath[strlen(ci.pgp_dir) + strlen(rep) + 5];
  sprintf(pgppath, "%s/%s", ci.pgp_dir, rep);

  return(_check_rep_sig(rep, pgppath, mtrans));
}

/* 
 * We have a signature that claims to be from someone we trust, just need
 * to actually verify it
 */
int _check_rep_sig(char * rep, char * pgppath, mirror_transaction * mtrans){
  /* copy the input file: split the input file into two parts: 
   * -> from 0 - rep->fpos_start              <- portion to be verified
   * -> from rep->fpos_start - rep->fpos_end  <- portion to do verification
   */
  /* XXX - when checking sig, check the Hex id of the valid signer  */
  rep_sig_h * rep_sig = &mtrans->rep_sig;
  char * copied_file = NULL,* sigfile = NULL;
  FILE * fp = NULL;

  /* find the right repository header */
  while(rep_sig){
    if( !strcmp(rep_sig->repository, rep) )
      break;
    rep_sig = rep_sig->next;
  }
  
  /* copy the file (we still need to put the whole file in our journal */
  copied_file = tempnam(ci.db_dir, "mtr.");
  fcopy(mtrans->trans_begin.pln_fname, copied_file);
  
  /* strip out our repository signature */
  if( (fp = fopen(copied_file, "r")) == NULL ){
    trace(ERROR, default_trace, "check_rep_sig: Error opening file %s: %s\n", copied_file, strerror(errno));
    free(copied_file);
    return 0;
  }
  fseek(fp, rep_sig->fpos_start, SEEK_SET);
  sigfile = tempnam(ci.db_dir, "mtr.");
  if( !dump_pgp_block(fp, sigfile) ){
    trace(ERROR, default_trace, "check_rep_sig: Error dumping pgp block\n");
    remove(copied_file);
    remove(sigfile);
    free(copied_file);
    free(sigfile);
    fclose(fp);
    return 0;
  }
  fclose(fp);

  /* truncate copy */
  truncate(copied_file, rep_sig->fpos_start);

  /* finally, check the signature */
  {
    pgp_data_t pdat;
    char_list_t * tmp;
    rps_database_t * db = find_database(rep);

    if( !pgp_verify_detached(default_trace, pgppath, copied_file, sigfile, &pdat) ){
      trace(ERROR, default_trace, "check_rep_sig: Bad signature from %s\n", rep);
      return 0;
    }
    
    /* pdat tells us who signed, check it against what we expected */
    /* the db->hexid here should be filled in by reading the repository object,
     * it is NOT initialized from the irrd.conf file */
    for(tmp = pdat.hex_first; db && tmp; tmp = tmp->next){
      if( !strcmp(tmp->key, db->hexid) )
	break;
    }
    
    remove(copied_file);
    remove(sigfile);
    free(copied_file);
    free(sigfile);

    if(db && tmp){
      pgp_free(&pdat);
      return 1;
    }
    else if(db && !tmp){
      pgp_free(&pdat);
      return 0;
    }
    pgp_free(&pdat);
  }
  
  /* we didn't know about DB (PGP ran, but didn't check hexid...oh well it's the only key anyway) */
  return 1;
}

char * sign_jentry(rps_database_t * db, char * jentry_nm){
  char pgppath[MAXLINE];
  char * detsigfile, *s;
  FILE * infile = NULL;
  rps_database_t * signing_db;

  /* put in rep sig object */
  if( (infile = open_db_jentry_xxx(db->name, "a", db->current_serial+1)) == NULL ){
    trace(ERROR, default_trace, "process_jentry_file: error opening jentry for append: %s\n", strerror(errno));
    return "Error opening jentry file";
  }

  for(signing_db = db_list; signing_db; signing_db = signing_db->next){
    if(db->authoritative){
      fprintf(infile, "\nrepository-signature: %s\n", signing_db->name); /* XXX - Sign as whom? */
      fflush(infile);
      
      snprintf(pgppath, MAXLINE, "%s/%s/", ci.pgp_dir, signing_db->name);
      detsigfile = tempnam(ci.db_dir, "mtr.");
      if( !pgp_sign_detached(default_trace, pgppath, signing_db->pgppass, signing_db->hexid, jentry_nm, detsigfile)){
	trace(ERROR, default_trace, "sign_jentry: Error signing file\n");
	return "Error signing file";
      }
      
      /* append the signature */
      fprintf(infile, "signature:\n");
      
      if( (s = append_file_to_file(infile, detsigfile, "+ ")) != NULL ){
	trace(ERROR, default_trace, "process_jentry_file: Error appending sig\n");
	return "Error appending signature";
      }
      
      remove(detsigfile);
      free(detsigfile);
    }
  }

  fclose(infile);
  
  return NULL;
}

char *
rpsdist_auth(rps_database_t * db, char * jentry_name){
  return "rpsdist_auth: User checking not implemented yet";
}

/*
 * See if we're already holding this transaction 
 * the DB is already locked before entry.
 */
int
currently_holding(rps_database_t * db,  long serial){
  mirror_transaction * mtrans = db->held_transactions;

  while(mtrans && (serial > mtrans->trans_label.serial) )
    mtrans = mtrans->next;
  
  if(mtrans && (serial == mtrans->trans_label.serial))
    return 1;

  return 0;
}

/*
 * We have an object who's source we've never seen before.  We need to get that
 * Source's repository object and keycert object before we can go any farther.
 * We do that by searching in the following order:
 *          - Root (IANA?)
 *          - Local IRRd (maybe an admin added it..or something)
 *          - DB who sent transaction
 */
char *
get_new_source_keycert(char * new_src, unsigned long from_host, char * reseed_addr, char * hexid){
  char irrd_out_name[256] = "mtr.XXXXXX";
  int conn_open = 0, sockfd;
  FILE * result;
  char * rep_cert_str = "repository-cert:[ \t]*PGPKEY-([a-e0-9]{8})[\n]?$";
  char * rep_query_str = "query-address:[ \t]*whois://([[:graph:]]+"
    "[ \t]*:[ \t]*[[:digit:]]+)?";
  char * rep_seed_str = "reseed-addr:[ \t]*(.*)[\n]?$";
  char * str_ptr;
  regex_t rep_query_re, rep_seed_re, rep_cert_re;
  regmatch_t args[2];
  char curline[MAXLINE];
  char * host, *port, * lasts;
  int port_val;

  /* XXX- try IANA */
  /*if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
    "repository", new_src, ci.root_name, MAX_IRRD_OBJ_SIZE, 
    ci.root_host, ci.root_port)) == NULL){*/

    /* XXX - IANA failed, try local IRRd */
    if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
				 "repository", new_src, NULL, MAX_IRRD_OBJ_SIZE, 
				 ci.irrd_host, ci.irrd_port)) == NULL){
      /* local failed, try upstream */
      
      /* get upstream's rep object (for query address) */
      if( (str_ptr = get_db_line_from_irrd_obj(find_host_db(from_host), find_host_db(from_host), 
					       ci.irrd_host, ci.irrd_port, "repository", rep_query_str)) == NULL){
	trace(ERROR, default_trace, "get_new_source_keycert: Failed to get upstream rep object\n");
	return NULL;
      }
      /* now we know where to query upstream, so do it */
      else{
	host = strtok_r(str_ptr, ":", &lasts);
	port = strtok_r(NULL, "", &lasts);
	port_val = atoi(port);
	
	if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
				     "repository", new_src, NULL, MAX_IRRD_OBJ_SIZE,
				     host, port_val)) == NULL){
	  trace(ERROR, default_trace, "get_new_source_keycert: Failed to get unknown rep object"
		" from upstream DB (%s: %s)\n", from_host, find_host_db(from_host));
	  free(str_ptr);
	  return NULL;
	}
	free(str_ptr);
      }
    }
    /* }*/
  
  /* result now contains the unknown rep's object */
  rewind(result);
  regcomp(&rep_query_re, rep_query_str, REG_EXTENDED);
  regcomp(&rep_seed_re, rep_seed_str, REG_EXTENDED);
  regcomp(&rep_cert_re, rep_cert_str, REG_EXTENDED);
  
  while(fgets(curline, MAXLINE, result)){
    if( !regexec(&rep_query_re, curline, 2, args, 0)){
      *(curline+args[1].rm_eo) = '\0';
      host = strdup(strtok_r(str_ptr, ":", &lasts));
      port = strtok_r(NULL, "", &lasts);
      port_val = atoi(port);
    }
    else if( !regexec(&rep_cert_re, curline, 2, args, 0)){
      *(curline+args[1].rm_eo) = '\0';
      strcpy(hexid, curline+args[1].rm_so);
    }
    else if( !regexec(&rep_seed_re, curline, 2, args, 0)){
      *(curline+args[1].rm_eo) = '\0';
      strcpy(reseed_addr, curline+args[1].rm_so);
    }
  }
  fclose(result);
  remove(irrd_out_name);

  if( (str_ptr = dump_keycert_from_db_to_file(new_src, hexid, host, port_val)) == NULL){
    trace(ERROR, default_trace, "get_new_source_keycert: Error getting keycert for %s\n", new_src);
    return NULL;
  }

  return str_ptr;
}

/*
 * Add new DB
 * We've determined we should add a new DB, we need to signify such to IRRd.
 * This opration needs to be atomic, as when we create the DB, IRRd will
 * try to fetch the entire DB Out of band (i.e. FTP).  We have to wait to
 * hear how IRRd does.
 */
int
add_new_db(char * new_name, char * new_hexid, char * get_from_addr){
  char * ret_code;
  char buf[MAXLINE];
  struct timeval tv = {1800, 0};     /* 30 minute timeout */
  fd_set read_fds;
  int fd = open_connection(default_trace, ci.irrd_host, ci.irrd_port);
  int n;
  long new_serial;
  rps_database_t * ndbp;

  if(fd < 0)
    trace(NORM, default_trace, "add_new_db: Error opening connection to %s(%d)\n", 
	  ci.irrd_host, ci.irrd_port);
  else{
    snprintf(buf, MAXLINE, "!C%s,%s\n", new_name, get_from_addr);
    if ((ret_code = send_socket_cmd (default_trace, fd, buf)) != NULL)
      return 0;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    if (select (fd + 1, 0, &read_fds, 0, &tv) < 1) {
      trace (ERROR, default_trace, "add_new_db: read time out!\n");
      return 0;
    }

    n = read (fd, buf, MAXLINE - 1);
    if( n <= 0 ){
      trace(ERROR, default_trace, "add_new_db: read error: %s\n", strerror(errno));
      return 0;
    }
    
    new_serial = atol(buf);
    
    if ((ret_code = read_socket_cmd (default_trace, fd, "C\n")) != NULL) {
      trace(ERROR, default_trace, "add_new_db: Read error, no C\\n after serial");
      return 0;
    }
    
    /* IRRd got the DB, and sent us what serial the DB covers to */
    ndbp = (rps_database_t *)malloc(sizeof(rps_database_t));
    if(!ndbp){
      trace(ERROR, default_trace, "add_new_db: Malloc error\n");
      return 0;
    }
    bzero(ndbp, sizeof(rps_database_t));
    ndbp->name = strdup(new_name);
    ndbp->hexid = strdup(new_hexid);
    add_db(ndbp);
    start_new_journal(ndbp, new_serial);
    return 1;
  }
  
  return 0;
}
