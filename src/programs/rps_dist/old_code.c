/*#include <stdio.h>
#include "rpsdist.h"
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <irrd_ops.h>

extern trace_t *default_trace;
extern char tmpfntmpl[];
FILE * dfile;
extern FILE * ofile;
extern config_info_t ci;

void add_missing_dbs(){
  regex_t db_rps_journal;
  regmatch_t args[2];
  struct dirent *dp;
  DIR *dirp;
  char buf[MAXLINE];
  
  regcomp(&db_rps_journal, "([[:alpha:]][a-z0-9_-]*[[:alnum:]]*)\.RPS.JOURNAL", REG_EXTENDED | REG_ICASE);

  if ((dirp = opendir (ci.db_dir)) != NULL) {
    while ((dp = readdir (dirp)) != NULL){
      if(!regexec(&db_rps_journal, dp->d_name, 2, args, 0) ){
	char db_name[(args[1].rm_eo - args[1].rm_so) + 1];
	memcpy(db_name, buf + args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
	db_name[args[1].rm_eo - args[1].rm_so] = '\0';
	if( !find_database(db_name) ) {
	  rps_database_t * new_db = (rps_database_t *)malloc(sizeof(rps_database_t));
	  bzero(new_db, sizeof(rps_database_t));
	  new_db->name = strdup(db_name);
	  pthread_mutex_init(&new_db->lock, NULL);
	}
      }
    }
    closedir(dirp);
  }
  else 
    trace(ERROR, default_trace, "add_missing_dbs: Could not open directory %s\n", ci.db_dir);
  
  regfree(&db_rps_journal);
  
  return;
  }
/* 
 * check the PGP signature, return 1 on success, negative on failure
 * At this point, fin points to the first line of the object, after the
 * blank line.  
 * The signature covers all the way up to the last blank line before the 
 * "signature" object.
 * Also, on a redistributed transaction from another mirror, the file
 * will have the
 * transaction-label object, which is in the signature.
 *
 *    Seperate into two files
 *       1. Message(tmpnam)
 *       2. Signature(tmpnam.asc)
 *    Call pgp
 *       (2.6.3 & 6.5 "pgp tmpnam.asc")
 *       (5 "pgpv tmpnam.asc" then requires user input "tmpnam"...dumb program) 
 *
 *  PGP is sensitive to the '\r' that telnets and sockets can add, make
 *  SURE to remove them or sigs will fail.
 *
 *  Type: whether the signatures we're checking are repository or user sigs.  Makes
 *        a difference because if it's rep, we want to skip user sigs. 
 *        (1 = ignore user, (repsig)
 *         0 = check user only (user sig))
 */
