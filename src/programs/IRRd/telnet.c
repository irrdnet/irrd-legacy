
/*
 * $Id: telnet.c,v 1.19 2001/08/09 20:15:16 ljb Exp $
 * originally Id: telnet.c,v 1.59 1998/08/03 17:29:10 gerald Exp 
 */

/* Most of this code was taken from the MRT user.c library. I'm not sure what
 * all -- shrug, even most -- of the code does. Ask Masaki. Seems to work.
 */

#include <stdio.h>
#include <string.h>
#include "mrt.h"
#include "select.h"
#include "trace.h"
#include <time.h>
#include <ctype.h>
#include <signal.h>
#ifndef NT
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif /* NT */
#include <stdarg.h>
#include "config_file.h"
#include <sys/types.h>
#include "irrd.h"
#include <fcntl.h>
#ifndef NT
#ifndef SETPGRP_VOID
#include <sys/termios.h>
#endif
#endif /* NT */

#include "config_file.h"

extern int IPV4;
extern trace_t *default_trace;
extern irr_t IRR;

static int irr_read_command (irr_connection_t * irr);
/* JW use now in commands.c to give user error feedback
   in ripe whois mode.
void irr_write  (irr_connection_t *irr, int fd, char *buf, int len);
*/

/* local yokel's */
void irr_write_mem_answer (LINKED_LIST *, int, enum ANSWER_T, irr_connection_t *);
void irr_write_answer     (char *, char **, int *, irr_answer_t *, irr_connection_t *);
void long_fields_output   (irr_connection_t *, char *, char **);
static int count_lines    (irr_answer_t *);
#ifndef HAVE_LIBPTHREAD    
static int irr_read_command_schedule (irr_connection_t *irr);
#endif /* HAVE_LIBPTHREAD */

void *start_irr_connection (irr_connection_t * irr_connection) {
  fd_set          read_fds;
  struct timeval  tv;
  int		  ret;

#ifdef HAVE_LIBPTHREAD
#ifndef NT
  sigset_t set;

  sigemptyset (&set);
  sigaddset (&set, SIGALRM);
  sigaddset (&set, SIGHUP);
  pthread_sigmask (SIG_BLOCK, &set, NULL);
#endif /* NT */
#endif /* HAVE_LIBPTHREAD */

  irr_connection->cp = irr_connection->buffer;
  irr_connection->end = irr_connection->buffer;
  irr_connection->end[0] = '\0';

  trace (NORM, default_trace, "IRR connection from %s:%d\n",
	 prefix_toa (irr_connection->from), irr_connection->sockfd);

#ifndef HAVE_LIBPTHREAD    
  select_add_fd (irr_connection->sockfd, SELECT_READ,
		 (void *) irr_read_command_schedule, irr_connection);
  return NULL;
#endif /* HAVE_LIBPTHREAD */

  FD_ZERO(&read_fds);
  FD_SET(irr_connection->sockfd, &read_fds);

  while (1) {
    /* set connection timeout */
    tv.tv_sec = irr_connection->timeout;
    tv.tv_usec = 0;

    ret = select (irr_connection->sockfd + 1, &read_fds, 0, 0, &tv);
    if (ret <= 0) {
      if (ret == 0) 
	trace (NORM, default_trace, "-- IRR Connection Timeout -- \n");

      trace (NORM, default_trace,
	     "ERROR on IRR select (before read). Closing connection (%s)\n",
	     strerror (errno));
      irr_destroy_connection (irr_connection);
      return NULL;
    }

    ret = irr_read_command (irr_connection);

    if (ret < 0) {
	trace (NORM, default_trace, "IRR Connection Aborted\n");
	return NULL;
    }

    if (((ret == 1) && (irr_connection->stay_open == 0)) ||
	(irr_connection->scheduled_for_deletion == 1)) {
      /*trace (NORM, default_trace,
	"Closing connection.  stay_open set to close.\n");*/
      irr_destroy_connection (irr_connection);
      return NULL;
    }
  }
}


int irr_accept_connection (int fd) {
  int sockfd;
  int len, family;
  prefix_t *prefix;
  struct sockaddr_in addr;
  irr_connection_t *irr_connection;
  u_int one = 1;
  char tmp[BUFSIZE];
  irr_database_t *database;

  len = sizeof (addr);
  memset ((struct sockaddr *) &addr, 0, len);
  
  if ((sockfd = accept (fd, (struct sockaddr *) &addr, &len)) < 0) {
    trace (ERROR, default_trace, "ERROR -- IRR Accept failed (%s)\n",
	   strerror (errno));
    select_enable_fd (fd);
    return (-1);
  }
  select_enable_fd (fd);

  if (setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &one,
		  sizeof (one)) < 0) {
    trace (NORM | INFO, default_trace, "IRR setsockoptfailed\n");
    return (-1);
  }
  
  if ((family = addr.sin_family) == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
    prefix = New_Prefix (AF_INET, &sin->sin_addr, 32);
  }  
  else {
    trace (ERROR, default_trace, "IRR ERROR unknown connection family = %d\n",
	   family);
    close (sockfd);
    return (-1);
  }  

  /* check load */
  if (IRR.connections > IRR.max_connections) {
    trace (INFO, default_trace, "Too many connections -- REJECTING %s\n",
	   prefix_toa (prefix));
    Deref_Prefix (prefix);
    close (sockfd);
    return (-1);
  }

  /* Apply access list (if one exists) */
  if (IRR.irr_port_access > 0) {
    if (!apply_access_list (IRR.irr_port_access, prefix)) {
      trace (NORM, default_trace, "IRR connection DENIED from %s\n",
	     prefix_toa (prefix));
      Deref_Prefix (prefix);
      close (sockfd);
      return (-1);
    }
  }

  trace (TRACE, default_trace, "IRR accepting connection from %s\n",
	 prefix_toa (prefix));

  IRR.connections++;

  irr_connection = New (irr_connection_t);
