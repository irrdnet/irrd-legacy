#include "rpsdist.h"
#include <stdarg.h>
#include <trace.h>
#include <regex.h>
#include <unistd.h>
#include <strings.h>
#include <irrd_ops.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h> /* inet_addr */

extern trace_t * default_trace;
extern config_info_t ci;

int synch_databases(rps_connection_t * irr, rps_database_t * db){
  char * ret_code, *str_ptr, *host, *port, *lasts;
  char * rep_query_str = "query-address:[ \t]*whois://([[:graph:]]+"
    "[ \t]*:[ \t]*[[:digit:]]+)?";
  char * j_str = "[a-z0-9_-]+:[a-z]{1}:[[:digit:]]+-([[:digit:]]+)[\n]?";
  regex_t j_reg;
  regmatch_t args[2];
  int port_val;
  long cs;

  trace(NORM, default_trace, "synch\n");

  /* query our irrd to get the query address for the DB */
  if( (str_ptr = get_db_line_from_irrd_obj(db->name, db->name, 
					   ci.irrd_host, ci.irrd_port, "repository", rep_query_str)) == NULL){
    trace(ERROR, default_trace, "synch_databases: Failed to get rep object\n");
    return 0;
  }
  else{
    host = strtok_r(str_ptr, ":", &lasts);
    port = strtok_r(NULL, "", &lasts);
    trace(NORM, default_trace, "Host: %s, port: %s\n", host, port);
    port_val = atoi(port);
  }
    
  /* using that query address, send a !j */
  if( (ret_code = irrd_curr_serial(default_trace, db->name, host, port_val)) == NULL ){
    trace(ERROR, default_trace, "synch_databases: error getting cs from irrd\n");
    free(str_ptr);
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
    trace(ERROR, default_trace, "handle_db_crash: Received invalid !j syntax (%s)\n", ret_code);
    regfree(&j_reg);
    return 0;
  }
  regfree(&j_reg);
  
  trace(NORM, default_trace, "cs: %ld", cs);

  free(str_ptr);

  /* compare that value to our value, if different send a trans request down irr */
  if( db->current_serial < cs ){
    int send_ok;
    
    trace(NORM, default_trace, "synch_databases: Remote DB is newer, requesting items %ld - %ld\n",
	  db->current_serial+1, cs);
    send_ok = send_trans_request(irr, db->name, db->current_serial+1,cs);
    if( !send_ok )
      trace(ERROR, default_trace, "process_db_queues: error sending transaction-request for %s\n", db->name);
  }

  return 1;
}


/* host can either be the dot notation or name of the
 * party you wish to allow
 */
int add_acl(char * host, char * db_name, rpsdist_acl ** acl_list){
  unsigned long addr;
  struct in_addr addr_str;

  addr = name_to_addr(host);
  
  addr_str.s_addr = addr;
  trace(NORM, default_trace, "add_acl: allowing %s\n", inet_ntoa(addr_str));

  if( !find_acl_long(addr, acl_list) ){
    rpsdist_acl * tmp = *acl_list;
    
    while(tmp && tmp->next) tmp=tmp->next;  /* find end */

    if(!tmp){
      if( (tmp = (rpsdist_acl *)malloc(sizeof(rpsdist_acl))) == NULL){
	trace (ERROR, default_trace, "add_acl: malloc error!\n");
	return 0;
      }
      *acl_list = tmp;
    }
    else{
      if( (tmp->next = (rpsdist_acl *)malloc(sizeof(rpsdist_acl))) == NULL){
	trace (ERROR, default_trace, "add_acl: malloc error!\n");
	return 0;
      }
      tmp = tmp->next;
    }
    
    bzero(tmp, sizeof(acl));
    tmp->db_name = db_name;  /* ci is always here */
    tmp->addr = addr;
  }
  
  return 1;
}

rpsdist_acl * find_acl_char(char * host, rpsdist_acl ** acl_list){
  unsigned long addr;

  addr = name_to_addr(host);
  
  return find_acl_long(addr, acl_list);
}

rpsdist_acl * find_acl_long(unsigned long addr, rpsdist_acl ** acl_list){
  rpsdist_acl * tmp;
  
  for(tmp=*acl_list; tmp; tmp = tmp->next)
    if(tmp->addr == addr)
      return tmp;

  return NULL;
}

int remove_acl_char(char * host, rpsdist_acl ** acl_list){
  unsigned long addr;

  addr = name_to_addr(host);

  return remove_acl_long(addr, acl_list);
}