/*int check_pgp_sig (trace_t *tr, char * inflnm, int type){
  char *txt_name = tmpnam(NULL);
  char sig_name[256], rep_name[256], pgp_ver[20];
  FILE *txt_file = NULL, *sig_file = NULL, *results = NULL;
  char curline[MAXLINE];
  int  ret = 0, found_start = 0, found_end = 0;
  regmatch_t args[3];
  char *newline;
  FILE *fin = fopen(inflnm, "r");
  short last_blank = 0;
  regex_t pgpbeg_re, pgpend_re, version_re, goodsig_re;
  regex_t integrity_re, repsig_re, signature_re, blankline_re;
  char *pgpbeg     = "^[ \t]*[+]?-----BEGIN PGP SIGNATURE-----";
  char *pgpend     = "^[ \t]*[+]?-----END PGP SIGNATURE-----";
  char *version    = "^[ \t]*[+]?Version: (.*)";
  char *goodsig    = "^Good signature";
  char *signature  = "^signature:[\r]?[\n]?$";
  char *blankline  = "^[ \t]*[\r]?[\n]?$";
  char *repsig     = "^repository-signature:[\t]*([[:graph:]]+)[ \t]*[\r]?[\n]?$";
  char *integrity  = 
   "^integrity_re:[\t]*(legacy|no-auth|auth-failed|authorized)[ \t]*[\r]?[\n]?$";

  strcpy (sig_name, txt_name);
  strcat (sig_name, ".asc");
  
  regcomp (&pgpbeg_re,    pgpbeg,    REG_EXTENDED|REG_NOSUB);
  regcomp (&pgpend_re,    pgpend,    REG_EXTENDED|REG_NOSUB);
  regcomp (&version_re,   version,   REG_EXTENDED|REG_ICASE);
  regcomp (&goodsig_re,   goodsig,   REG_EXTENDED|REG_NOSUB);
  regcomp (&integrity_re, integrity, REG_EXTENDED|REG_ICASE|REG_NOSUB);
  regcomp (&repsig_re,    repsig,    REG_EXTENDED|REG_ICASE);
  regcomp (&signature_re, signature, REG_EXTENDED|REG_ICASE|REG_NOSUB);
  regcomp (&blankline_re, blankline, REG_EXTENDED|REG_NOSUB);

  if ((txt_file = fopen (txt_name, "w+")) == NULL){
    printf("Error checking PGP signature, couldn't write plain text file\n");
    ret = -1;
  }
  else{
    while (fgets(curline, MAXLINE-1, fin)) {
      /* remove \r's */
      if (!type)
        while ((newline = strchr (curline, '\r')) != NULL)
          memmove (newline, newline + 1, strlen (newline));
      
      /* check for sig */
      if (!regexec (&repsig_re, curline, 2, args, 0) && last_blank && type){
        memcpy (rep_name, curline+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
        rep_name[(args[1].rm_eo - args[1].rm_so)] = '\0';
        printf ("Match: repository %s\n", rep_name);
        fputs (curline, txt_file);
        fgets (curline, MAXLINE-1, fin);
        /* rep sig has upto 3 lines */
        if(!regexec (&signature_re, curline, 0, 0, 0) )
          break;
        else if (!regexec (&integrity_re, curline, 2, args, 0)) { 
          fputs (curline, txt_file);
          fgets (curline, MAXLINE-1, fin);
          /* rep sig has upto 3 lines */
          if(!regexec (&signature_re, curline, 0, 0, 0) )
            break;
        }
      }
      
      /* check for sig alone, i.e. new submission */
      if (!regexec (&signature_re, curline, 0, 0, 0) && !type)
        break;

      /* if we get a blank line */
      if (!regexec (&blankline_re, curline, 0, 0, 0))
        last_blank = 1;
      else
        last_blank = 0;

      fputs (curline, txt_file);
    }

    if (fclose (txt_file)) {
      printf ("Error closing %s: %s\n", txt_name, strerror(errno));
      ret = -1;
    }
  }

  /* only does ONE signature at the moment */
  found_start = 0;

  /* Build the signature file */

  /*dump the sig...first read should be first line of it. starts with a
'+' */
  if ((sig_file = fopen (sig_name, "w+")) == NULL) {
    printf("Error checking PGP signature, couldn't write signature file\n");
    ret = -1;
  }
  else {
    while (fgets (curline, MAXLINE-1, fin)) {
      /* remove \r's */
      if(!type)
        while( (newline = strchr(curline, '\r')) != NULL)
          memmove(newline, newline + 1, strlen(newline));

      /* if start line */
      if(!regexec (&pgpbeg_re, curline+1, 0, 0, 0)) {
        found_start = 1;
        fputs (curline+1, sig_file);
        fgets (curline, MAXLINE-1, fin);
        /* next line should be version */
        if(regexec (&version_re, curline+1, 2, args, 0)) {
          printf("No PGP version found, aborting\n");
          ret = -1;
          break;
        }
        memcpy (pgp_ver, curline+args[1].rm_so, (args[1].rm_eo -
args[1].rm_so));
        pgp_ver[(args[1].rm_eo - args[1].rm_so)] = '\0';
        printf ("Match pgp version %s\n", pgp_ver);
        fputs (curline+1, sig_file);
        fgets (curline, MAXLINE-1, fin);
      }

      if (found_start) {
        fputs (curline+1, sig_file);
        if (!regexec (&pgpend_re, curline, 0, 0, 0)) {
          found_end = 1;
          break;
        }
      }
    }

    if (fclose(sig_file)) {
      printf("Error closing %s: %s\n", sig_name, strerror(errno));
      ret = -1;
    }
  }

  /* See if we can verify via detached signature our submission */

  results = exec_pgp (tr, sig_name, 0);
  while (fgets(curline, MAXLINE-1, results)) {
    if (!regexec(&goodsig_re, curline, 0, 0, 0) ) {
      ret = 1;
      break;
    }
  }
  fclose(results);

  regfree (&pgpbeg_re);
  regfree (&pgpend_re);
  regfree (&version_re);
  regfree (&goodsig_re);

  return ret;
}