#ifndef HAVE_LIBPTHREAD    
  irr_connection->schedule = New_Schedule ("irr_connection", default_trace);
#else
  irr_connection->schedule = NULL;
#endif
  irr_connection->sockfd = sockfd;
  irr_connection->from = prefix;
  irr_connection->ll_database = LL_Create (0);
  irr_connection->timeout = 60; /* *5;  default timeout of five minutes */
  irr_connection->full_obj = 1;
  irr_connection->end = irr_connection->buffer;
  irr_connection->start = time (NULL);
  irr_connection->ENCODING = strdup("gzip"); /* be sure to free this */
  
  if (pthread_mutex_lock (&IRR.connections_mutex_lock) != 0)
    trace (NORM, default_trace, "Error locking -- connection_mutex_lock--: %s\n",
	   strerror (errno));

  LL_Add (IRR.ll_connections, irr_connection);

  if (pthread_mutex_unlock (&IRR.connections_mutex_lock) != 0)
    trace (NORM, default_trace, "Error unlocking -- connection_mutex_lock--: %s\n",
	   strerror (errno));

  /* by default, use all databases in order appear IRRd config file */
  LL_Iterate (IRR.ll_database, database) {

    /* Apply access list (if one exists) */
    if ((database->access_list > 0) &&
	(!apply_access_list (database->access_list, prefix))) {
      trace (NORM, default_trace, "Access to %s denied for %s...\n",
	     database->name, prefix_toa (prefix));
    }
    else {
      if (! (database->flags & IRR_NODEFAULT))
	LL_Add (irr_connection->ll_database, database);
    }
  }

  sprintf (tmp, "IRR %s", prefix_toa (prefix));
  mrt_thread_create (tmp, irr_connection->schedule,
		     (thread_fn_t) start_irr_connection, irr_connection);
  return (1);
}


/*
 * begin listening for connections on a well known port
 */
int 
listen_telnet (u_short port) {
  struct sockaddr_in serv_addr;
  struct sockaddr *sa;
  int len, optval = 1;
  int sockfd;

  /* this port has not been configured */
  if (port <= 0) return (0);

  memset (&serv_addr, 0, sizeof (serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  serv_addr.sin_port = htons (port);
  sa = (struct sockaddr *) &serv_addr;
  len = sizeof (serv_addr);
  if ((sockfd = socket (sa->sa_family, SOCK_STREAM, 0)) < 0) {
    trace (ERROR, default_trace, 
	   "IRR ERROR -- Could not get socket (%s)\n",
	   strerror (errno));
    return (-1);
  }

  if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR,
		  (const char *) &optval, sizeof (optval)) < 0) {
    trace (ERROR, default_trace, "IRR ERROR -- Could setsocket (%s)\n",
	   strerror (errno));
  }
  
  if (bind (sockfd, sa, len) < 0) {
    trace (ERROR, default_trace, 
	   "IRR ERROR -- Could not bind to port %d (%s)\n",
	   port, strerror(errno));
    return (-1);
  }

  listen (sockfd, 5);

  trace (NORM, default_trace,
	 "IRR listening for connections on port %d (socket %d)\n",
	 port, sockfd);
                   
  select_add_fd (sockfd, 1, (void_fn_t) irr_accept_connection, (void *) sockfd);

  return (sockfd);
}

#ifndef HAVE_LIBPTHREAD
static int irr_read_command_schedule (irr_connection_t *irr) {
  schedule_event (irr->schedule, (void *) irr_read_command, 1, irr);
	return (1);
}
#endif /* HAVE_LIBPTHREAD */

/* 
 * irr_read_command
 * A misnamed routine -- actually read command input into buffer and
 * and process the buffer. We may, or may not have a command...
 * If we have not processed a command, return 0 (1 otherwise)
 */
