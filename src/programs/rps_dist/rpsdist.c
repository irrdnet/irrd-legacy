/*
 *  The persistant daemon that handles incoming floods,
 *  outgoing floods, etc.
 */
#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <mrt_thread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <trace.h>
#include <read_conf.h>
#include "rpsdist.h"

char * DFNAME = "/etc/irrd.conf";              /* default irrd.conf file */
trace_t * default_trace;                       /* logging */
config_info_t ci;                              /* filled in by read conf */
struct select_master_t user_submits;           /* the select master for the user sockets */
struct select_master_t mirror_floods;          /* the select master for mirror sockets */
rps_database_t * db_list = NULL;               /* list of known databases */
rpsdist_acl * mirror_acl_list = NULL;          /* ACL's */

static int run = 1;                            

pthread_mutex_t db_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t poll_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t need_poll = PTHREAD_COND_INITIALIZER;

static void mirror_listener(int port);
static void user_listener(int port);
static void poll_func(void);
static void daemonize(void);

/* print the usage.  prog_name is intended to be argv[0] */
void usage(char * prog_name){
  fprintf(stderr, "\nUsage:  %s [-f irrd.conf file] [-p irr_submit port] [-P mirror port] [-D]\n", prog_name);
  exit(1);
}

/* 
 * callback function, called from the rps_select. Set by the
 * init_rps_select function.  Whenever the select codes determines
 * there is new data on a socket, it spawns a thread running this
 * function. 
 */
void 
process_flood(int fd){
  mirror_transaction mtrans;

  /* get the corresponding object for this connection */
  rps_connection_t * irr = rps_select_find_fd(fd, &mirror_floods);

 MORE_FLOODS:
  bzero(&mtrans, sizeof(mirror_transaction));
  if( read_buffered_line(default_trace, irr) > 0 ){
    char * newline = NULL;

    /* it's some sort of normal mirror interaction */
    if( strip_trans_begin(irr, &mtrans) ){
      if( irr->db_name != NULL){
	mtrans.from_host = irr->host;
	if( sockn_to_file(irr, &mtrans.trans_begin.pln_fname, mtrans.trans_begin.length) )
	  handle_mirror_flood(&mtrans);
      }
      else
	trace(NORM, default_trace, "process_flood: Peer %s, an unknown DB, sent us a flood which we denied\n", irr->host);
    }
    
    /* else, maybe a heartbeat */
    else if( strip_heartbeat(irr) );
    
    /* else, maybe  a transaction request */
    else if( strip_trans_request(irr) );

    /* or, maybe a transaction response! */
    /*else if( strip_trans_response(irr) );*/

    /* check if we've got some good data buffered */
    if( (newline = strchr(irr->cp, '\n')) != NULL)
      goto MORE_FLOODS;

    /* we've processed all the input, re-add the fd */
    rps_select_add_peer(fd, &mirror_floods);
  }
  else
    rps_select_remove_peer(fd, &mirror_floods);
}

/*
 * The registered call back for handling user submissions from irr_submit 
 */
void
process_submit(int fd) {
  
  /* get the corresponding object for this connection */
  rps_connection_t * irr = rps_select_find_fd(fd, &user_submits);
  
  if( read_buffered_line(default_trace, irr) > 0 ){
    
    /* sanity check for !us */
    if( !strncmp( irr->tmp, "!us", 3 ) ){
      if( !handle_user_submit(irr) )
	flush_connection(irr);
    }

    /* we've processed all the input, re-add the fd */
    rps_select_add_peer(fd, &user_submits);
  }
  else
    rps_select_remove_peer(fd, &user_submits);
}

/* 
 * When we get Ctrl-C, do some cleanup
 */
void clean_exit(int signo){
  if( signo == SIGINT ){
    trace(NORM, default_trace, "Caught signal, stopping threads...\n");
    run = 0;
  }
}

/*
 * Main
 * Open the default trace, and get the command options.  Start listening on the 
 * intended ports.
 */