FILE *exec_pgp (trace_t *tr, char * inflnm, int sign_or_check){
  int childpid;
  int status;
  int p[2];
  FILE *pgppipe;

  if( pipe(p) < 0){
    trace(ERROR, tr, "Exec PGP: Pipe error\n");
    return NULL;
  }
  
  switch( (childpid = fork())) {
  case -1:
    trace(ERROR, tr, "Exec PGP: Fork Error\n");
    break;
  case 0:  /* child */
    dup2(p[0], 1);
    dup2(p[0], 2);
    close(p[1]);
    
    /* check */
    if( sign_or_check == 0){
      if( execlp("pgpv", "pgpv", inflnm, NULL) < 0 ){
        printf("execlp error");
        exit(1);
      }
    }
    /* sign */
    else{
      if( execlp("pgpv", "pgpv", "+force", "xxxx", "-ba", inflnm, NULL)
< 0 ){
        printf("execlp error");
        exit(1);
      }
    }
    break;
  default:
    break;
  }
  /* parent */
  while( (wait( &status)) != childpid);
  close(p[0]);
  if ((pgppipe = fdopen(p[1], "r")) == NULL) {
    trace (NORM, tr, "fdopen in PGP decode failed\n");
    return NULL;
  }
  return pgppipe;
}

int
  send_irr_submit_buffer(int sockfd, char * buf){
  struct   timeval tv = {5, 0};
  fd_set write_set;
  int wrote = 0;
  
  while(wrote < strlen(buf)){
  FD_SET(sockfd, &write_set);
  if(select(sockfd+1, NULL, &write_set, NULL, &tv) > 0)
      wrote += write(sockfd, buf, strlen(buf));
      else{
      trace(ERROR, default_trace, "send_irr_submit_buffer: error sending to irr_submit\n");
      break;
      }
  }
  trace(NORM, default_trace, "sent_irr_submit(%d): %s\n", wrote, buf);
  
  return 1;
  }


int pgpsign(char * inflnm, char * pgpdir){
  FILE * results = NULL;
  FILE * objfile = NULL;
  FILE * sigfile = NULL;
  char pgppath[MAXLINE];
  char curline[MAXLINE], sig_name[1024];
  char goodpass[] = "^Creating output file";
  regex_t goodpassre;
  int ret = 0;

   strcpy (pgppath, "PGPPATH=");
  strcat (pgppath, pgpdir);
  if (putenv (pgppath)) {
    trace (ERROR, default_trace, "rpsdist_update_pgp_ring () Couldn't set environment var PGPPATH\n");
    return 1;
  }

  printf("Signing %s\n", inflnm);

  results = exec_pgp(inflnm, 1);
  regcomp(&goodpassre, goodpass, REG_EXTENDED);

  while( fgets(curline, MAXLINE-1, results)) {
    printf("%s", curline);
    if( !regexec(&goodpassre, curline, 0, 0, 0) ){
      ret = 1;
      break;
    }
  }
  fclose(results);
  
  regfree(&goodpassre);
  
   if( (objfile = fopen(inflnm, "a+")) == NULL){
    trace(ERROR, default_trace, "pgp sign: error opening file\n");
    return -1;
  }

  strcpy(sig_name, inflnm);
  strcat(sig_name, ".asc");
  
  if( (sigfile = fopen(sig_name, "r")) == NULL){
    trace(ERROR, default_trace, "pgp sign: error opening file\n");
    return -1;
  }
  
  fprintf(objfile, "repository-signature: %s\nsignature:\n", "RADB");
 
  curline[0] = '+';
  while( fgets(curline+1, MAXLINE-1, sigfile) )
    fputs(curline, objfile);
  
  fclose(objfile);
  fclose(sigfile);
  remove(sig_name);

  return ret;
}
  