int remove_acl_long(unsigned long addr, rpsdist_acl ** acl_list){
  rpsdist_acl * tmp = *acl_list;
  rpsdist_acl * prev = NULL;

  while( tmp && (tmp->addr != addr) ){
    prev = tmp;
    tmp = tmp->next;
  }

  if(prev)
    prev->next = tmp->next;
  if(tmp == *acl_list)
    *acl_list = tmp->next;
  free(tmp);

  return 1;
}

void flush_connection(rps_connection_t * irr){
  /* XXX - PROBLEM: if submissions are back to back, this may clear
   * valid submissions from the stream
   */
  while(read_buffered_line(default_trace, irr) > 0);
}

char * get_db_line_from_irrd_obj(char * obj_src, char * obj_key, char * host, 
				 int port, char * obj_type, char * line_regex){
  char irrd_out_name[256] = "mtr.XXXXXX";
  int conn_open=0, sockfd;
  FILE * result;
  char * id = NULL;

  if( !obj_src | !obj_key | !host | !port | !obj_type | !line_regex )
    return NULL;
  
  if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
			    obj_type, obj_key, obj_src, MAX_IRRD_OBJ_SIZE, host, port)) != NULL){
    regex_t line;
    regmatch_t args[2];
    char curline[MAXLINE];

    rewind(result);
    regcomp(&line, line_regex, REG_EXTENDED|REG_ICASE);
    while(fgets(curline, MAXLINE, result)){
      if( !regexec(&line, curline, 2, args, 0)){
	*(curline+args[1].rm_eo) = '\0';
	id = strdup(curline+args[1].rm_so);
	break;
      }
    }
  
    fclose(result);
    remove(irrd_out_name);
  }

  return id;
}


char * get_db_hexid_from_irrd(char * db_name, char * host, int port){
  char irrd_out_name[256] = "mtr.XXXXXX";
  int conn_open, sockfd;
  FILE * result;
  char * id = NULL;
  
  if( (result = IRRd_fetch_obj(default_trace, irrd_out_name, &conn_open, &sockfd, FULL_OBJECT,
			    "repository", db_name, db_name, MAX_IRRD_OBJ_SIZE, host, port)) != NULL){
    regex_t cert_line;
    regmatch_t args[2];
    char curline[MAXLINE];

    fseek(result, 0, SEEK_SET);
    regcomp(&cert_line, "repository-cert:[ \t]*PGPKEY-([a-e0-9]{8})[\n]?$", REG_EXTENDED|REG_ICASE);
    while(fgets(curline, MAXLINE, result)){
      if( !regexec(&cert_line, curline, 2, args, 0)){
	*(curline+args[1].rm_eo) = '\0';
	id = strdup(curline+args[1].rm_so);
      }
    }
  }   
  
  fclose(result);
  remove(irrd_out_name);
  return id;
}

/*
 * Dump directly from a socket to a file
 */
int sockn_to_file(rps_connection_t * irr, char ** filename, int length){
  FILE * fp;
  int cumul_size = 0;
  int cur_rd_size = 0;
  
  if(length == 0)
    return 0;
   
  /* generate a temp name.  if we have to ungzip it, give it a .gz
   * extension.  The plain text file will be the name minus the .gz 
   */
 
  *filename = tempnam(ci.db_dir, "mtr.");
  fp = fopen(*filename, "w+");
  
  while(cumul_size < length){
    if((cur_rd_size = read_buffered_amt(default_trace, 
					irr, (length - cumul_size))) > 0)
      fwrite(irr->tmp, cur_rd_size, 1, fp);
    else
      break;
    cumul_size += cur_rd_size;
  }

  fclose(fp);
  
  /* the amount we're told we got should be the same as amount we read */
  if(length != cumul_size){
    trace (NORM, default_trace, "sockn_to_file: size error: %d != %d\n", length, cumul_size);
    return -1;
  }
  return cumul_size;
}

/*
 *  Dump directly from a file to a socket/repository
 */
int _filen_to_sock(rps_connection_t * irr, FILE * fp, int size){
  char big_buf[50000];
  int cumul_size = 0;
  int cur_rd_size = 0;
  int cur_wt_size;

  while( cumul_size < size ) {
    cur_rd_size = fread(big_buf, 1, (size-cumul_size)%50000, fp);
    if( (cur_wt_size = safe_write(irr->sockfd, big_buf, cur_rd_size)) <= 0){
      trace(ERROR, default_trace, "filen_to_sock: Write error\n");
      return 0;
    }
    cumul_size += cur_wt_size;
  }
  
  return 1;
}


