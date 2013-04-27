/* 
 * $Id: connect.c,v 1.3 2000/06/15 16:42:27 jeffhaas Exp $
 */

#include <netinet/in.h>
#include <arpa/inet.h>

#include <mrt.h>

/* utility code to do a non-blocking connecting -- kind of a pain because
 * we can't just catch a signal
 */
int nonblock_connect (trace_t *default_trace, prefix_t *prefix, int port, int sockfd) {
  struct  sockaddr_in	serv_addr;
  fd_set		fdvar_write; 
  struct timeval	tv;
  int			n, optval;
  socklen_t		optlen;

  tv.tv_sec = 3; /* XXX */
  tv.tv_usec = 0;

  memset((char *)&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_addr.s_addr = prefix_tolong (prefix);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  trace (TRACE, default_trace, "NONBLOCK Connect\n");

  if (fcntl (sockfd, F_SETFL, O_NONBLOCK) == -1) {
    trace (NORM, default_trace, "fnctl failed:  %s\n", strerror (errno)); 
    return -1;
  }

  if ((connect(sockfd, (struct sockaddr *)&serv_addr, 
	       sizeof(struct sockaddr_in))) != 0) {
    if (errno != EINPROGRESS) {
      trace (NORM, default_trace, "Nonblocking connect failed to %s (fd=%d):  %s\n",
	     prefix_toa (prefix), sockfd,
	     strerror (errno));
      close (sockfd);
      return -1;
    }
  }

  /* Use select to check for connection completion.
   * This returns that the socket is ready for writing when the
   * the connection attempt is complete, *whether or not the connection
   * succeeded*.  We must then use getsockopt() to see if the connection
   * actually completely successfully, or if an error occurred (most
   * commonly this will probably be ECONNREFUSED).
   */
  FD_ZERO (&fdvar_write);
  FD_SET(sockfd, &fdvar_write);
  n = select (FD_SETSIZE, NULL, &fdvar_write, NULL, &tv);


  /* If select call was interrupted or returned 0, treat the connection as
   * pending and return.  We have to do this since if select is interrupted
   * or times out, it does not modify the descriptor sets, so we have no other
   * way to differentiate between an interrupted select call and a completed
   * connection.
   */
  if ((!n) || ((n < 0) && (errno == EINTR)))
    goto pending_connect;
  else if (n < 0) {
      close(sockfd);
      trace (NORM, default_trace,
	     "** select on connection to %s failed: %s **\n",
	     prefix_toa (prefix), strerror(errno));
      /* call failed routine */
      return -1;
  }

  /* Select returned success; make sure that it is talking about
   * our socket.
   * Since we are only selecting on one socket, this is probably not
   * necessary, but it was already here and I will leave it that way
   * as a sanity check for now...
   */
  if ((n = FD_ISSET (sockfd, &fdvar_write)) == 0)
    /* Our socket is still pending */
    goto pending_connect;
  else {
    /* Connection completed; check its status */
    optval = 0;
    optlen = (socklen_t)sizeof(optval);
    n = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void *) &optval, &optlen);

    /* Solaris has a broken implementation of getsockopt; it should return
     * any error conditions in optval, but if an error exists on the socket
     * (e.g. ECONNREFUSED), it actually doesn't change optval, but instead
     * returns a -1 and sets errno to the error condition.
     * Under BSD it works correctly: getsockopt() returns a 0 and sets
     * optval to the value of the error condition.  We have to check for
     * both cases here:
     */

   if (n < 0) 
        /* On non-Solaris systems, this will return an error in getsockopt
         * as if it were an error on the socket, but that probably won't
         * be a problem.
         */
        optval = errno;

    if (!optval)                /* No errors */
        goto done_connect;

    /* We have an error. */
    if (optval != EINPROGRESS) {
      trace (NORM, default_trace,
	     "- Non-blocking connect failed to %s: %s\n",
	     prefix_toa (prefix), strerror(optval));

      close (sockfd);
      return -1;
    } else
      goto pending_connect;
  }

done_connect:
  return 1;
  
pending_connect:
  /* Our socket is still pending */
  trace (NORM, default_trace, "Nonblocking connection to %s pending (%d)\n",
         prefix_toa (prefix), sockfd);
  
  /* Add to select fds so we that when the connection attempt is finished
   * we will know the status and can try to use it.
   */
  close (sockfd);
  return (-1);
}