FILE * exec_pgp(char * inflnm, int sign_or_check){
  int childpid;
  int status;
  int p[2];
  FILE * pgppipe;
  char PGPPASS[] = "76xs650cc";

  if( pipe(p) < 0){
    trace(ERROR, default_trace, "Exec PGP: Pipe error\n");
    return NULL;
  }
  
  switch( (childpid = fork())) {
  case -1:
    trace(ERROR, default_trace, "Exec PGP: Fork Error\n");
    break;
  case 0: 
    dup2(p[0], 1);
    dup2(p[0], 2);
    close(p[1]);
    
    if( sign_or_check == 0){
      if( execlp("pgpv", "pgpv", inflnm, NULL) < 0 ){
	printf("execlp error");
	exit(1);
      }
    }
   
    else{
      if( execlp("pgps", "pgps", "+force", "-z", PGPPASS, "-ba", inflnm, NULL) < 0 ){
	printf("execlp error");
	exit(1);
      }
    }
    break;
  default:
    break;
  }
  
  while( (wait( &status)) != childpid);
  close(p[0]);
  if ((pgppipe = fdopen(p[1], "r")) == NULL) {
    trace (NORM, default_trace, "fdopen in PGP decode failed\n");
    return NULL;
  }
  
  return pgppipe;
}
*/
/* OLD FUNCTIONS */
/*char * build_db_xxx_file(rps_connection_t * irr, rps_database_t * db){
  FILE * fptr_db;
  if( (fptr_db = open_db_xxx(db->name, "w+", db->current_serial+1)) ){
    do{
      fputs(irr->tmp, fptr_db);
    } while(read_buffered_line(default_trace, irr) && strncmp(irr->tmp, "!ue", 3));
    
    if( !strncmp(irr->tmp, "!ue", 3) )
      fputs(irr->tmp, fptr_db);
    else
      return strdup("Dbuild_db_xxx_file: Error reading submission: no '!ue'");
    fclose(fptr_db);
  }
  else
    return strdup("Dbuild_db_xxx_file: Error creating file");

  return NULL;
}

*/
/* given a fully qualified filename, get the directory */
/*char * get_dir(char * full_path){
  char * last_slash = strrchr(full_path, '/');
  if(last_slash){
    int dir_length = (last_slash - full_path) + 1;
    char * dir = (char *)malloc(dir_length);
    memcpy(dir, full_path, dir_length);
    dir[dir_length] = '\0';
    return dir;
  }
  else
    return NULL;
}*/

/*int send_updates_to_irrd(char * inflnm, rps_database_t * src){*/
  /* irrd_ops:
   * char *send_transaction (trace_t *tr, char *warn_tag, int fd, char *op,
   * char *source, FILE *fin) 
   * inflnm starts with transaction-label:, ends with rep-signature.
   * we need to strip those and insert operations (ADD, DEL)
   */
/*
  char * pureflnm = make_pure_objects(inflnm);
  int in = open(pureflnm, O_RDONLY);
  char * ret;
  char buf[MAXLINE];
  int irrd_fd = open_connection(default_trace, ci.irrd_host, ci.irrd_port);
  char startBuf[200];
  char endBuf[] = "\n!ue\n";
  int numRead = 0;
  printf("Objects are in %s\n", pureflnm);
  
  if(src->authoritative)
    sprintf(startBuf, "!!\n!us%s\n", src->name);
  else
    sprintf(startBuf, "!!\n!ms%s\n", src->name);

  ret = send_socket_cmd(default_trace, irrd_fd, startBuf);
  if(ret)
    printf("1%s\n", ret);
  ret = read_socket_cmd(default_trace, irrd_fd, "C\n");
  numRead = read(in, buf, MAXLINE-1);
  buf[numRead] = '\0';
  ret = send_socket_cmd(default_trace, irrd_fd, buf);
  if(ret)
    printf("2%s\n", ret);
  ret = send_socket_cmd(default_trace, irrd_fd, endBuf);
  if(ret)
    printf("3%s\n", ret);
  
  if(ret)
    return 0;
  else
    return 1;
}

char * make_pure_objects(char * inflnm){
  FILE * in = fopen(inflnm, "r");
  char outflnm[L_tmpnam];
  char buf[MAXLINE];
  FILE * out;
  long fpos = 0;
  char add_str[] = "\nADD\n\n";
  char del_str[] = "\nDEL\n\n";
  long lastObj = 0;
    
  tmpnam(outflnm);
  out = fopen(outflnm, "w");
  
  if(!in || !out)
    return NULL;
  
  while(fgets(buf, MAXLINE-1, in)){
    if(!regexec(&trans_label, buf, 0, 0, 0) || !regexec(&sequence, buf, 0, 0, 0) ||
       !regexec(&timestamp, buf, 0, 0, 0) || !regexec(&integrity, buf, 0, 0, 0) ||
       !regexec(&rep_sig, buf, 0, 0, 0));
    
    else if(!regexec(&blank_line, buf, 0, 0, 0)){
      fpos = ftell(out);
      fputs(add_str, out);
    }
    else if(!strncmp(buf, "delete:", 7)){
      long tmpPos = ftell(out);
      fseek(out, fpos, SEEK_SET);
      fputs(del_str, out);
      fseek(out, tmpPos, SEEK_SET);
    }
    else if(!regexec(&signature, buf, 0, 0, 0))
      break;
    else{
      fputs(buf, out);
      lastObj = ftell(out);
    }
  }
  fclose(out);

  truncate(outflnm, lastObj);
  return strdup(outflnm);
}
*/


  
/* OLD CODE **/
/* see if the journal has an '#EOF'
 *  return
 *    1 it does
 *    0 it doesn't.
 */