static int irr_read_command (irr_connection_t * irr) {
  int n, i, state;
  char *cp, *newline;
  int command_found = 0;


#ifdef NT
  if ((n = recv (irr->sockfd, irr->end, BUFSIZE - (irr->end - irr->buffer) - 2,0)) <= 0) {
#else
  if ((n = read (irr->sockfd, irr->end, BUFSIZE - (irr->end - irr->buffer) - 2)) <= 0) {
      /*if((n = read_socket_line(default_trace, irr->sockfd, irr->buffer, BUFSIZE)) <= 0) {*/
#endif /* NT */

    trace (NORM, default_trace, "IRR read failed\n",
	   strerror (errno));
    irr_destroy_connection (irr);
    return (-1);
  }

  irr->end += n;
  *(irr->end) = '\0'; /* necessary for IRR_MODE_LOAD_UPDATE case */
  state = irr->state;
  /* we should probably have this in a loop... */
  while ((newline = strchr (irr->buffer, '\r')) != NULL) {
    memmove (newline, newline + 1, irr->end - newline);
    irr->end--;
    n--;
  }

  while ((newline = strchr (irr->buffer, '\n')) != NULL ||
	 state == IRR_MODE_LOAD_UPDATE) {

    if (newline != NULL) {
      if (state == IRR_MODE_LOAD_UPDATE) {
	i = newline - irr->buffer + 1;
	memcpy (irr->tmp, irr->buffer, i);
	*(irr->tmp + i) = '\0';
      }
      else
	*newline = '\0';
    }
    else
      strcpy (irr->tmp, irr->buffer);

    command_found = 1;

    if (state != IRR_MODE_LOAD_UPDATE) {
      strcpy (irr->tmp, irr->buffer);

      /* remove tailing spaces */
      cp = irr->tmp + strlen (irr->tmp) - 1;
      while (cp >= (irr->tmp) && isspace ((int) *cp)) {
	*cp = '\0';
	cp--;
      }
      
      /* remove heading spaces */
      cp = irr->tmp;
      while (*cp && isspace ((int) *cp)) {
	cp++;
      }
      
      if (irr->tmp != cp)
	strcpy (irr->tmp, cp);
    }
    irr->cp = irr->tmp;

    irr_process_command (irr); 

    /* user has quit or we've unexpetdly terminated */
    if (irr->stay_open == 0) {
#ifndef HAVE_LIBPTHREAD
      irr_destroy_connection (irr);
#endif /* HAVE_LIBPTHREAD */
      return (1);
    }

    /* IRR_MODE_LOAD_UPDATE case:
     * there may not be a newline, so
     * process all 'n' chars in buffer
     */
    if (state == IRR_MODE_LOAD_UPDATE) {
      n -= strlen (irr->tmp);
      if (n <= 0) {
	irr->end = irr->buffer;
	break;
      }
    }

    newline++;
    if ((irr->end - newline) > 0) 
      memmove (irr->buffer, newline, irr->end - newline);
    irr->end = irr->buffer + (irr->end - newline);
    *(irr->end) = '\0';
  }

#ifndef HAVE_LIBPTHREAD
  select_enable_fd (irr->sockfd);
#endif /* HAVE_LIBPTHREAD */
  
  return (command_found);
}

int irr_destroy_connection (irr_connection_t * connection) {
#if 0
  int n;

  n = close (connection->sockfd);
  n = WSAGetLastError();
#endif

#ifndef HAVE_LIBPTHREAD
    select_delete_fd (connection->sockfd);
#else
    close (connection->sockfd);
#endif /* HAVE_LIBPTHREAD */

#if 0
    n = close (connection->sockfd);
    n = WSAGetLastError();
#endif

   trace (NORM, default_trace, 
	  "IRR Closing a connection from %s:%d (%d connections)\n", 
	  prefix_toa (connection->from),
	  connection->sockfd,
	  IRR.connections);

   /*LL_RemoveFn (IRR.ll_irr_connections, connection, 0);*/
   Deref_Prefix (connection->from);
   if (connection->ENCODING != NULL)
     free(connection->ENCODING);
#ifndef HAVE_LIBPTHREAD    
   if(connection->schedule->is_running > 0)
     /* if we running as a scheduled event, can't destroy schedule yet */
     delete_schedule (connection->schedule);
   else
     /* it's safe to destroy the schedule */
     destroy_schedule (connection->schedule);
#endif
   LL_Destroy (connection->ll_database);

   if (pthread_mutex_lock (&IRR.connections_mutex_lock) != 0)
     trace (NORM, default_trace, "Error locking -- connection_mutex_lock--: %s\n",
	    strerror (errno));

   LL_Remove (IRR.ll_connections, connection);

   if (pthread_mutex_unlock (&IRR.connections_mutex_lock) != 0)
     trace (NORM, default_trace, "Error locking -- connection_mutex_lock--: %s\n",
	    strerror (errno));

   if (connection->answer != NULL)
     Delete (connection->answer);

   Delete (connection);
   IRR.connections--;

#ifndef NT
   mrt_thread_exit ();
#endif /* NT */
   /* NOTREACHED */
   return(-1);
}

/* unformatted version of add_answer */
int irr_add_answer_fast (irr_connection_t *irr, char *buf) { 
  int len;

  if (irr->answer == NULL) {
    irr->answer = malloc (IRR_OUTPUT_BUFFER_SIZE);
    irr->cp = irr->answer;
    irr->answer_len = 0;
  }

  if (irr->answer_len > (IRR_OUTPUT_BUFFER_SIZE - 200)) {
    return (-1);
  }
  
  len = strlen (buf);
  memcpy (irr->cp, buf, len);
  irr->cp += len;
  irr->answer_len += len;
  return (1);
}

int irr_add_answer (irr_connection_t *irr, char *format, ...) {
  va_list args;
  int len;

  if (irr->answer == NULL) {
    irr->answer = malloc (IRR_OUTPUT_BUFFER_SIZE);
    irr->cp = irr->answer;
    irr->answer_len = 0;
  }

  if (irr->answer_len > (IRR_OUTPUT_BUFFER_SIZE - 200)) {
    return (-1);
  }

  va_start (args, format);
  vsprintf (irr->cp, format, args);  

  /*sprintf (irr->cp, "%s", s);*/
  len = strlen (irr->cp);
  irr->cp += len;
  irr->answer_len += len;

  return (1);
}

void irr_write_nobuffer (irr_connection_t *irr, int fd, char *buf, int len);

/* irr_send_bulk_data
 * A "more"  -- when we need to send pages of information (i.e. BGP table dumps)
 * out to the irr socket.
 *
 * When we get around to it, we should figure out how to
 *  1) get termcap string to clear screen
 *  2) and learn how big the screen is (how many lines). I think this comes during
 *     the telnet setup negotiations?
 *
 */
void irr_send_answer (irr_connection_t * irr) {
  char *cp;
  char tmp[20];
  int no_eol = 0;

  cp = irr->answer;
  if (cp == NULL) {
    sprintf (tmp, "D\n");
    irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
    return;
  }

  if (irr->answer[irr->answer_len -1] != '\n') {
    no_eol = 1;
  }

  /* add two bytes for terminating carrige return */
  sprintf (tmp, "A%d\n", irr->answer_len + no_eol); 
  irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
  irr_write_nobuffer (irr, irr->sockfd, irr->answer, irr->answer_len);
    
  if (no_eol) {
    sprintf (tmp, "\n");
    irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
  }

  sprintf (tmp, "C\n");
  irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));

  Delete (irr->answer);
  irr->answer = NULL;
  return;
}

