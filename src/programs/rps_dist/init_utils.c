#include <stdio.h>
#include "rpsdist.h"
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <irrd_ops.h>
#include <regex.h>
#include <irr_defs.h>
#include <strings.h> /* bzero */
#include <pgp.h>

extern trace_t *default_trace;
extern config_info_t ci;
extern struct select_master_t mirror_floods;          /* the select master for mirror sockets */
extern rpsdist_acl * mirror_acl_list;

 /*
 * Before here we've parsed the irrd.conf file, and converted the ci.srcs into
 * rps_database_t's.  Now, see if there are any files remaining from another transaction. 
 * And fix up whatever we need to (journal, files, etc)
 */
int init_databases(void){
  rps_database_t * temp_db = db_list;
  char * s;

  /* remove any old files */
  remove_update_files();

  while(temp_db){
    /* check this db has it's own pgp directory */
    char db_pgp_dir[strlen(ci.pgp_dir) + strlen(temp_db->name) + 5];
    sprintf(db_pgp_dir, "%s/%s", ci.pgp_dir, temp_db->name);
    if( (s = dir_chks(db_pgp_dir, 1)) != NULL){
      trace(ERROR, default_trace, "init_databases: Directory error: %s\n", s);
      fprintf(stderr, "init_databases: A necessary directory gave the following error: %s\n", s);
      exit(EXIT_ERROR);
    }
    
    serial_from_journal(temp_db);                     /* load current serial */
    if( db_crash(temp_db) )                           /* did we crash last time? */
      handle_db_crash(temp_db);                       /* if so, handle it */
    
    /* remove these no matter what*/
    remove_db_crash_files(temp_db->name, 0);          /* remove all */
    remove_db_pgp_copies(temp_db->name);
  
    /* if the DB is trusted and not authoritative (a mirror), and we have the info, open a connection to it */
    /* XXX - use the MRT nonblock_connect() instead? */
    if( temp_db->trusted && !temp_db->authoritative && temp_db->host && temp_db->port ){
      if( !already_connected_to(temp_db->host) ){
	struct in_addr addr;
	int newsock;
	addr.s_addr = temp_db->host;
	newsock = open_connection(default_trace, inet_ntoa(addr), temp_db->port);
	if(newsock < 0)
	  trace(NORM, default_trace, "init_databases: Error opening connection to %s(%d)\n", 
		inet_ntoa(addr), temp_db->port);
	else{
	  rps_connection_t * peer;
	  if( (peer = rps_select_add_peer(newsock, &mirror_floods)) != NULL){
	    peer->host = temp_db->host;
	    peer->db_name = strdup(temp_db->name);
	    synch_databases(peer, temp_db);
	  }
	  trace(NORM, default_trace, "init_database: Opened connection to: %s (%d)\n", inet_ntoa(addr), temp_db->port);
	}
      }
    }
    temp_db = temp_db->next;
  }
  return 1;
}

void check_directories(){
  char * s;
  char dirname[strlen(DEF_DBCACHE) + 15];

  /* if a dir is NULL, we have to try and make one based off DEF_DBCACHE */
  if(ci.db_dir == NULL){
    trace(ERROR, default_trace, "check_directories:  Database directory NULL, most likely a permissions problem, using default\n");
    ci.db_dir = strdup(DEF_DBCACHE);
  }
  if(ci.log_dir == NULL){
    sprintf(dirname, "%s/log", ci.db_dir);
    ci.log_dir = strdup(dirname);
  }
  if(ci.pgp_dir == NULL){
    sprintf(dirname, "%s/.pgp", ci.db_dir);
    ci.pgp_dir = strdup(dirname);
  }

  if( (s = dir_chks(ci.db_dir, 1)) != NULL) {
    trace(ERROR, default_trace, "check_directories: Cache directory error: %s\n", s);
    fprintf(stderr, "check_directories: Your database cache directory (%s) is misconfigured: %s\n", ci.db_dir, s);
    exit(EXIT_ERROR);
  }

  if( (s = dir_chks(ci.log_dir, 1)) != NULL) {
    trace(ERROR, default_trace, "check_directories: Log directory error: %s\n", s);
    fprintf(stderr, "check_directories: Your log directory (%s) is misconfigured: %s\n", ci.log_dir, s);
  }

  if( (s = dir_chks(ci.pgp_dir, 1)) != NULL) {
    trace(ERROR, default_trace, "check_directories: Pgp directory error: %s\n", s);
    fprintf(stderr, "check_directories: Your pgp directory (%s) is misconfigured: %s\n", ci.pgp_dir, s);
    exit(EXIT_ERROR);
  }
}