/*int db_journal_has_eof(char * db_name){
  char curline[MAXLINE];
  FILE * fptr = open_db_journal(db_name, "a");
  int ret = 0;
 
  if(!fptr){
    trace(ERROR, default_trace, "Error opening %s journal", db_name);
    return 0;
  }

  while(fgets(curline, MAXLINE, fptr));

  if(!strcmp(curline, "#EOF"))
    ret = 1;
  else 
    ret = 0;
  
  fclose(fptr);
  return ret;
}
*/


/*
 * For a given DB, make sure it has a ./pgp/<db> directory, if
 * not, create one.  On error, exit.
 */
void init_db_pgp_dir(char * db_name){
  DIR * dirp;
  char dir[MAXLINE];

  strcpy(dir, ci.pgp_dir);
  if( (dirp = opendir (dir)) == NULL)
    if( mkdir(dir, S_IRWXU) == -1 ){
      trace(ERROR, default_trace, "init_db_pgp_dir: erro creating .pgp directory\n");
      printf("FATAL: while creating .pgp directory: %s\n", strerror(errno));
      exit(EXIT_ERROR);
    }
  
  strcat(dir, "/");
  strcat(dir, db_name);
  if( (dirp = opendir (dir)) == NULL)
    if( mkdir(dir, S_IRWXU) == -1 ){
      trace(ERROR, default_trace, "init_db_pgp_dir: erro creating .pgp/%s directory\n", db_name);
      printf("FATAL: while creating .pgp/%s directory: %s\n", strerror(errno), db_name);
      exit(EXIT_ERROR);
    }
}

/*
 * For a given open file, append the signature inside the sig_file_name
 */
char * append_rep_sig(FILE * dest, char * sig_file_name){
  FILE * sigfile;
  char curline[MAXLINE];

  if( (sigfile = fopen(sig_file_name, "r")) == NULL){
    trace(ERROR, default_trace, "append_rep_sig: error opening file %s\n", sig_file_name);
    return "append_rep_sig: error opening file\n";
  }
  
  fprintf(dest, "\nrepository-signature: %s\nsignature:\n", "RADB");
 
  curline[0] = '+';
  curline[1] = ' ';
  while( fgets(curline+2, MAXLINE-3, sigfile) )
    fputs(curline, dest);
  
  fclose(sigfile);

  return NULL;
}
/* LIST FUNCTIONS */
/* Find the source 'source' in the source list pointed
 * to by 'start'.
 *
 * Return:
 *   pointer to element if 'source' is found
 *   NULL otherwise
 */
rps_connection_t * find_peer_obj (rps_connection_t * list, char * host, int port) {
  rps_connection_t *obj = list;

  while (obj != NULL) {
    if ( !strcasecmp(obj->host, host) && (obj->port == port) ) 
      break;
    obj = obj->next;
  }
  
  return obj;
}


/* Create a rps_connection_t list element with value 
 *
 * Return:
 *   pointer to new element
 */
rps_connection_t * create_peer_obj (int fd, char * host, int port) {
  rps_connection_t * obj;
 
  if (fd < 0 || host == NULL || port <= 0)
    return NULL;

  obj = (rps_connection_t *) malloc (sizeof (rps_connection_t));
  obj->sockfd = fd;
  obj->host = strdup(host);
  obj->port = port;
  pthread_mutex_init(&obj->write, NULL);

  return (obj);
}
/* Add a source_t element to the list pointed to by 'start'.
 *
 * Return:
 *   void
 */
void add_peer_obj (rps_connection_t * list, rps_connection_t *obj) {
  printf("Inside add_peer_obj\n");
  
  if( !list ){
    list = obj;
  }
  else{
    /* add in front of list */
    obj->prev = NULL;
    obj->next = list;
    list->prev = obj;
  }
}
/*
 * Delete an object.  Search through from start to object.
 * Once get to object, relink list using previous item.
 */