int filen_to_sock(rps_connection_t * irr, char * filename, int size){
  FILE * fp;
  int ret = 0;

  if( (fp = fopen(filename, "r")) == NULL){
    trace(ERROR, default_trace, "filen_to_sock: Error opening file %s: %s\n", filename, strerror(errno));
    return 0;
  }

  ret =  _filen_to_sock(irr, fp, size);
  
  fclose(fp);
  return ret;
}
  
/*
 * Make sure the amount of data we want to send is actually sent
 */
int safe_write(int sock, char * buf, int length){
  fd_set write_set;
  struct timeval tv = {10, 0};
  int cumul_write = 0;
  int cur_write;

  while(cumul_write < length ){
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);
    if( select(sock+1, NULL, &write_set, NULL, &tv) > 0 ) {
      cur_write = write(sock, buf+cumul_write, length-cumul_write);
      cumul_write += cur_write;
    }
    else {
      trace(ERROR, default_trace, "safe_write: Timeout writing to socket\n");
      return cumul_write;
    }
  }
  
  return cumul_write;
}   
    
/*
 * We had everything but a valid journal entry, fix up the journal.
 */
void trans_rollback(rps_database_t * db){
  fflush(db->journal);
  if( ftruncate(fileno(db->journal), db->jsize) == -1){
    trace(ERROR, default_trace, "trans_rollback: Could not roll back the %s journal: %s\n",
	  db->name, strerror(errno));
  }
  journal_write_eof(db);

  restore_pgp_rings(db);
}

/* Convert an RPS-DIST timestamp to a long value (ie, calendar format).
 *  
 * Input: 
 *    a timestamp of the form "YYYMMDD hh:mm:ss [+/-]hh:mm" (timestamp) 
 *    eg, "20000717 12:28:51 +04:00" 
 * 
 * Return: 
 *   -1 means there was a format error in the input timestamp argument 
 *   long value of the timestamp argument otherwise 
 */ 
long tstol (char * timestamp) {
  int hours, mins;
  char sign;
  time_t offset;
  struct tm tms = {0};   /* initialize all fields to 0 */

  if (sscanf (timestamp, "%4d%2d%2d %2d:%2d:%2d %c%2d:%2d", 
              &tms.tm_year, &tms.tm_mon, &tms.tm_mday, 
              &tms.tm_hour, &tms.tm_min, &tms.tm_sec,
              &sign,        &hours,      &mins) < 9)
    return -1;/* syntax error in date format */
  
  offset = (hours * 60 * 60) + (mins * 60);
  tms.tm_year -= 1900;
  tms.tm_mon--;

  return mktime (&tms) + ((sign == '+') ? -offset : offset);
}

/*
 *  Since we print timestamps so often, wrote this
 */
char * time_stamp(char * buf, int blen){
  time_t now;

  now = time (NULL);
  strftime (buf, blen, "%Y%m%d %H:%M:%S ", gmtime (&now));
 
  /* UTC_OFFSET is the [+/-]hh:mm value, ie, the amount
   * we are away from UTC time */
  strcat (buf, UTC_OFFSET);
  return buf;
}


/* 
 * find a database in our db_list struct
 */
rps_database_t * find_database(char * name){
  rps_database_t * tmp = db_list;
  char * newline, *newname;

  if( !name || !tmp || !tmp->name )
    return NULL;
  
  newname = strdup(name);

  newline = strchr(newname, '\n');
  if(newline)
    *newline = '\0';

  while( tmp ){
    if( !strcasecmp(tmp->name, newname) ){
      free(newname);
      return tmp;
    }
    tmp = tmp->next;
  }
  
  free(newname);
  return NULL;
}

/*
 * convert the first newline we see into a NULL
 */
void newline_to_null(char * buf){
  char * newline = strchr(buf, '\n');
  if(newline)
    *newline = '\0';
}

unsigned long name_to_addr(char * name){
   unsigned long addr;
   struct hostent *hostinfo;
   addr = inet_addr(name);
   
   if ((signed long) addr == -1) {
     if ((hostinfo = gethostbyname(name)) == NULL) {
       trace (NORM, default_trace,
	      "find_acl_char () ERROR resolving %s: (%s)\n",
	      name, strerror (errno));
       return 0;
     }
     memcpy(&addr, hostinfo->h_addr_list[0], sizeof(struct in_addr));
   }

  return addr;
}

/*
 * read a line from irr->buffer, fetch more if needed
 * return in irr->tmp
 */