int
main(int argc, char * argv[]){  
  int u_port = 7777, m_port = 8000, status, redirect_trace = 0;
  char c;
  pthread_t mirror_thread, user_thread, poll_thread;

  default_trace = New_Trace2("rpsdist");

  while( (c = getopt(argc, argv, "f:p:P:Dv")) != -1 ){
    switch(c){
    case 'f':
      DFNAME = optarg;
      break;
    case 'p':
      u_port = atoi(optarg);
      break;
    case 'P':
      m_port = atoi(optarg);
      break;
    case 'D':
      daemonize();
      break;
    case 'v':
      redirect_trace = 1;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }

  rps_select_init(process_flood, &mirror_floods); /* set the call back functions */
  rps_select_init(process_submit, &user_submits);
  if( parse_irrd_conf_file(DFNAME, default_trace) == -1)    /* build the list of databases from the conf file (libhdrs)*/
    exit(EXIT_ERROR);
  if( redirect_trace )                            /* parse conf will set trace if one is found (undoing the -v flag) */
    set_trace(default_trace, TRACE_FLAGS, TR_ALL, TRACE_LOGFILE, "stdout", NULL);
  
  check_directories();                            /* check the db_dir, log_dir, and pgp_dir */
  parse_ci_srcs();                                /* convert the srcs into rps_databases */
  parse_ci_acls();                                /* convert the acls into rpsdist acls */
  init_databases();                               /* check for crash files, cleanup old files, etc.. */
  init_rps_regexes();                             /* compile frequently used regular expressions */
  
  /*signal(SIGINT, clean_exit);*/
  
  pthread_create(&mirror_thread, NULL, (void*)mirror_listener, (void*) m_port);   /* listen for mirrors */
  pthread_create(&user_thread, NULL, (void*)user_listener, (void*) u_port);       /* listen for irr_submit */
  pthread_create(&poll_thread, NULL, (void*)poll_func, (void*) NULL);             /* start the poll handler 
										     (for held transactions) */

  pthread_join(mirror_thread, (void*)&status);                                    /* go ahead and wait for em all to finish */
  pthread_join(user_thread, (void*)&status);
  pthread_join(poll_thread, (void*)&status);

  /* do cleanup - whatever that may be */
  return 1;
}       

/*
 *  Listen for flooded transactions, transaction requests, heartbeats..
 *  Anything connecting to this port will implicitly become a peer.
 */
void
mirror_listener(int port){
  struct sockaddr_in addr;
  int addrlen;
  int lsd, asd, i=1;
  rps_connection_t * peer;

  lsd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

  if(lsd == -1){
    fprintf(stderr, "mirror_listener: Unable to open socket\n");
    trace(ERROR, default_trace, "mirror_listener: Unable to open socket\n");
    exit(1);   
  }
  
  setsockopt(lsd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));         /* should this be? */

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  
  if( bind( lsd, (struct sockaddr *) &addr, sizeof( addr ) ) == -1 ){
    fprintf(stderr, "mirror_listener: Error binding to port %d\n", port);
    trace(ERROR, default_trace, "mirror_listener: Error binding to port %d\n", port);
    exit(1);   
  }

  trace(NORM, default_trace, "Listening for mirrors on port %d\n", port);
  listen(lsd,10);

  while(run) {
    addrlen = sizeof( addr );
    asd = accept(lsd,(struct sockaddr *) &addr, &addrlen);
    if( find_acl_long(addr.sin_addr.s_addr, &mirror_acl_list) ){
      if( (peer = rps_select_add_peer(asd, &mirror_floods)) == NULL)
	continue;
      peer->host = addr.sin_addr.s_addr;
      trace(NORM, default_trace, "Got a mirror connection from %s\n", inet_ntoa(addr.sin_addr));
    }
    else{
      trace(NORM, default_trace, "Mirror listener: refusing connection from: %s\n", inet_ntoa(addr.sin_addr));
      close(asd);
    }
  }

  close(lsd);
}

/*
 * Listen for irr_submits
 */