/*
 * Compile frequently used regular expressions
 */
void init_rps_regexes(void){
  regcomp(&trans_label, "^transaction-label:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&sequence, "^sequence:[ \t]*([[:digit:]]+)[ \t]*[\r]?[\n]?$", 
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&timestamp, "^timestamp:[ \t]*([[:digit:]]{8}[ \t]+[[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2}[ \t]+[+-][[:digit:]]{2}:[[:digit:]]{2})[ \t]*[\n]?$", 
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&integrity, "^integrity:[ \t]*(legacy|no-auth|auth-failed|authorized)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&auth_dep, "^auth-dependency:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&rep_sig, "^repository-signature:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$", 
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&signature, "^signature:[ \t]*[\n]?$", (REG_EXTENDED|REG_ICASE));
  regcomp(&trans_begin, "^transaction-begin:[ \t]*([[:digit:]]+)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&trans_method, "^transfer-method:[ \t]*(gzip|plain)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&trans_request, "^transaction-request:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&seq_begin, "^sequence-begin:[ \t]*([[:digit:]]+)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&seq_end, "^sequence-end:[ \t]*([[:digit:]]+)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&trans_resp, "^transaction-response:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&heartbeat, "^heartbeat:[ \t]*([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
  regcomp(&blank_line, "^[ \t]*[\n]?$", (REG_EXTENDED|REG_ICASE));
  regcomp(&key_cert, "^key-cert:[ \t]*PGPKEY-([a-z0-9]{8})[ \t]*[\n]?$",
	  (REG_EXTENDED|REG_ICASE));
}

/*
 * Check to see if there are any <db>.trans.xxx or around,
 * if so, we crashed last time.
 *  RETURN:
 *     1    crashed
 *     0    ok
 *     -1   really bad
 */
int db_crash(rps_database_t * db){
  regex_t db_trans;
  regmatch_t args[2];
  struct dirent *dp;
  DIR *dirp;
  char buf[200];
  int ret = 0;

  /* sanity check...no seg faults */
  if(!db || !db->name){
    trace(ERROR, default_trace, "Corrupt db, NULL name value");
    return -1;
  }
  
  /* only look for <db>.trans.xxx file, "There can be only one" */
  snprintf(buf, 200, "%s\.trans\.[[:digit:]]{1-10}", db->name);
  regcomp(&db_trans, buf, REG_EXTENDED);

  /* check all the files in the directory */
  if ((dirp = opendir (ci.db_dir)) != NULL) {
    char serial[200];
    while ((dp = readdir (dirp)) != NULL){
      if(!regexec(&db_trans, dp->d_name, 2, args, 0) ){
 	memcpy(serial, dp->d_name + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
	serial[args[1].rm_eo - args[1].rm_so] = '\0';
	db->crash_serial = atol(serial);
	ret = 1;
	break;
      }
    }
    closedir(dirp);
  }
  else {
    trace(NORM, default_trace, "Could not open directory %s", ci.db_dir);
    regfree(&db_trans);
    return -1;
  }

  regfree(&db_trans);
  return ret;
}

/*
 * Check to see if a given transaction file is intact.  Meaning
 * The first line equals the last line
 */
int db_trans_file_intact(rps_database_t * db, long * start, char * pgp_file, long * jsize, long * end){
  FILE * fptr;
  char curline[MAXLINE];

  /* open <db>.<cs+1> file */
  if( !(fptr = open_db_trans_xxx(db->name, "r", db->crash_serial)) ){
    trace(NORM, default_trace, "db_trans_file_intact: Error opening a crash input file: %s\n", strerror(errno));
    return 0;
  }

  /* first line: cs+1 */
  if(fgets(curline, MAXLINE, fptr)){
    *start = atol(curline);
    if((start == 0) && (errno == ERANGE)){
      trace(NORM, default_trace, "db_trans_file_intact: Crash file had an invalid range: %s\n", curline);
      goto ERROR_CLEANUP;
    }
  }
  else {
    trace(NORM, default_trace, "db_trans_file_intact: Crash file had a bad line: %s\n", curline);
    goto ERROR_CLEANUP;
  }
   
  /* second line: pgpname */
  if( !fgets(pgp_file, MAXLINE, fptr)){
    trace(NORM, default_trace, "db_trans_file_intact: Crash file had a bad line: %s\n", curline);
    goto ERROR_CLEANUP;
  }

  /* third line: size of journal file */
  if(fgets(curline, MAXLINE, fptr)){
    *jsize = atol(curline);
    if((*jsize == 0) && (errno == ERANGE)){
      trace(NORM, default_trace, "db_trans_file_intact: Crash file had an invalid range: %s\n", curline);
      goto ERROR_CLEANUP;
    }
  }
  else {
    trace(NORM, default_trace, "db_trans_file_intact: Crash file had a bad line: %s\n", curline);
    goto ERROR_CLEANUP;
  }

  /* last line: cs+1 */
  if(fgets(curline, MAXLINE, fptr)){
    *end = atol(curline);
    if((*end == 0) && (errno == ERANGE)){
      trace(NORM, default_trace, "db_trans_file_intact: Crash file had an invalid range: %s\n", curline);
      goto ERROR_CLEANUP;
    }
  }
  else {
    trace(NORM, default_trace, "db_trans_file_intact: Crash file had a bad line: %s\n", curline);
    goto ERROR_CLEANUP;
  }
 
  fclose(fptr);

  if((*start == *end) && (*end == db->crash_serial))
    return 1;
  else
    return 0;

 ERROR_CLEANUP:
  fclose(fptr);
  return 0;
}

/* we've determined we're coming back up after a crash.
 * db->crash_serial is the last serial that matched the trans.xxx file.
 *
 *  RETURN
 *    1 ok
 *    0 error
 *   
 */    
int handle_db_crash(rps_database_t * db) {
  char pgp_file[MAXLINE];
  long start, end, jsize;
  char * ret_code;
  long cs;
  char * j_str = "[a-z0-9_-]+:[a-z]{1}:[[:digit:]]+-([[:digit:]]+)[\n]?";
  regex_t j_reg;
  regmatch_t args[2];

  /* open and check the db_trans file */
  if( !db_trans_file_intact(db, &start, pgp_file, &jsize, &end) ){
    trace(NORM, default_trace, "handle_db_crash: trans file was corrupted\n");
    return 0;
  }
   
  db->jsize = jsize;
  
  /* see if we committed it, if so we're done */
  if(db->current_serial == db->crash_serial)
    return 1;

  /* if we didn't write an EOF, it wasn't copied properly */
  if( find_eof(db->journal) == 0){
    trans_rollback(db);
    return 1;
  }

  /* now we know we most likely sent it to IRRd, see what happened */
  if( (ret_code = irrd_curr_serial(default_trace, db->name, ci.irrd_host, ci.irrd_port)) == NULL ){
    trace(ERROR, default_trace, "handle_db_crash: error getting cs from irrd\n");
    return 0;
  }
  
  regcomp(&j_reg, j_str, REG_EXTENDED|REG_ICASE);

  if( regexec(&j_reg, ret_code, 2, args, 0) == 0){
    *(ret_code+args[1].rm_eo) = '\0';
    cs = atol(ret_code+args[1].rm_so);
    if( (cs == 0) && (errno == ERANGE) ){
      trace(ERROR, default_trace, "handle_db_crash: IRRd sent us a garbage serial\n");
      regfree(&j_reg);
      return 0;
    }
  } else {
    trace(ERROR, default_trace, "handle_db_crash: Invalid !j syntax (%s)\n", ret_code);
    regfree(&j_reg);
    return 0;
  }
  regfree(&j_reg);
  
  /* if IRRd thinks the serial is the crash serial, it's in */
  if(cs == db->crash_serial){
    db->current_serial = cs;
    return 1;
  }
  
  /* else it failed */
  return 0;
}

/*
 * After we scan the IRRd file, the database are listed under the
 *  ci.srcs list.  Make a new list of rps_database_t type for use.
 * Make our own ACL list (read_conf only has host names, we want unsigned
 * long addresses for fast comparison)
 */
void parse_ci_srcs(){
  source_t * tmp;
  rps_database_t * ndbp;

  for(tmp = ci.srcs; tmp; tmp = tmp->next){
    if( !tmp->rpsdist_flag || !tmp->source )
      continue;
    
    ndbp = (rps_database_t *)malloc(sizeof(rps_database_t));
    if(!ndbp){
      trace(ERROR, default_trace, "parse_ci_srcs: Malloc error\n");
      fprintf(stderr, "parse_ci_srcs: Malloc error\n");
      exit(EXIT_ERROR);
    }
    bzero(ndbp, sizeof(rps_database_t));
    ndbp->name = tmp->source;
    ndbp->authoritative = tmp->authoritative;
    pthread_mutex_init(&ndbp->lock, NULL);
    ndbp->trusted = tmp->rpsdist_trusted;
    ndbp->pgppass = tmp->rpsdist_pgppass;
    if( tmp->rpsdist_host )
      ndbp->host = name_to_addr(tmp->rpsdist_host);
    if(tmp->rpsdist_port)
      ndbp->port = atoi(tmp->rpsdist_port);
    if( (ndbp->hexid = get_db_line_from_irrd_obj(ndbp->name, ndbp->name, ci.irrd_host, ci.irrd_port, "repository",
					    "repository-cert:[ \t]*PGPKEY-([a-f0-9]{8})[\n]?$")) == NULL){
      trace(ERROR, default_trace, "parce_ci_srcs: The database %s isn't in IRRD, Abort!\n", ndbp->name);
      fprintf(stderr, "parce_ci_srcs: The database %s isn't in IRRD, Abort!\n", ndbp->name);
      exit(EXIT_ERROR);
    }
    if(ndbp->authoritative)
      check_auth_db(ndbp);

    add_db(ndbp);
  }
}

void parse_ci_acls(){
  acl * tmp;
  
  for(tmp = ci.acls; tmp; tmp = tmp->next)
    add_acl(tmp->host, tmp->db_name, &mirror_acl_list);
}

/*
 * Each DB rpsdist is authoritative for will HAVE 
 * to have the hexid and pgppass specified.  This checks that they're
 * specified, and that the HEXid is a signing one.
 */
void check_auth_db(rps_database_t * db){
  char pgppath[MAXLINE];
  pgp_data_t pdat;

  if(!db->hexid){
    trace(ERROR, default_trace, "check_auth_db: Authoritative DB %s, has no hexid!\n", db->name);
    fprintf(stderr, "check_auth_db: Authoritative DB %s, has no hexid!\n", db->name);
    exit(EXIT_ERROR);
  }
  if(!db->pgppass){
    trace(ERROR, default_trace, "check_auth_db: Authoritative DB %s, has no pgppass!\n", db->name);
    fprintf(stderr, "check_auth_db: Authoritative DB %s, has no pgppass!\n", db->name);
    exit(EXIT_ERROR);
  }
  
  /* now check that the Hexid is the signing type */
  snprintf(pgppath, MAXLINE, "%s/%s", ci.pgp_dir, db->name);
  if( !pgp_fingerpr(default_trace, pgppath, db->hexid, &pdat) ){
    trace(ERROR, default_trace, "check_auth_db: Authoritative DB %s has an invalid fingerprint!\n", db->name);
    fprintf(stderr, "check_auth_db: Authoritative DB %s has an invalid fingerprint!\n", db->name);
    exit(EXIT_ERROR);
  }

  /* Houston, we have a signing key */
  if( pdat.sign_key )
    return;
  
  trace(ERROR, default_trace, "check_auth_db: The specified key (%s) for %s is not a signing key\n", db->hexid, db->name);
  fprintf(stderr, "check_auth_db: The specified key (%s) for %s is not a signing key\n", db->hexid, db->name);
  exit(EXIT_ERROR);
}
  
/* 
 * Used during initialization and during runtime to
 * add databases to the list 
 */
void add_db(rps_database_t * new_db){
  rps_database_t * tmp = db_list;

  trace(NORM, default_trace, "add_db: adding database: %s\n", new_db->name);

  if( tmp == NULL ){
    db_list = new_db;
    new_db->next = NULL;
    new_db->prev = NULL;
    return;
  }

  /* find end */
  while(tmp->next) tmp = tmp->next;

  tmp->next = new_db;
  new_db->prev = tmp;
  new_db->next = NULL;
}


  
  
