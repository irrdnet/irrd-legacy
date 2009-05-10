#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/time.h> /* needed for Linux */

#include <irrd_ops.h>
#include <hdr_comm.h>
#include <pgp.h>

#ifndef INADDR_NONE
#define	INADDR_NONE	((in_addr_t) 0xffffffff)
#endif

static char *start_irrd_session (trace_t *tr, int sockfd);
static int read_socket_obj (trace_t *tr, int sockfd, FILE *fobj, int obj_size, 
			    int len, int max_line_size);
/* JP moved to irrd_ops.h */
/*static char *send_socket_cmd (trace_t *tr, int fd, char *cmd);*/
/*static char *read_socket_cmd (trace_t *tr, int fd, char *response);*/
static char *send_transaction (trace_t *tr, char *warn_tag, int fd, char *op, 
			       char *source, FILE *fin);
static char *send_object (trace_t *, FILE *, int, char *);

/* open connection  */
int open_connection (trace_t *tr, char *IRRd_HOST, int IRRd_PORT) {
  int sockfd;
  struct sockaddr_in servaddr;
  struct hostent *hoste;
  in_addr_t addr;

  trace (TR_TRACE, tr, "open_connection %s:%d\n", IRRd_HOST, IRRd_PORT);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family= AF_INET;
  servaddr.sin_port = htons(IRRd_PORT);

  /* don't you love how gethostbyname is too stupid to accept numeric
     addresses? grrrr. */

  addr = inet_addr(IRRd_HOST);
  if (addr == INADDR_NONE) {
    if ((hoste = gethostbyname(IRRd_HOST)) == NULL) {
      trace (NORM, tr, 
	     "open_connection () ERROR resolving %s: (%s)\n", 
	     IRRd_HOST, strerror (errno));
      return -1;
    }
    memcpy(&addr, hoste->h_addr_list[0], sizeof(struct in_addr));
  }

  servaddr.sin_addr.s_addr = addr;

  if (-1 == connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) {
    trace (NORM, tr, "ERROR opening IRRd TCP connection \n");
    return -1;
  }

  return sockfd;
}

/* close connection */
int close_connection (int fd) {
  close (fd);
  return 1;
}

char *send_socket_cmd (trace_t *tr, int fd, char *cmd) {
  fd_set write_fds;
  struct timeval  tv;

  if (fd < 0)
    return "\"System error: send_socket_cmd () broken IRRd pipe.\"\n";

  /*  fprintf (stderr, "send_socket_cmd () cmd len (%d)\n", strlen(cmd)); */
  FD_ZERO(&write_fds);
  FD_SET(fd, &write_fds);
  tv.tv_sec = 3;
  tv.tv_usec = 0;

  if (select (fd + 1, 0, &write_fds, 0, &tv) < 1) {
    trace (ERROR, tr, "send_socket_cmd () write time out!\n");
    return "\"System error: IRRd write timeout.  IRRd down or connectivity problems.\"\n";
  }


  if (write (fd, cmd, strlen (cmd)) < 0) {
    trace (ERROR, tr, "send_socket_cmd () write error (%s)\n", strerror(errno));
    return "\"System error: IRRd socket write error.\"\n";
  }
 
  return NULL;
}

/*
 * Read something back from IRRd.  If you don't care
 * about the return code from IRRd set response = NULL.
 * Otherwise, set *response to what you are expecting
 * from IRRd (eg, "C\n").  If the response does not match,
 * the function will return the entire IRRd response line.
 *
 * Return codes:
 *  NULL means *response matched IRRd's response
 *  otherwise return IRRd response.
 */