void delete_peer_obj(rps_connection_t * list, rps_connection_t * obj) {
  rps_connection_t * temp = list;

  while(temp) {
    if(temp == obj)
      break;
    else
      temp = temp->next;
  }
  
  if(temp == NULL){
    printf("Object not found for delete\n");
    return;
  }
  
  if(temp->prev == NULL) /* if we're deleting the first item */
    list = temp->next;
  else
    temp->prev->next = NULL;

  free(temp);
}


/* creation... */
void write_trans_label(rps_database_t * db, FILE * fp, enum INTEGRITY_T integrity){
  char buf[100];
  time_stamp(buf, 100);

  fprintf(fp, "transaction-label: %s\nsequence: %ld\ntimestamp: %s\n",
	  db->name, db->current_serial+1, buf);
  switch(integrity){
  case LEGACY:
    fprintf(fp, "integrity: legacy\n");
    break;
  case NO_AUTH:
    fprintf(fp, "integrity: no-auth\n");
    break;
  case AUTH_FAIL:
    fprintf(fp, "integrity: auth-failed\n");
    break;
  case AUTH_PASS:
    fprintf(fp, "integrity: authorized\n");
    break;
  default:
    break;
  }

  fprintf(fp, "\n");
  fflush(fp);
}

/* for lack of a better place */
int parse_input(rps_connection_t * irr){
  regmatch_t args[5];
  rps_connection_t * temp_peer;
  int length;
  mirror_transaction mtrans;
  char rep_name[100], seqno[100], time[100];

  if(!irr)
    return 0;

  memset(&mtrans, 0, sizeof(mirror_transaction));

  read_buffered_line(default_trace, irr);
  
  /* if irr_submit is sending to us */
  if(!strncasecmp(irr->tmp, "!us", 3)){
    if( !handle_user_submit(irr) )
      flush_connection(irr);
  }
  
  /* * * * temp: add-peer * * * * */
  if(!regexec(&add_peer, irr->tmp, 5, args, 0) ){
    char host[100], port[100];
    memcpy(host, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
    host[(args[1].rm_eo - args[1].rm_so)] = '\0';
    memcpy(port, irr->tmp+args[2].rm_so, (args[2].rm_eo - args[2].rm_so));
    port[(args[2].rm_eo - args[2].rm_so)] = '\0';
    
    if(find_peer_obj(peer_list, host, atoi(port)))
      printf("Peer already exists\n");
    else{
      /* create a thread, open connection, handle connection() on it */
      /*add_peer_obj(&peer_list, 
	create_peer_obj(open_connection(default_trace,method, atoi(size)), method, atoi(size)));*/
      printf("add Host(%s), port(%s)\n", host, port);
      if ( (length = open_connection(default_trace, host, atoi(port))) < 0){
	printf("Bad open_connection\n");
	return length;
      }
      write(length, "!!\n!t600\n", 9); /* stay open, 10 minute timeout...later > heartbeat */
      add_peer_obj(peer_list, create_peer_obj(length, host, atoi(port)));
    }
    return length;
  }
  /********************* temp: rm-peer ********************/
  if(!regexec(&rm_peer, irr->tmp, 5, args, 0) ){
    char host[100], port[100];
    memcpy(host, irr->tmp+args[1].rm_so, (args[1].rm_eo - args[1].rm_so));
    host[(args[1].rm_eo - args[1].rm_so)] = '\0';
    memcpy(port, irr->tmp+args[2].rm_so, (args[2].rm_eo - args[2].rm_so));
    port[(args[2].rm_eo - args[2].rm_so)] = '\0';
    if(find_peer_obj(peer_list, host, atoi(port))){
      /*close(find_peer_obj(peer_list, host, atoi(port))->sockfd);*/
      delete_peer_obj(peer_list, find_peer_obj(peer_list, host, atoi(port)));
    }
    else
      write(irr->sockfd, "** Doesn't exist **\n", 14);
    return length;
  }
  if(!regexec(&sh_peer, irr->tmp, 5, args, 0) ){
    temp_peer = peer_list;
    while(temp_peer){
      printf("Host: %s, Port: %d, FD: %d\n", temp_peer->host, temp_peer->port, temp_peer->sockfd);
      temp_peer = temp_peer->next;
    } 
  }
  
  return length;
}