/* irr_flush_final_answer
 * Called after we're done itterating through the database building up an answer.
 * This routine actually writes out to the socket, feeding it final_answer
 * structures built during irr_write
 */
void irr_write_buffer_flush (irr_connection_t *irr, int fd) {
  int n, ret;
  u_char *ptr;
  fd_set          write_fds;
  struct timeval  tv;
  final_answer_t *final_answer;

  /* something happened to the socket at some point -- delete before read */
  if (irr->scheduled_for_deletion)
    return;

  FD_ZERO(&write_fds);
  FD_SET(fd, &write_fds);

  if (irr->ll_final_answer == NULL) return;

  /* iterate through all of our linked answers */
  LL_Iterate (irr->ll_final_answer, final_answer) {
    ptr = final_answer->buf;
    /* len = final_answer->ptr - final_answer->buf; */

    while (ptr < final_answer->ptr) {
      tv.tv_sec = 3; /* 3 second timeout on trying to write to socket */
      tv.tv_usec = 0;

      /* select call with timeout --- add me !!!!!!!  */
      ret = select (fd + 1, 0, &write_fds, 0, &tv);
      if (ret <= 0) {
	trace (NORM, default_trace, "-- IRR Connection Timeout -- \n");
	trace (NORM, default_trace,
	       "ERROR on IRR select (before write). Closing connection (%s)\n",
	       strerror (errno));
	irr->scheduled_for_deletion = 1;
	return;
      }

#ifdef NT
	if ((n = send (fd, ptr, final_answer->ptr - ptr,0 )) < 0) {
#else
	if ((n = write (fd, ptr, final_answer->ptr - ptr)) < 0) {
#endif /* NT */
	  irr->scheduled_for_deletion = 1;
	  trace (NORM, default_trace, "Write error %s \n", strerror (errno));
	  /* free ll_final_answer structs!!!! */
	  LL_Destroy (irr->ll_final_answer);
	  return;
	}
	ptr += n;

	}
  }

  /* free ll_final_answer structs!!!! Need to write a destroy routine */
  LL_Destroy (irr->ll_final_answer);
  irr->ll_final_answer = NULL;
  return;
}

void irr_write_nobuffer (irr_connection_t *irr, int fd, char *buf, int len) {
  int n, ret;
  char *ptr;
  fd_set          write_fds;
  struct timeval  tv;
  ptr = buf;

  /* something happened to the socket at some point -- delete before read */
  if (irr->scheduled_for_deletion)
    return;

  FD_ZERO(&write_fds);
  FD_SET(fd, &write_fds);

  while ((ptr - buf) < len) {
    tv.tv_sec = 20; /* 20 second timeout on trying to write to socket */
    tv.tv_usec = 0;

    /* select call with timeout --- add me !!!!!!!  */
    ret = select (fd + 1, 0, &write_fds, 0, &tv);
    if (ret <= 0) {
      trace (NORM, default_trace, "-- IRR Connection Timeout -- \n");
      trace (NORM, default_trace,
             "ERROR on IRR select (before write). Closing connection (%s)\n",
             strerror (errno));
      irr->scheduled_for_deletion = 1;
      return;
    }

#ifdef NT
	if ((n = send  (fd, buf, len - (ptr-buf),0)) < 0) {
#else
    if ((n = write (fd, buf, len - (ptr-buf))) < 0) {
#endif /* NT */
      irr->scheduled_for_deletion = 1;
      trace (NORM, default_trace, "Write error %s \n", strerror (errno));
      return;
    }
    ptr += n;
  }
  return;
}

void delete_final_answer (final_answer_t *tmp) {
  Destroy (tmp->buf);
  Destroy (tmp);
}

/* irr_write_buffer
 * Just copy buf answer to memory buffers in a linked_list hung off the  
 * irr_connection structure.
 * We later call irr_answer_flush after we finish gathering answer and releasing
 * all the locks
 */
/* JMH TODO - n was throwing warnings due to being uninitialized.  Based
   on the way this is written, its not certain n is guaranteed to be set
   to a useful value.  For now, n is set to 0 and later we need to audit
   the code to see if we're guaranteed to get a useful value in here. */
void irr_write (irr_connection_t *irr, int fd, char *buf, int len) {
  int bytes, n = 0;
  char *ptr;
  final_answer_t *final_answer;

  ptr = buf;
  
  /* something happened to the socket at some point -- delete before read */
  if (irr->scheduled_for_deletion)
    return;

  /* fill in final_answer structures */
  while ((ptr - buf) < len) {

    /* oops, first time */ 
    if (irr->ll_final_answer == NULL) {
      irr->ll_final_answer = LL_Create (LL_DestroyFunction, delete_final_answer, 0);
      final_answer = New (final_answer_t);
      final_answer->buf = final_answer->ptr = malloc (4096); 
      LL_Add (irr->ll_final_answer, final_answer);
    }

    /* see if there is room in the last entry */
    final_answer = NULL;
    if ((final_answer = LL_GetTail (irr->ll_final_answer)) != NULL)
      n = 4096 - (final_answer->ptr - final_answer->buf);

    /* no room, we need to add another one */
    if ((final_answer == NULL) || n == 0) {
      final_answer = New (final_answer_t);
      final_answer->buf = final_answer->ptr = malloc (4096); 
      LL_Add (irr->ll_final_answer, final_answer);
      n = 4096;
    }

    /* write either all thats left, or as much room as left in buffer */
    if (n < (len- (ptr-buf))) 
      bytes = n;
    else
      bytes = len - (ptr-buf);

    memcpy (final_answer->ptr, ptr, bytes);
    ptr += bytes;
    final_answer->ptr += bytes;
  }

  return;
}

void irr_send_okay (irr_connection_t * irr) {
  char tmp[20];
  sprintf (tmp, "C\n");
  irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
}

void irr_send_error (irr_connection_t * irr, char *msg) {
  char tmp[256];

  if (msg != NULL)
    sprintf (tmp, "F%s\n", msg);
  else
    sprintf (tmp, "F\n");

  irr_write_nobuffer (irr, irr->sockfd, tmp, strlen (tmp));
  trace (NORM, default_trace, "Returned error: %s\n", tmp);
}

void send_dbobjs_answer (irr_connection_t *irr, enum INDEX_T index,
			 enum ANSWER_T answer_type, int mode) {
  char *cp; 
  char buffer[BUFSIZE];
  irr_answer_t *irr_answer;
  u_long answer_size = 0;
  int first = 1, space_left;
  enum DB_SYNTAX db_syntax = RIPE181;

  /* compute answer size */ /*JW this is wrong for long field output!, must fix */
  LL_ContIterate (irr->ll_answer, irr_answer) {
    if (first != 1 && answer_type == DB_OBJ) 
      answer_size += 1; /* need to add '\n' between db obj's for blank line */
    answer_size += (u_long) irr_answer->len;
    if (mode == RAWHOISD_MODE &&
	!irr->short_fields    &&
	answer_type == DB_OBJ &&
	irr_answer->db_syntax == RIPE181)
      answer_size += count_lines (irr_answer) * 11;
    first = 0;
    db_syntax = irr_answer->db_syntax;
  }

  /* return "D" empty answer return code */
  if (answer_size == 0) {
    /* JW
     * if (irr->stay_open) 
     */
    if (mode == RAWHOISD_MODE)
    	sprintf (buffer, "D\n");
    else  /* later read this string from configs, ie make it customizable */
	sprintf (buffer, "%%  No entries found for the selected source(s).\n");
    irr_write (irr, irr->sockfd, buffer, strlen (buffer));
    trace (NORM, default_trace, "Returned D -- no entries found\n");
    return;
  }

  /* JW
   * if (irr->stay_open) {
   */
  if (mode == RAWHOISD_MODE) {
    /* # of bytes in answer */
    sprintf (buffer, "A%d\n", (int) answer_size);
  
    /* write the answer to the socket buffer */
    cp = buffer + strlen(buffer);
    space_left = BUFSIZE - strlen(buffer) - 1;
  }
  else {
    cp = buffer;
    space_left = BUFSIZE - 1;
  }

  if (index == DISK_INDEX) {
    first = 1;
    LL_ContIterate (irr->ll_answer, irr_answer) {
      if (first != 1 && answer_type == DB_OBJ) {
	sprintf (cp, "\n");
	cp++;
	space_left -= 1;
      }
      irr_write_answer (buffer, &cp, &space_left, irr_answer, irr); 
      first = 0;
    }
  }
  else { /* MEM_INDEX */
    if (cp != buffer) /* flush answer length */
      irr_write (irr, irr->sockfd, buffer, cp - buffer);
    cp = buffer;
    irr_write_mem_answer (irr->ll_answer, irr->sockfd, answer_type, irr);
  }
    
  /* add "C" return code */
  /* JW
   *   if (irr->stay_open) { 
   */
  if (mode == RAWHOISD_MODE) {
    if (space_left < 2) {
      irr_write (irr, irr->sockfd, buffer, cp - buffer);
      cp = buffer;
    }
    sprintf (cp, "C\n");
    cp += 2;
  }

  /* flush anything left in the buffer */
  if (irr->short_fields || db_syntax == RPSL) 
    irr_write (irr, irr->sockfd, buffer, cp - buffer);
  else
    long_fields_output (irr, buffer, &cp);


  trace (NORM, default_trace, "Sent %d bytes\n", answer_size);  

} /* end send_dbobjs_answer() */

void irr_write_mem_answer (LINKED_LIST *ll_answer, int sockfd, 
			   enum ANSWER_T answer_type, irr_connection_t *irr) {
  int first = 1;
  irr_answer_t *irr_answer;

  LL_ContIterate (ll_answer, irr_answer) {
    if (!first && answer_type == DB_OBJ)
      irr_write (irr, sockfd, "\n", 1); 
    first = 0;
    irr_write (irr, sockfd, irr_answer->blob, irr_answer->len);
  }

} /* irr_write_mem_answer () */

void irr_write_answer (char *buf, char **cp, int *space_left, 
		       irr_answer_t *irr_answer, irr_connection_t *irr) {
  int bytes_to_read;

  if (fseek (irr_answer->fd, irr_answer->offset, SEEK_SET) < 0)
    trace (NORM, default_trace, "** Error ** fseek failed in irr_write_answer");

  while (irr_answer->len > 0) {
    if (*space_left >= irr_answer->len) 
      bytes_to_read = irr_answer->len;
    else
      bytes_to_read = *space_left;

    fread(*cp, 1, bytes_to_read, irr_answer->fd);
    *cp += bytes_to_read;
    *space_left -= bytes_to_read;

    if (*space_left < 3) { /* calling routine assumes space for newline */
      if (irr->short_fields || irr_answer->db_syntax == RPSL) 
      	irr_write (irr, irr->sockfd, buf, *cp - buf);      
      else
	long_fields_output (irr, buf, cp);
	
      *cp = buf;
      *space_left = BUFSIZE - 1;
    }
    
    irr_answer->len -= bytes_to_read;
  }

} /* end irr_write_answer() */

int count_lines (irr_answer_t * irr_answer) {
  char buf[BUFSIZE], *p;
  u_long len, bytes_to_read;
  int num_lines = 0;

  if (fseek (irr_answer->fd, irr_answer->offset, SEEK_SET) < 0) 
    trace (NORM, default_trace, "** Error ** fseek failed in count_lines");

  len = irr_answer->len;
  while (len > 0) {
    if (len > BUFSIZE)
      bytes_to_read = BUFSIZE - 1;
    else
      bytes_to_read = len;

    fread(buf, 1, bytes_to_read, irr_answer->fd);
    buf[bytes_to_read] = '\0';

    p = buf;
    while ((p = strchr (p, '\n')) != NULL) {
      num_lines++;
      p++;
    }

    len -= bytes_to_read;
  }

  return num_lines;
}

void irr_build_answer (irr_connection_t *irr, FILE *fd, enum IRR_OBJECTS type,
		       u_long offset, u_long len, char *blob, enum DB_SYNTAX syntax) {
  irr_answer_t *irr_answer;

  irr_answer = New (irr_answer_t);
  irr_answer->fd = fd;
  irr_answer->type = type;
  irr_answer->offset = offset;
  irr_answer->len = len;
  irr_answer->blob = blob;
  irr_answer->db_syntax = syntax;
  LL_Add (irr->ll_answer, irr_answer);
} /* end irr_build_answer() */

void irr_build_key_answer (irr_connection_t *irr, FILE *fd, char *dbname,
			   enum IRR_OBJECTS type, u_long offset, 
			   u_short origin) {
  char buffer[BUFSIZE], str_orig[10];

  irr_answer_t *irr_answer;


  strcpy (buffer, dbname);
  strcat (buffer, " ");
  if (get_prefix_from_disk (fd, offset, buffer) < 0)
    return;
  strcat (buffer, "-AS");
  sprintf (str_orig, "%d", origin);
  strcat (buffer, str_orig);
  strcat (buffer, "\n");
  irr_answer = New (irr_answer_t);
  irr_answer->len = strlen (buffer);
  irr_answer->blob = malloc (irr_answer->len);
  strcpy (irr_answer->blob, buffer);
  LL_Add (irr->ll_answer, irr_answer);
} /* end irr_build_key_answer() */

void long_fields_output (irr_connection_t * irr, char *buffer, char **cp) {
  char *p, *q;
  char *rt = "route:         ";
  char *cm = "community:     ";
  char *au = "authority:     ";
  char *de = "descr:         ";
  char *so = "source:        ";
  char *or = "origin:        ";
  char *mt = "mntner:        ";
  char *ch = "changed:       ";
  char *mb = "mnt-by:        ";
  char *ny = "notify:        ";
  char *mn = "mnt-nfy:       ";
  char *dt = "upd-to:        ";
  char *at = "auth:          ";
  char *ai = "as-in:         ";
  char *an = "aut-num:       ";
  char *ao = "as-out:        ";
  char *tc = "tech-c:        ";
  char *ac = "admin-c:       ";
  char *av = "advisory:      ";
  char *rm = "remark:        ";
  char *aa = "as-name:       ";
  char *gd = "guardian:      ";
  char *am = "as-macro:      ";
  char *al = "as-list:       ";
  char *pn = "person:        ";
  char *ad = "address:       ";
  char *ph = "phone:         ";
  char *fx = "fax-no:        ";
  char *nh = "nic-hdl:       ";
  char *em = "e-mail:        ";
  char *df = "default:       ";
  char *ir = "inet-rtr:      ";
  char *la = "localas:       ";
  char *if2 = "ifaddr:        ";
  char *pe = "peer:          ";
  char *ri = "rs-in:         ";
  char *rx = "rs-out:        ";
  char *cp2 = "contact-proc:  ";
  char *ne = "noc-email:     ";
  char *np = "noc-phone:     ";
  char *rc = "rtr-loc:       ";
  char *rr = "rtr-ver:       ";
  char *ry = "rtr-type:      ";
  char *cs = "comm-str:      ";
  char *cl = "comm-list:     ";
  char *wd = "withdrawn:     ";


  for (p = q = buffer; p < *cp; p = q + 1) {
    q = strchr (p, '\n');

    if (!strncasecmp (p, "*rt:", 4)) {
      irr_write (irr, irr->sockfd, rt, strlen (rt));
      p +=4;
    } 
    else if (!strncasecmp (p, "*de:", 4)) {
      irr_write (irr, irr->sockfd, de, strlen (de));
      p +=4;
    }
    else if (!strncasecmp (p, "*so:", 4)) {
      irr_write (irr, irr->sockfd, so, strlen (so));
      p +=4;
    }     
    else if (!strncasecmp (p, "*ch:", 4)) {
      irr_write (irr, irr->sockfd, ch, strlen (ch));
      p +=4;
    }     
    else if (!strncasecmp (p, "*mt:", 4)) {
      irr_write (irr, irr->sockfd, mt, strlen (mt));
      p +=4;
    }     
    else if (!strncasecmp (p, "*or:", 4)) {
      irr_write (irr, irr->sockfd, or, strlen (or));
      p +=4;
    }     
    else if (!strncasecmp (p, "*ai:", 4)) {
      irr_write (irr, irr->sockfd, ai, strlen (ai));
      p +=4;
    }     
    else if (!strncasecmp (p, "*ao:", 4)) {
      irr_write (irr, irr->sockfd, ao, strlen (ao));
      p +=4;
    }     
    else if (!strncasecmp (p, "*an:", 4)) {
      irr_write (irr, irr->sockfd, an, strlen (an));
      p +=4;
    }     
    else if (!strncasecmp (p, "*tc:", 4)) {
      irr_write (irr, irr->sockfd, tc, strlen (tc));
      p +=4;
    }     
    else if (!strncasecmp (p, "*ac:", 4)) {
      irr_write (irr, irr->sockfd, ac, strlen (ac));
      p +=4;
    }     
    else if (!strncasecmp (p, "*ny:", 4)) {
      irr_write (irr, irr->sockfd, ny, strlen (ny));
      p +=4;
    }     
    else if (!strncasecmp (p, "*at:", 4)) {
      irr_write (irr, irr->sockfd, at, strlen (at));
      p +=4;
    }     
    else if (!strncasecmp (p, "*dt:", 4)) {
      irr_write (irr, irr->sockfd, dt, strlen (dt));
      p +=4;
    }     
    else if (!strncasecmp (p, "*mn:", 4)) {
      irr_write (irr, irr->sockfd, mn, strlen (mn));
      p +=4;
    }     
    else if (!strncasecmp (p, "*mb:", 4)) {
      irr_write (irr, irr->sockfd, mb, strlen (mb));
      p +=4;
    }     
    else if (!strncasecmp (p, "*av:", 4)) {
      irr_write (irr, irr->sockfd, av, strlen (av));
      p +=4;
    }     
    else if (!strncasecmp (p, "*rm:", 4)) {
      irr_write (irr, irr->sockfd, rm, strlen (rm));
      p +=4;
    }         
    else if (!strncasecmp (p, "*aa:", 4)) {
      irr_write (irr, irr->sockfd, aa, strlen (aa));
      p +=4;
    }        
    else if (!strncasecmp (p, "*gd:", 4)) {
      irr_write (irr, irr->sockfd, gd, strlen (gd));
      p +=4;
    }        
    else if (!strncasecmp (p, "*am:", 4)) {
      irr_write (irr, irr->sockfd, am, strlen (am));
      p +=4;
    }    
    else if (!strncasecmp (p, "*al:", 4)) {
      irr_write (irr, irr->sockfd, al, strlen (al));
      p +=4;
    }    
    else if (!strncasecmp (p, "*pn:", 4)) {
      irr_write (irr, irr->sockfd, pn, strlen (pn));
      p +=4;
    }
    else if (!strncasecmp (p, "*ad:", 4)) {
      irr_write (irr, irr->sockfd, ad, strlen (ad));
      p+=4;
    }
    else if (!strncasecmp (p, "*ph:", 4)) {
      irr_write (irr, irr->sockfd, ph, strlen (ph));
      p+=4;
    }
    else if (!strncasecmp (p, "*fx:", 4)) {
      irr_write (irr, irr->sockfd, fx, strlen (fx));
      p+=4;
    }
    else if (!strncasecmp (p, "*nh:", 4)) {
      irr_write (irr, irr->sockfd, nh, strlen (nh));
      p+=4;
    }
    else if (!strncasecmp (p, "*em:", 4)) {
      irr_write (irr, irr->sockfd, em, strlen (em));
      p+=4;
    }
    else if (!strncasecmp (p, "*df:", 4)) {
      irr_write (irr, irr->sockfd, df, strlen (df));
      p+=4;
    }
    else if (!strncasecmp (p, "*ir:", 4)) {
      irr_write (irr, irr->sockfd, ir, strlen (ir));
      p+=4;
    }
    else if (!strncasecmp (p, "*la:", 4)) {
      irr_write (irr, irr->sockfd, la, strlen (la));
      p+=4;
    }
    else if (!strncasecmp (p, "*if:", 4)) {
      irr_write (irr, irr->sockfd, if2, strlen (if2));
      p+=4;
    }
    else if (!strncasecmp (p, "*pe:", 4)) {
      irr_write (irr, irr->sockfd, pe, strlen (pe));
      p+=4;
    }
    else if (!strncasecmp (p, "*ri:", 4)) {
      irr_write (irr, irr->sockfd, ri, strlen (ri));
      p+=4;
    }
    else if (!strncasecmp (p, "*rx:", 4)) {
      irr_write (irr, irr->sockfd, rx, strlen (rx));
      p+=4;
    }
    else if (!strncasecmp (p, "*cp:", 4)) {
      irr_write (irr, irr->sockfd, cp2, strlen (cp2));
      p+=4;
    }
    else if (!strncasecmp (p, "*ne:", 4)) {
      irr_write (irr, irr->sockfd, ne, strlen (ne));
      p+=4;
    }
    else if (!strncasecmp (p, "*np:", 4)) {
      irr_write (irr, irr->sockfd, np, strlen (np));
      p+=4;
    }
    else if (!strncasecmp (p, "*rc:", 4)) {
      irr_write (irr, irr->sockfd, rc, strlen (rc));
      p+=4;
    }
    else if (!strncasecmp (p, "*rr:", 4)) {
      irr_write (irr, irr->sockfd, rr, strlen (rr));
      p+=4;
    }
    else if (!strncasecmp (p, "*ry:", 4)) {
      irr_write (irr, irr->sockfd, ry, strlen (ry));
      p+=4;
    }
    else if (!strncasecmp (p, "*cs:", 4)) {
      irr_write (irr, irr->sockfd, cs, strlen (cs));
      p+=4;
    }
    else if (!strncasecmp (p, "*cm:", 4)) {
      irr_write (irr, irr->sockfd, cm, strlen (cm));
      p+=4;
    }
    else if (!strncasecmp (p, "*cl:", 4)) {
      irr_write (irr, irr->sockfd, cl, strlen (cl));
      p+=4;
    }
    else if (!strncasecmp (p, "*au:", 4)) {
      irr_write (irr, irr->sockfd, au, strlen (au));
      p+=4;
    }
    else if (!strncasecmp (p, "*wd:", 4)) {
      irr_write (irr, irr->sockfd, wd, strlen (wd));
      p+=4;
    }
  
    if (q == NULL) { /* flush rest of buffer and exit */
      irr_write (irr, irr->sockfd, p, *cp - p);
      break;
    }
    else
      irr_write (irr, irr->sockfd, p, q - p + 1);

  }
}

/* show_connections
 * List current RAWhoisd TCP connections to UII 
 */
void show_connections (uii_connection_t *uii) {
  irr_connection_t *connection;
  int i = 1;

  uii_add_bulk_output (uii, "Currently %d connection(s) [MAX %d]\r\n\r\n",
		       IRR.connections, IRR.max_connections);

  if (pthread_mutex_lock (&IRR.connections_mutex_lock) != 0)
    trace (NORM, default_trace, "Error locking -- connection_mutex_lock--: %s\n",
	   strerror (errno));

  LL_Iterate (IRR.ll_connections, connection) {
    uii_add_bulk_output (uii, "  %d  %s (fd=%d)  Age=%d\r\n", 
			 i++, prefix_toa (connection->from),
			 connection->sockfd,
			 time (NULL) - connection->start);
  }

  if (pthread_mutex_unlock (&IRR.connections_mutex_lock) != 0)
    trace (NORM, default_trace, "Error locking -- connection_mutex_lock--: %s\n",
	   strerror (errno));

  uii_send_bulk_data (uii);
  return;
}