char *read_socket_cmd (trace_t *tr, int fd, char *response) {
  char buf[MAXLINE];
  int n;
  fd_set read_fds;
  struct timeval  tv;

  if (fd < 0)
    return "\"System error: read_socket_cmd () broken IRRd pipe.\"\n";

  FD_ZERO (&read_fds);
  FD_SET  (fd, &read_fds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  if (select (fd + 1, 0, &read_fds, 0, &tv) < 1) {
    trace (ERROR, tr, "read_socket_cmd () read time out!\n");
    return "Read timeout.  Remote host is down or connectivity problems.";
  }

  n = read (fd, buf, MAXLINE - 1);
  if (response == NULL)
    return NULL;

  if (n <= 0) {
    trace (ERROR, tr, "read_socket_cmd () read error (%s)!\n", strerror (errno));
    return "\"System error: Read IRRd socket error.\"\n";
  }
  buf[n] = '\0';

  if (strncmp (buf, response, strlen (response)))
    return strdup (buf);

  return NULL;
}


/*
 * Read a single line terminated by a '\n'.  'bufsize' should be >= 2
 * or else function will return an error.  The function pads the line
 * with a '\0' null byte string terminator.
 *
 * Return:
 *   number of characters read.
 */
int read_socket_line (trace_t *tr, int sockfd, char buf[], int bufsize) {
  int n = 0, x = 0, i;
  fd_set read_fds;
  struct timeval  tv;

  if (sockfd < 0) {
    trace (ERROR, tr, "System error: read_socket_line () broken IRRd pipe.\n");
    return -1;
  }

  FD_ZERO(&read_fds);
  FD_SET(sockfd, &read_fds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  for (i = bufsize - 1; i > 0; i--) {
    if (select (sockfd + 1, 0, &read_fds, 0, &tv) < 1) {
      trace (ERROR, tr, "read_socket_line () read time out!\n");
      return -1;
    }
    if ((x = read (sockfd, &buf[n], 1)) <= 0 ||
	buf[n++] == '\n')
      break;
  }

  buf[n] = '\0';

  if (x <= 0) {
    trace (ERROR, tr, 
	   "read_socket_line () read (%d) bytes and did not get a '\\n', buffer size (%d)\n", n, bufsize);
    return -1;
  }

  /*
  fprintf (dfile, "read_socket_line () irrd:(%s)\n", buf);
  */

  return n;
}

/* JW: need to check this routine for bug: if an object submission
 * exceeds the 'max_line_size' then SNIP_MSG could be inserted into
 * the object and written to the DB.  Not sure if this is can happen or not.
 */
/* Read a DB object into file 'fobj'.  We have already read the 'A...'
 * answer length specifier, 'len', and are now reading the object data.
 * 'obj_size' can be either 'FULL_OBJECT' or 'SHORT_OBJECT'.  'SHORT_OBJECT'
 * will retrieve select fields only to minimize the amount of processing
 * and disk space needed.  For example, irr_auth only needs the maintainer
 * references, auth fields, etc...  Some aut-num objects can be as large
 * as 25Mb so a definite savings is possible.
 *
 * Return:
 *   -1 means the 'A...'/'len' specifier did not match or a socket
 *      error occured.
 *    1 object was successfully read and saved to file.
 */
int read_socket_obj (trace_t *tr, int sockfd, FILE *fobj, int obj_size, 
		     int len, int max_line_size) {
  char buf[MAXLINE];
  int n, print_snip = 1;
  int dump_line = 0, line_cont = 0, line_begin;

  while (len > 0) {
    if ((n = read_socket_line (tr, sockfd, buf, MAXLINE)) <= 0)
      return -1;
    
    len -= n;

    if ((line_begin = !line_cont))
      dump_line = 0;
    line_cont = (buf[n - 1] != '\n');

    if (dump_line ||
	(line_begin && ((--max_line_size < 0 || obj_size == SHORT_OBJECT) &&
	!(is_upd_to (buf)  || 
	  is_mnt_nfy (buf) ||
	  is_notify (buf)  ||
	  is_auth (buf)    ||
	  is_mnt_by (buf)  ||
	  is_changed (buf) ||
	  is_source (buf))))) {
      if (print_snip && max_line_size < 0) {
	fwrite (SNIP_MSG, 1, strlen (SNIP_MSG), fobj);
	print_snip = 0;
      }
      dump_line = 1;
      continue;
    }

    if (fwrite (buf, 1, n, fobj) < n) {
      trace (ERROR, tr, "read_socket_obj () write file error: (%s)\n", strerror (errno));
      return -1;
    }
  }

  /* read the IRRd 'C\n' return code */
  read_socket_line (tr, sockfd, buf, MAXLINE);

  return 1;
}

/* send '!!' */
char *start_irrd_session (trace_t *tr, int sockfd) {

  return send_socket_cmd (tr, sockfd, "!!\n"); 
}

/* send '!q' */
char *end_irrd_session (trace_t *tr, int sockfd) {

  return send_socket_cmd (tr, sockfd, "!q\n");
}

/* Send a 
 * !us<DB>
 * <OPERATION>
 *
 *...<object>...
 *
 * !ue
 *
 * Return:
 *   Char return line from IRRd.
 *   Char error line from this routine if
 *   a system or network error occurs.
 *   NULL if routine encounters something
 *   other than ADD or DEL (NOOP?).  NULL
 *   means nothing was sent.
 */
char *send_transaction (trace_t *tr, char *warn_tag, int fd, char *op, 
			char *source, FILE *fin) {
  char buf[MAXLINE], *ret_code;
  int line_cont, line_begin, n;

  sprintf (buf, "!us%s\n", source);
  if (!strcmp (op, ADD_OP) || !strcmp (op, REPLACE_OP)) {
    /*fprintf (tr, "!us%s\n", source );*/
    if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
      return ret_code;
    if ((ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL)
      return ret_code;
    if ((ret_code = send_socket_cmd (tr, fd, "ADD\n")) != NULL ||
	(ret_code = send_socket_cmd (tr, fd, "\n")) != NULL)
      return ret_code;
  }    
  else if (!strcmp (op, DEL_OP)) {
    /*fprintf (dfile, "!us%s\nDEL\n\n", source);*/
    if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
      return ret_code;
    if ((ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL)
      return ret_code;
    if ((ret_code = send_socket_cmd (tr, fd, "DEL\n")) != NULL ||
	(ret_code = send_socket_cmd (tr, fd, "\n")) != NULL)
      return ret_code;
  }
  else /* Must be a NOOP, don't do anything.
	* We should never get here, calling routine
	* should recognize NOOP's and skip.
	*/
    return NULL;

  line_cont = 0; /* line cont in the fgets () sense, not in the rpsl sense
		  * ie, the current input line was larger than our memory 
		  * buffer
		  */
  while (fgets (buf, MAXLINE - 1, fin) != NULL) {
    n = strlen (buf);
    line_begin = !line_cont;
    line_cont = (buf[n - 1] != '\n');
    if (line_begin && !strncmp (buf, warn_tag, strlen (warn_tag)))
      continue;

    if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
      return ret_code;

    if (line_begin && buf[0] == '\n') /* looking for a blank line */
      break;
  }
  
  /*fprintf (dfile, "!ue\n");
  printf ("send !ue...\n");
  */
  if ((ret_code = send_socket_cmd (tr, fd, "!ue\n")) != NULL)
    return ret_code;

  if ((ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL) {
    /*
printf ("send_trans (): exit bad return code (%s)", ret_code);
    */
    return ret_code;
  }
/*
printf ("send_trans: exit good return code (%s)", "C\n");  
*/
  return "C\n";
}

/* Send an IRRd transaction (ie, !us...!ue).
 * Function expects file position pointer to
 * be at the start of the object.
 *
 * Return:
 *
 *   Char *response/return code from IRRd
 *   Char *line from this routine if the IRRd
 *   socket could not be opened succesfully.
 */
char *irrd_transaction (trace_t *tr, char *warn_tag, int *socketfd, FILE *fin, 
			char *op, char *source, int num_trans, int *open_conn,
			char *IRRd_HOST, int IRRd_PORT) {
  char *ret_code;
  
  if (num_trans == 1) {
    
    /* Open connection */
    if ((*socketfd = open_connection (tr, IRRd_HOST, IRRd_PORT)) < 0)
      return "\"System error: Open socket () error!\"\n";
    
    *open_conn = 1;
    
    if ((ret_code = start_irrd_session (tr, *socketfd)) != NULL)
      return ret_code;
  }
  
  return send_transaction (tr, warn_tag, *socketfd, op, source, fin);
}

/* Send a single object from the irr_submit pipeline file to IRRd.
 * Object is canonicalized and ready for DB inclusion.
 *
 * Need to add comments.
 */
char *send_object (trace_t *tr, FILE *fin, int fd, char *warn_tag) {
  char buf[MAXLINE], *ret_code;
  int line_cont, line_begin, n;

  line_cont = 0; /* line cont in the fgets () sense, not in the rpsl sense
		  * ie, the current input line was larger than our memory 
		  * buffer
		  */
  while (fgets (buf, MAXLINE - 1, fin) != NULL) {
    n = strlen (buf);
    line_begin = !line_cont;
    line_cont = (buf[n - 1] != '\n');
    if (line_begin && !strncmp (buf, warn_tag, strlen (warn_tag)))
      continue;
    
    if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
      return ret_code;
    
    if (line_begin && buf[0] == '\n') /* looking for a blank line */
      break;
  }

  return NULL;
}

/* Send RPS-DIST a transaction.  The format of the message sent
* to RPS-DIST is as follows:
*
* !us<db source>
* <!us...!ue file name for IRRd>
* <pgp updates file name of "NULL">
* <journal entry file name>
* !ue
*
* RPS-DIST will responsd with current serial value of the current
* transaction.  The "cs" value can be used to query IRRd should
* we timeout/RPS-DIST become unreachable.
*
* This routine expects one more message from RPS-DIST which is
* the return code from IRRd, ie, a "C" to signify the transaction
* was committed or a 'F...' which means the transaction was not
* committed and a text message is given.
*
* Input:
*   -the IRRd !us...!ue transaction file name (irrdfn)
*   -the pgp updates file name (pgpfn)
*   -the journal entry file name (jentryfn)
*   -the RPS-DIST host and port (RPSDIST_HOST,RPSDIST_PORT)
*
* Return:
*   -"C\n" which means the transaction was committed without error
*   -"current serial" which means we lost contact or timed out with
*    RPS-DIST and irr_submit can use this value to contact IRRd to find
*    out the transaction outcome
*   -otherwise a text message is returned that explains an error 
*    note that a text message does not indicate the transaction
*    aborted.  For example we could timeout yet the transaction
*    could end up committing.
*/
char *rpsdist_transaction (trace_t *tr, char *irrdfn, char *pgpfn, 
			   char *jentryfn, char *source, char *RPSDIST_HOST, 
			   int RPSDIST_PORT) {
  int fd;
  char buf[MAXLINE], cs[25], *ret_code = NULL;
  
  /* Open an IRRd connection */
  if ((fd = open_connection (tr, RPSDIST_HOST, RPSDIST_PORT)) < 0)
    return "\"System error: RPSDIST open socket () error!\"\n";
  
  /* Send the !us<db> */
  sprintf (buf, "!us%s\n", source);
  if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
    goto CLOSE_CONN;
  
  /* Send the irrd !us...!ue file name */
  strcpy (buf, irrdfn);
  strcat (buf, "\n");
  if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
    goto CLOSE_CONN;
  
  /* Send the pgp update file name */
  strcpy (buf, pgpfn);
  strcat (buf, "\n");
  if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
    goto CLOSE_CONN;
  
  /* Send the journal entry file name */
  strcpy (buf, jentryfn);
  strcat (buf, "\n");
  if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
    goto CLOSE_CONN;
  
  /* Send the !ue to terminate the transaction */
  if ((ret_code = send_socket_cmd (tr, fd, "!ue\n")) != NULL)
    goto CLOSE_CONN;
  
  /* Expecting the current serial of the transaction from RPS-DIST */
  ret_code = read_socket_cmd (tr, fd, "^|@");

  /* Sanity check */
  if (ret_code == NULL) {
    trace (ERROR, tr, "rpsdist_trans () NULL response from RPSDIST.  Expecting 'cs' or error message.  Abort transaction!");
    ret_code = strdup ("NULL response from RPSDIST, transaction outcome not known.");
    goto CLOSE_CONN;
  }
  
  if (*ret_code < '0' || *ret_code >'9') {
    trace (ERROR, tr, "rpsdist_trans () Error message from RPS-DIST.  Expected current serial.  Aborting transaction! (%s)", ret_code);
    goto CLOSE_CONN;
  }
  else
    trace (NORM, tr, "rpsdist_trans () current serial (%s)", ret_code);
  
  /* save the current serial */
  strcpy (cs, ret_code);
  
  /* Expecting a "C\n" return code to signify IRRd committed the trans */
  if ((ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL) {
    trace (ERROR, tr, "rpsdist_trans () Error message from RPS-DIST.  Expected 'C'. (%s)", ret_code);
    
    /* Something went wrong, irr_submit should contact IRRd to find out the
     * transaction outcome.  irr_submit will use the current serial
     * to query IRRd */
    ret_code = strdup (cs);
  }
  else
    ret_code = "C\n";

CLOSE_CONN:
  close_connection (fd);
  
  trace (NORM, tr, "exit rps_dist_trans ()\n");
  
  return ret_code;
}

char *irrd_transaction_new (trace_t *tr, char *warn_tag, FILE *fin, 
			    ret_info_t *start, char *IRRd_HOST, int IRRd_PORT) {
  int fd;
  irrd_result_t *p;
  char buf[MAXLINE], *ret_code = NULL;
  
  /* Open an IRRd connection */
  if ((fd = open_connection (tr, IRRd_HOST, IRRd_PORT)) < 0)
    return "\"System error: Open socket () error!\"\n";

  /* Stay open mode !! */
  if ((ret_code = send_socket_cmd (tr, fd, "!!\n")) != NULL)
    goto CLOSE_CONN;

  trace (NORM, tr, "send !us\n");

  /* Send the !us<db> */
  sprintf (buf, "!us%s\n", start->first->source);
  if ((ret_code = send_socket_cmd (tr, fd, buf))   != NULL ||
      (ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL)
    goto CLOSE_CONN;

  trace (NORM, tr, "send first object\n");
   
  /* Send the objects */
  for (p = start->first; p != NULL; p = p->next) {
    
    trace (NORM, tr, "top of loop\n");

    /* Skip if we have a NOOP object */
    if (p->svr_res & NOOP_RESULT) {
      trace (NORM, tr, "NOOP submission.  Object not added to IRRd.\n");
      continue;
    }

    /* seek to the beginning of object */
    fseek (fin, p->offset, SEEK_SET);

    trace (NORM, tr, "send operator\n");

    /* Send the operator */
    if (!strcmp (p->op, DEL_OP)) {
      if ((ret_code = send_socket_cmd (tr, fd, "DEL\n\n")) != NULL)
	goto CLOSE_CONN;
    }
    else if ((ret_code = send_socket_cmd (tr, fd, "ADD\n\n")) != NULL)
      goto CLOSE_CONN;
	
    trace (NORM, tr, "send object\n");

    /* Send the object */
    send_object (tr, fin, fd, warn_tag);

    trace (NORM, tr, "end object\n");
  }

  trace (NORM, tr, "send !ue\n");

  /* Send the !ue to terminate the transaction */
  if ((ret_code = send_socket_cmd (tr, fd, "!ue\n")) != NULL ||
      (ret_code = read_socket_cmd (tr, fd, "C\n"))   != NULL)
    goto CLOSE_CONN;

  trace (NORM, tr, "send !q\n");

  /* send '!q' */
  send_socket_cmd (tr, fd, "!q\n");

CLOSE_CONN:
  fflush (fin); /* JW only needed when irrd commands are sent to terminal */
  close_connection (fd);

  trace (NORM, tr, "exit irrd_send_trans ()\n");

  return ret_code;
}

/* Fetch a DB object from IRRd and put it into file '*fname'.  'obj_size'
 * can be either 'SHORT_OBJECT' in which case only notify, mnt_nfy, upd_to,
 * and auth fields are fetched, or 'FULL_OBJECT'.
 *
 * Return:
 *  Stream file pointer to the object if it was received successfully from IRRd
 *  and the name of the file in 'fname'.
 *  NULL otherwise.
 */
FILE *IRRd_fetch_obj (trace_t *tr, char *fname, int *conn_open, int *sockfd,
		      int obj_size, char *obj_type, char *obj_key, char *source,
		      int max_obj_line_size, char *IRRd_HOST, int IRRd_PORT) {
  char buf[MAXLINE];
  FILE *fobj;
  int fd;
  
  /* open the output file */
  fd = mkstemp (fname);
  if ((fobj = fdopen (fd, "w+")) == NULL) {
    trace (ERROR, tr, "IRRd_fetch_obj() can't open \"%s\": (%s)\n", 
	   fname, strerror (errno));
    return NULL;
  }
  
  /* Open an IRRd connection */
  if (++*conn_open == 1) {
    if ((*sockfd = open_connection (tr, IRRd_HOST, IRRd_PORT)) < 0) {
      fclose (fobj);
      remove (fname);
      *conn_open = 0;
      return NULL;
    }

    /* Send a '!!' */
    if (start_irrd_session (tr, *sockfd) != NULL) {
      trace (ERROR, tr, "IRRd_fetch_obj() '!!' failed\n");
      goto fetch_fail;
    }
    
    /* make sure we can get withdrawn routes too */
    if (send_socket_cmd (tr, *sockfd, "!uwd=1\n") != NULL ||
	read_socket_cmd (tr, *sockfd, "C\n") != NULL) {
      trace (ERROR, tr, "IRRd_fetch_obj() '!uwd=1' failed\n");
      goto fetch_fail;
    }
  }
  
  /* send the IRRd commands to retrieve the object */
  sprintf (buf, "!s%s\n", source);
  if (send_socket_cmd (tr, *sockfd, buf)   != NULL ||
      read_socket_cmd (tr, *sockfd, "C\n") != NULL) {
    trace (ERROR, tr, "IRRd_fetch_obj() '!s' failed\n");
    goto fetch_fail;
  }
    
  sprintf (buf, "!m%s,%s\n", obj_type, obj_key);
  if (send_socket_cmd (tr, *sockfd, buf)           != NULL ||
      read_socket_line (tr, *sockfd, buf, MAXLINE) < 0) {
    trace (ERROR, tr, "IRRd_fetch_obj() '!m' failed\n");
    goto fetch_fail;
  }
  
  /* An 'A' return code means we have the object, otherwise 
   * the object was not found.
   */
  /*fprintf (dfile, "IRRd_fetch_object () irrd ret_code (%s)\n", buf);*/
  if (buf[0] != 'A') {
    fclose (fobj);
    remove (fname);
    return NULL;
  }
  
  /* put the object into file */
  newline_remove (buf);
  if (read_socket_obj (tr, *sockfd, fobj, obj_size, atoi (&buf[1]), max_obj_line_size) < 0) {
    trace (ERROR, tr, "IRRd_fetch_obj() read_socket_obj failed\n");
fetch_fail:
    fclose (fobj);
    remove (fname);
    close(*sockfd);
    *sockfd = -1;
    *conn_open = 0;
    return NULL;
  }
  
  /* Simplify NOOP checking, put a blank line after the object */
  if (EOF == fputs ("\n", fobj))
    trace (ERROR, tr, "IRRd_fetch_obj () can't write \\n at object end: (%s)\n", strerror (errno)); 

  return fobj;
}

/* Take a !us...!ue file from irr_submit and send it to IRRd.
 *
 * Input:
 *   an input !us...!ue file (fin)
 *     Note: file should not have leading or trailing blank lines
 *   the irrd host (IRRd_HOST)
 *   the irrd port (IRRd_PORT)
 *
 * Return:
 *   -"C\n" if the file was committed by irrd
 *   -a text file error message otherwise.  This would
 *    mean irrd did not commit the transation.
 */
char *send_rpsdist_trans (trace_t *tr, FILE *fin, char *IRRd_HOST, 
			  int IRRd_PORT) {
  int fd;
  char buf[MAXLINE], *ret_code = NULL;

  /* make sure were at the beginning of file */
  fseek (fin, 0L, SEEK_SET);

  /* Open an IRRd connection */
  if ((fd = open_connection (tr, IRRd_HOST, IRRd_PORT)) < 0) {
    trace (ERROR, tr, "send_rpsdist_trans () Could not open connection to IRRd (%s, %d)\n", IRRd_HOST, IRRd_PORT);
    return "send_rpsdist_trans () IRRd unreachable or down. Tranaction aborted!\n";
  }

  /* Stay open mode !! */
  if ((ret_code = send_socket_cmd (tr, fd, "!!\n")) != NULL)
    goto CLOSE_CONN;

  /* JW:!!!: needs to be fixed to handle lines longer than MAXLINE */

  /* loop through the !us...!ue irrd update file */
  while (fgets (buf, MAXLINE - 1, fin) != NULL) {
    if ((ret_code = send_socket_cmd (tr, fd, buf)) != NULL)
      goto CLOSE_CONN; 
    
    /* for !us or !ue expect a "C\n" return code from irrd */
    if ((!strncmp (buf, "!us", 3) ||
        !strncmp (buf, "!ms", 3) ||
	!strcmp (buf, "!ue\n")) &&
	(ret_code = read_socket_cmd (tr, fd, "C\n")) != NULL)
      goto CLOSE_CONN;
  }

  /* send '!q' */
  send_socket_cmd (tr, fd, "!q\n");

CLOSE_CONN:
  close_connection (fd);

  return ret_code;
}

/* Perform the PGP operations from irr_submit on the local rings.
 *
 * Input:
 *   -pointer to the file of PGP operations to be performed (fin)
 *   -fully qualified dir path to the location of the production rings (pgpdir)
 *
 * Return:
 *   -NULL if there were no errors and the operations were carried
 *    out successfully
 *   -a character text message of an error condition, ie, the PGP
 *    operations were not carried to completion
 */
char *rpsdist_update_pgp_ring (trace_t *tr, FILE *fin, char *pgpdir) {
  char curline[MAXLINE], *ret_code = NULL;
  regex_t blanklinere;
  char *blankline = "^[ \t]*\n$";
  char *newline;
  int add;

  /* compile the regex */
  regcomp (&blanklinere, blankline, REG_EXTENDED|REG_NOSUB);

  /* rewind */
  fseek (fin, 0L, SEEK_SET);

  /* loop through the pgp update file and perform the updates */
  while (fgets (curline, MAXLINE - 1, fin) != NULL) {
    /* skip blank lines */
    if (regexec (&blanklinere, curline, 0, 0, 0) == 0)
      continue;

    /* read the op */
    add = !strncmp(curline, ADD_OP, 3);
   
    /* read the operation data item */
    if (fgets (curline, MAXLINE - 1, fin) == NULL) {
      trace (ERROR, tr, "rpsdist_update_pgp_ring () PGP disk read error.\n");
      ret_code = "SERVER ERROR: PGP disk read error.  Transaction aborted.";
      break; /* we're hosed, break out of the loop */
    }

    /* change the '\n' to EOS */
    if ((newline = strchr (curline, '\n')))
      *newline = '\0';

    /* (ret_code) not NULL means we are aborting the transaction */
    if (ret_code == NULL) {
      if (add) {
	/* perform the add operation */
	if (!pgp_add (tr, pgpdir, curline, NULL)) {
	  trace (ERROR, tr, "rpsdist_update_pgp_ring () pgp add error.\n");
	  ret_code = "SERVER ERROR: pgp add read error.  Transaction aborted.";
	}
      }
      /* perform the delete operation */
      else if (!pgp_del (tr, pgpdir, curline)) {
	trace (ERROR, tr, "rpsdist_update_pgp_ring () PGP delete error.\n");
	ret_code = "SERVER ERROR: PGP delete error.  Transaction aborted.";
      }
    }

    /* clean up the temp files */
    if (add)
      remove (curline);
  }
 
  /* make purify happy */
  regfree (&blanklinere);

  return NULL;
}

char *irrd_curr_serial (trace_t *tr, char *source, char *host, int port) {
  int fd;
  char buf[MAXLINE], cs[MAXLINE];

  /* Open an IRRd connection */
  if ((fd = open_connection (tr, host, port)) < 0) {
    trace (ERROR, tr, "irrd_curr_serial () Could not open connection to IRRd (%s, %d)\n", host, port);
    return NULL;
  }
  
  /* Stay open mode !! */
  sprintf (buf, "!j%s\n", source);
  if (send_socket_cmd (tr, fd, buf) != NULL ||
      read_socket_line (tr, fd, buf, MAXLINE) < 0) {
    trace (ERROR, tr, "irrd_curr_serial () 1. failed !j command\n");
    goto CLOSE_CONN;
  }

  /* Grab the 'A' return code */
  if (buf[0] != 'A') {
    trace (ERROR, tr, "irrd_curr_serial () 2. failed !j command\n");
    goto CLOSE_CONN;
  }

  /* read the data line and the 'C' */
  if (read_socket_line (tr, fd, cs, MAXLINE) < 0 ||
      read_socket_line (tr, fd, buf, MAXLINE) < 0) {
    trace (ERROR, tr, "irrd_curr_serial () 3. failed !j command\n");
    goto CLOSE_CONN;
  }

  close_connection (fd);
  return strdup (cs);

CLOSE_CONN:
  
  close_connection (fd);
  return NULL;
}