int
read_buffered_line(trace_t * default_trace, rps_connection_t * irr){
  int n;
  char * newline = NULL;
  fd_set readset;
  struct timeval tv = {15, 0};
  
  /* check if we can get a line without reading, if so return it */
  if( (newline = strchr(irr->cp, '\n')) != NULL){
    newline++;
    memcpy(irr->tmp, irr->cp, (newline - irr->cp));
    irr->tmp[newline - irr->cp] = '\0';
    irr->cp = newline;
    return 1;
  }
  
  do{
    FD_ZERO(&readset);
    FD_SET(irr->sockfd, &readset);
    if( (select(irr->sockfd+1, &readset, NULL, NULL, &tv)) ) {
#ifdef NT
      if ((n = recv (irr->sockfd, irr->end, MAXLINE - (irr->end - irr->buffer) - 2,0)) <= 0) {
#else
	if ((n = read (irr->sockfd, irr->end, MAXLINE - (irr->end - irr->buffer) - 2)) <= 0) {
#endif /* NT */
	  if(errno == EAGAIN || errno == EINTR)
	    continue;
	  trace (NORM, default_trace, "read_buffered_line failed: %s\n",
		 strerror (errno));
	  return (-1);
	}
      else{
	irr->end += n;
	*(irr->end) = '\0';
	/* if buffer full and there's room, move down */
	newline = strchr(irr->cp, '\n');
	if( (irr->end >= (irr->buffer + MAXLINE - 2)) && (irr->cp > irr->buffer) ){
	  memmove(irr->buffer, irr->cp, (irr->end - irr->cp));
	  irr->end = irr->buffer + (irr->end - irr->cp);
	  *(irr->end) = '\0';
	  irr->cp = irr->buffer;
	  n = 1; /* read again */
	}
      }
    }
    else{
      trace(ERROR, default_trace, "Read_buffered_line: read timeout\n");
      return -1;
    }
  }while( (newline == NULL) && (irr->end < irr->buffer + MAXLINE - 2) && (n > 0));
  
  if(!newline)
    return 0;
  
  if(newline < irr->end) newline++;
  memcpy(irr->tmp, irr->cp, (newline - irr->cp));
  irr->tmp[newline - irr->cp] = '\0';
  irr->cp = newline;
  return 1;
}

/* for reading a certain amount of data 
 * from irr->buffer (read more if needed)
 * return in irr->tmp
 */
int
read_buffered_amt(trace_t * default_trace, rps_connection_t * irr, int length){
  int n;  
  fd_set readset;
  struct timeval tv = {15, 0};

  /* check if the amount we need is readily available, return it if so */
  if( (irr->end - irr->cp) >= length){
    memcpy(irr->tmp, irr->cp, length);
    irr->cp += length;
    return length;
  }
  
  do{
    FD_ZERO(&readset);
    FD_SET(irr->sockfd, &readset);
    if( (select(irr->sockfd+1, &readset, NULL, NULL, &tv)) ) {
#ifdef NT
      if ((n = recv (irr->sockfd, irr->end, MAXLINE - (irr->end - irr->buffer) - 2,0)) <= 0) {
#else
	if ((n = read (irr->sockfd, irr->end, MAXLINE - (irr->end - irr->buffer) - 2)) <= 0) {
#endif /* NT */
          if(errno == EAGAIN || errno == EINTR)
	    continue;
	  trace (NORM, default_trace, "read_buffered_amt failed: %s\n",
		 strerror (errno));
	  /*rps_remove_peer (irr->sockfd);*/
	  return (-1);
	}
      else{
	irr->end += n;
	*(irr->end) = '\0';
	if( (irr->end >= (irr->buffer + MAXLINE - 2)) && (irr->cp > irr->buffer) ){
	  memmove(irr->buffer, irr->cp, (irr->end - irr->cp));
	  irr->end = irr->buffer + (irr->end - irr->cp);
	  *(irr->end) = '\0';
	  irr->cp = irr->buffer;
	  n = 1; /* read again */
	}
      }
    }
    else{
      trace(ERROR, default_trace, "read_buffered_amt: read timeout\n");
      return -1;
    }
  }while( irr->end < (irr->buffer + MAXLINE - 2) && (irr->end - irr->cp < length) );
  
  if( (irr->end - irr->cp) > length)
    n = length;
  else
    n = irr->end - irr->cp;

  memcpy(irr->tmp, irr->cp, n);
  irr->cp += n;
  return n;
}
  