void
user_listener(int port){
  struct sockaddr_in addr;
  int addrlen;
  int lsd, asd, i;
  rps_connection_t * peer;

  lsd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

  if(lsd == -1){
    fprintf(stderr, "user_listener: Unable to open socket (%s)\n", strerror(errno));
    trace(ERROR, default_trace, "user_listener: Unable to open socket(%s)\n", strerror(errno));
    exit(1);   
  }
  
  setsockopt(lsd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));         /* should this be? */

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  
  if( bind( lsd, (struct sockaddr *) &addr, sizeof( addr ) ) == -1 ){
    fprintf(stderr, "user_listener: Error binding to port %d (%s)\n", port, strerror(errno));
    trace(ERROR, default_trace, "user_listener: Unable to bind to port %d (%s)\n", port, strerror(errno));
    exit(1);   
  }

  trace(NORM, default_trace, "Listening for submissions on port %d\n", port);
  listen(lsd,10);

  while(run) {
    addrlen = sizeof( addr );
    asd = accept(lsd,(struct sockaddr*) &addr, &addrlen);
    if( (peer = rps_select_add_peer(asd, &user_submits)) == NULL)
      continue;
    trace(NORM, default_trace, "Got an irr_submit connection from %s\n", inet_ntoa(addr.sin_addr));
  }

  close(lsd);
}

/*
 * The poll thread.  When a database has held transactions, it should
 * poll the remote DB to try and get them.  Rather than slow everything down,
 * this thread handles sending the request for all DB's.  It only send the requests,
 * the response will be handled by the proper callback.
 */
void
poll_func(){
  struct timeval now;
  int ret;

  trace(NORM, default_trace, "Polling thread started\n");

  pthread_mutex_lock(&poll_mutex);  
  while(run){
    gettimeofday(&now, NULL);
    now.tv_sec += POLL_INTERVAL;                                  
    if( (ret = pthread_cond_timedwait(&need_poll, &poll_mutex, &now)) != 0){ /* unlocked while waiting */
      if( ret != ETIMEDOUT ){
	trace(ERROR, default_trace, "poll_func: wait error: %s\n", strerror(errno));
	return;
      }
    }
    /* ok, either someone signalled us or we timed out, process DB queues */
    process_db_queues();
    check_mirror_connections();
  }
  pthread_mutex_unlock(&poll_mutex);  
}    

#ifndef NT
static void daemonize ()
{
  int pid, t;  
  int time_left;
  
  t = 0; /* This gets rid of a warning when t is #ifdef'd out of existance */
  
  trace(NORM, default_trace, "daemonize: Rps Dist daemonizing\n");

  /* alarm's time may not inherited by fork */
  time_left  = alarm (0);
  
  if ((pid = fork ()) == -1) {
    perror ("fork");
    exit (1);  
  }
  else if (pid != 0) {        /* parent */
    exit (0);
  } 
  
  
#ifdef HAVE_SETSID
  (void) setsid();
#else
#ifdef SETPGRP_VOID
  if ((t = setpgrp ()) < 0) {
    perror ("setpgrp");
    exit (1);
  } 
  
  signal (SIGHUP, SIG_IGN);
  /* fork again so that not being a process group leader */
  if ((pid = fork ()) == -1) {
    perror ("fork");
    exit (1);
  }
  else if (pid != 0) {        /* parent */
    exit (0);
  }
  
#else /* !SETPGRP_VOID */
  if ((t = setpgrp (0, getpid ())) < 0) {
    perror ("setpgrp");
    exit (1);
  }
  
  /* Remove our association with a controling tty */
  if ((t = open ("/dev/tty", O_RDWR, 0)) >= 0) {
    if (ioctl (t, TIOCNOTTY, NULL) < 0) {
      perror ("TIOCNOTTY");
      exit (1);
    }
    (void) close (t);  
  }
#endif /* SETPGRP_VOID */
#ifdef notdef
  /* Close all open files --- XXX need to check for logfiles */
  for (t = 0; t < 255; t++)
    (void) close (t);
#endif
#endif /* HAVE_SETSID */
  
  /*  chdir ("/"); code rewrite needed in some places */
  umask (022);
  if (time_left)
    alarm (time_left);
}
#endif /* NT */

