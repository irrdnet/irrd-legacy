/*
 * $Id: user.c,v 1.10 2002/10/17 19:43:22 ljb Exp $
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "mrt.h"
#include "config_file.h"
#include <sys/utsname.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <irrdmem.h>

static int uii_command_help (uii_connection_t * uii, int alone);
static int uii_call_callback_fn (uii_connection_t * uii, uii_command_t * candidate);
static void telnet_option_process (uii_connection_t * uii, char cp);
static char *uii_translate_machine_token_to_human (char *token);

/* globals */
uii_t *UII;

#define WRITE3(uii, a, b, c) { u_char x[3]; \
	x[0] = (a); x[1] = (b); x[2] = (c); \
	if ((uii)->sockfd < 0  || send (uii->sockfd, x, 3, 0) < 3) \
	     uii_destroy_connection (uii);}

#define SAFE_WRITE(uii, a, n) {\
    if ((uii)->sockfd < 0 || send ((uii)->sockfd, a, n, 0) < (n)) \
	uii_destroy_connection (uii);}

/* Okay, signal the waiting uii that data is ready for it */
void 
signal_uii_data_ready (uii_connection_t * uii)
{
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_lock (&uii->mutex_cond_lock);
    pthread_cond_signal (&uii->cond);
    pthread_mutex_unlock (&uii->mutex_cond_lock);
#endif	/* HAVE_LIBPTHREAD */
}

/*
 * The following routines expect some basic terminal capabilities: 0x07 --
 * beep 0x08 -- non destractive backspace vt100-like arrow keys: ^[[A, B, C,
 * and D caution: when exceeding the right end of line will be a problem
 */
static void 
uii_bs (uii_connection_t * uii, int n)
{
    char bs = 0x08;

    if (uii->sockfd < 0)
	return;

    while (n--) {
	if (send (uii->sockfd, &bs, 1, 0) < 1) {
	    uii_destroy_connection (uii);
	    return;
	}
    }
}


/* similar to bs, but erase the previous character */

static void 
uii_erase (uii_connection_t * uii, int n)
{
    int i;
    uii_bs (uii, n);
    if (uii->sockfd < 0)
	return;
    for (i = 0; i < n; i++) {
	if (send (uii->sockfd, " ", 1, 0) < 1) {
	    uii_destroy_connection (uii);
	    return;
	}
    }
    uii_bs (uii, n);
}


static void 
uii_bell (uii_connection_t * uii)
{
    char bell = 0x07;

    if (uii->sockfd < 0)
	return;

    if (send (uii->sockfd, &bell, 1, 0) < 1) {
	uii_destroy_connection (uii);
	return;
    }
}


#ifndef HAVE_LIBPTHREAD
static void
uii_idle_timeout (uii_connection_t * uii)
{
    user_notice (TR_INFO, UII->trace, uii, 
	"Connection closed -- timeout (idle %d secs)\n", UII->timeout);
    uii_destroy_connection (uii);
}
#endif /* HAVE_LIBPTHREAD */


#ifdef HAVE_LIBPTHREAD
static int
uii_input_ready (uii_connection_t *uii)
{
    while (1) {
	struct timeval tv;
	fd_set read_fds;
	int ret;

	tv.tv_sec = UII->timeout;
	tv.tv_usec = 0;
	FD_ZERO (&read_fds);
	FD_SET (uii->sockfd, &read_fds);
	/* This should be here because select() may change the value */
	ret = select (uii->sockfd + 1, &read_fds, 0, 0, &tv);
	if (ret <= 0) {
	    if (ret < 0) {
		if (socket_errno () == EINTR)
            	    continue;
	        trace (TR_ERROR, UII->trace,
	           "select on %d (before read). "
		   "Closing connection (%m)\n", uii->sockfd);
	    }
	    else
    		user_notice (TR_INFO, UII->trace, uii, 
			"Connection fd %d closed -- timeout (idle %d secs)\n", 
			uii->sockfd, UII->timeout);
	    uii_destroy_connection (uii);
	    return (-1);
	}
	if (FD_ISSET (uii->sockfd, &read_fds)) {
	    return (1);
	}
    }
}
#endif	/* HAVE_LIBPTHREAD */

/*
 * _start_uii uii thread on its own
 */
static void 
start_uii (uii_connection_t * uii)
{

    pthread_mutex_lock (&MRT->mutex_lock); /* let creating process finish */
    pthread_mutex_unlock (&MRT->mutex_lock);

    /*
     * sigset_t set;
     * 
     * sigemptyset (&set); sigaddset (&set, SIGALRM); sigaddset (&set, SIGHUP);
     * pthread_sigmask (SIG_BLOCK, &set, NULL);
     */
#ifdef HAVE_LIBPTHREAD
    init_mrt_thread_signals ();
#endif /* HAVE_LIBPTHREAD */

    /*
     * look for SENT IAC SB NAWS 0 133 (133) 0 58 (58) to learn screen size
     */

    WRITE3 (uii, 0xff, 0xfb, 0x01);
    WRITE3 (uii, 0xff, 0xfb, 0x03);
    WRITE3 (uii, 0xff, 0xfe, 0x21);

    /* usa naws -- screen size negotiation 255 253 31 */
    WRITE3 (uii, 0xff, 0xfd, 0x1F);
    /* no lflow */
    WRITE3 (uii, 0xff, 0xfe, 0x22);
    /* don't goahead */
    WRITE3 (uii, 0xff, 0xfe, 0x03);

    /* uii_clear_screen (uii->sockfd); */

    if (uii_send_data (uii, "IRRd version %s\n", MRT->version) < 1)  
	return;

    if (UII->password != NULL) {
	if (uii_send_data (uii, "\nUser Access Verification\n\n") < 0)
	    return;
    } else {
	if (uii_send_data (uii, "\nNo Access Verification\n\n") < 0)
	    return;
    }

    uii->pos = 0;
    uii->end = 0;

#ifndef HAVE_LIBPTHREAD
    Timer_Turn_ON (uii->timer);
#endif /* HAVE_LIBPTHREAD */

    uii_send_data (uii, UII->prompts[uii->state]);
    uii->telnet = 0;

#ifdef HAVE_LIBPTHREAD
    while (1) {
        if (uii_input_ready (uii) < 0)
	    return;
	if (uii_read_command (uii) < 0)
	    return;
    }
#endif	/* HAVE_LIBPTHREAD */
}


/*
 * uii_accept_connection start thread for new connection and start connection
 * timeout timer.
 */
static void
uii_accept_connection (void)
{
    int sockfd;
    socklen_t len;
    int port, family;
    prefix_t *prefix;
#ifndef HAVE_IPV6
    struct sockaddr_in addr;
#else
    union sockaddr_any {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
    }            addr;
    /* struct sockaddr_in6 *sin6; */
#endif	/* HAVE_IPV6 */
    uii_connection_t *uii;
    u_int one = 1;
    char tmp[BUFSIZE];

    len = sizeof (addr);
    memset ((struct sockaddr *) & addr, 0, len);

    if ((sockfd =
	 accept (UII->sockfd, (struct sockaddr *) & addr, &len)) < 0) {
	trace (TR_ERROR, UII->trace, "accept failed (%m)\n");
	select_enable_fd (UII->sockfd);
	return;
    }
    select_enable_fd (UII->sockfd);

    if (setsockopt (sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &one,
		    sizeof (one)) < 0) {
	trace (TR_WARN, UII->trace, "TCP_NODELAY failed (%m)\n");
    }
#ifndef HAVE_IPV6
    if ((family = addr.sin_family) == AF_INET) {
#else
    if ((family = addr.sa.sa_family) == AF_INET) {
#endif	/* HAVE_IPV6 */
	struct sockaddr_in *sin = (struct sockaddr_in *) & addr;
	port = ntohs (sin->sin_port);
	prefix = New_Prefix (AF_INET, &sin->sin_addr, 32);
    }
#ifdef HAVE_IPV6
    else if (family == AF_INET6) {
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) & addr;
	port = ntohs (sin6->sin6_port);
	if (IN6_IS_ADDR_V4MAPPED (&sin6->sin6_addr))
	    prefix = New_Prefix (AF_INET, ((char *) &sin6->sin6_addr) + 12, 32);
	else
	    prefix = New_Prefix (AF_INET6, &sin6->sin6_addr, 128);
    }
#endif	/* HAVE_IPV6 */
    else {
	trace (TR_ERROR, UII->trace, "unknown connection family = %d\n",
	       family);
	close (sockfd);
	return;
    }

    trace (TR_INFO, UII->trace, "accepting connection from %a\n", prefix);

#ifdef notdef
    /* check if password exists -- if not AND state == 0, deny */
    if ((UII->password == NULL) && (!prefix_is_loopback (prefix))) {
	trace (TR_INFO, UII->trace, "DENIED to %a -- "
	       "restricted to loopback (no password specified in config)\n",
	       prefix);
	Deref_Prefix (prefix);
	close (sockfd);
	return;
    }
#endif
    /* check if login enabled -- if not AND not from localhost, deny */
    if (!UII->login_enabled && (!prefix_is_loopback (prefix))) {
	trace (TR_INFO, UII->trace, "DENIED to %a -- "
	       "restricted to loopback (no login enabled in config)\n",
	       prefix);
	Deref_Prefix (prefix);
	close (sockfd);
	return;
    }

    /* Apply access list (if one exists) */
    if (UII->access_list >= 0) {
	if (!apply_access_list (UII->access_list, prefix)) {
	    trace (TR_INFO, UII->trace, "DENIED to %a\n", prefix);
	    Deref_Prefix (prefix);
	    close (sockfd);
	    return;
	}
    }
    uii = irrd_malloc(sizeof(uii_connection_t));
    uii->state = (UII->initial_state >= 0)? UII->initial_state:
		     ((UII->password) ? UII_UNPREV : 
		         ((UII->enable_password) ? UII_NORMAL: UII_ENABLE));
    uii->prev_level = -1;
    uii->previous[++uii->prev_level] = UII_EXIT;
    uii->schedule = New_Schedule ("uii_connection", UII->trace);
    uii->sockfd = sockfd;
    uii->from = prefix;
    uii->sockfd_out = -1;
    uii->mutex_lock_p = NULL;
    uii->ll_tokens = NULL;
    uii->tty_max_lines = 23;
    uii->telnet = 0;
    uii->more = NULL;
    uii->buffer = New_Buffer (0);
    uii->ll_history = LL_Create (LL_DestroyFunction, free, 0);
    uii->control = 0;
    uii->data_ready = 0;
#ifndef HAVE_LIBPTHREAD
    select_add_fd_event ("uii_read_command", uii->sockfd, SELECT_READ, 1,
                         uii->schedule, (event_fn_t) uii_read_command, 1, uii);
    uii->timer = New_Timer2 ("UII timeout timer", UII->timeout, TIMER_ONE_SHOT,
                             uii->schedule, uii_idle_timeout, 1, uii);
#endif /* HAVE_LIBPTHREAD */

#ifdef HAVE_LIBPTHREAD
    pthread_cond_init (&uii->cond, NULL);
    pthread_mutex_init (&uii->mutex_cond_lock, NULL);
#endif	/* HAVE_LIBPTHREAD */

    pthread_mutex_lock (&UII->mutex_lock);
    LL_Add (UII->ll_uii_connections, uii);
    pthread_mutex_unlock (&UII->mutex_lock);

    sprintf (tmp, "UII %s", prefix_toa (prefix));
    if (mrt_thread_create (tmp, uii->schedule, (thread_fn_t) start_uii, uii) == NULL)
	uii_destroy_connection (uii);
}


static int
commands_compare (uii_command_t * a, uii_command_t * b)
{
    return (strcasecmp (a->string, b->string));
}


/* for compatibility */
int
listen_uii (void)
{
    return (listen_uii2 ("mrt"));
}


/*
 * begin listening for connections on a well known port
 */
int
listen_uii2 (char *portname)
{
    struct sockaddr *sa;
    struct servent *service;
    int len, optval = 1;
    int family, port;
    int fd;

    struct sockaddr_in serv_addr;
#ifdef HAVE_IPV6
    struct sockaddr_in6 serv_addr6;
    memset (&serv_addr6, 0, sizeof (serv_addr6));
    serv_addr6.sin6_family = AF_INET6;
    if (UII->prefix && UII->prefix->family == AF_INET6) {
	memcpy (&serv_addr6.sin6_addr, prefix_toaddr6 (UII->prefix), 16);
    }
#endif	/* HAVE_IPV6 */
    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (UII->prefix && UII->prefix->family == AF_INET) {
	memcpy (&serv_addr.sin_addr, prefix_toaddr (UII->prefix), 4);
    }

    if (UII->port >= 0) {
	port = htons (UII->port);
    } else {
	if (portname == NULL) {
	    port = htons (UII_DEFAULT_PORT);
	} else {
	    /* search /etc/services for port */
	    if ((service = getservbyname (portname, NULL)) == NULL) {
		int i = atoi (portname);	/* in case of number */
		port = (i > 0) ? htons (i) : htons (UII_DEFAULT_PORT);
	    } else
		port = service->s_port;
	}
    }

    family = AF_INET;
    serv_addr.sin_port = port;
    sa = (struct sockaddr *) & serv_addr;
    len = sizeof (serv_addr);
#ifdef HAVE_IPV6
    if (UII->prefix == NULL || UII->prefix->family == AF_INET6) {
	family = AF_INET6;
	serv_addr6.sin6_port = port;
	sa = (struct sockaddr *) & serv_addr6;
	len = sizeof (serv_addr6);
    }
#endif	/* HAVE_IPV6 */

    if ((fd = socket (family, SOCK_STREAM, 0)) < 0) {
#ifdef HAVE_IPV6
	if (UII->prefix || family == AF_INET)
	    goto error;
	/* try with AF_INET */
	sa = (struct sockaddr *) & serv_addr;
	len = sizeof (serv_addr);
	if ((fd = socket (family, SOCK_STREAM, 0)) < 0) {
    error:
#endif	/* HAVE_IPV6 */
	    trace (TR_ERROR, UII->trace, "socket (%m)\n");
	    /* it's fatal */
	    return (-1);
#ifdef HAVE_IPV6
	}
#endif	/* HAVE_IPV6 */
    }
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR,
		    (const char *) &optval, sizeof (optval)) < 0) {
	trace (TR_ERROR, UII->trace, "setsockopt SO_REUSEADDR (%m)\n");
    }
#ifdef SO_REUSEPORT
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEPORT,
		    (const char *) &optval, sizeof (optval)) < 0) {
	trace (TR_ERROR, UII->trace, "setsockopt SO_REUSEPORT (%m)\n");
    }
#endif /* SO_REUSEPORT */
#ifdef IPV6_V6ONLY
  /* want to listen for both IPv4 and IPv6 connections on same socket */
  optval = 0;
  if (setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY,
         (const char *) &optval, sizeof (optval)) < 0) {
    trace (TR_ERROR, UII->trace, "Could not clear IPV6_V6ONLY (%s)\n",
                strerror (errno));
  }
#endif
    if (bind (fd, sa, len) < 0) { 
	if (UII->prefix)
	    trace (TR_WARN, UII->trace,
		   "bind to port %d at %a on fd %d (%m)\n", 
		    ntohs (port), UII->prefix, fd);
	else
	    trace (TR_WARN, UII->trace,
		   "bind to port %d on fd %d (%m)\n", ntohs (port), fd);
	/* I should check errno to see this is fatal or not */
	if (UII->timer == NULL) {
	    assert (UII->schedule == NULL);
    	    UII->schedule = New_Schedule ("UII", UII->trace);
    	    mrt_thread_create2 ("UII", UII->schedule, NULL, NULL);
#define UII_OPEN_RETRY 30
    	    UII->timer = New_Timer2 ("UII bind retry timer", 
			     UII_OPEN_RETRY, TIMER_ONE_SHOT,
                             UII->schedule, (void_fn_t) listen_uii2, 1, NULL);
	}
	/* retry timer turn on */
    	Timer_Turn_ON (UII->timer);
	close (fd);
	return (-1);
    }
    if (UII->timer) {
        /* in case time is running */
        Timer_Turn_OFF (UII->timer);
    }

    pthread_mutex_lock (&UII->mutex_lock);
    if (UII->sockfd >= 0)
	select_delete_fdx (UII->sockfd);	/* this calls close() */
    UII->sockfd = fd;
    pthread_mutex_unlock (&UII->mutex_lock);
    listen (fd, 128);

    if (UII->prefix)
	trace (TR_INFO, UII->trace,
	     "listening for connections on port %d at %a (socket %d)\n",
	       ntohs (port), UII->prefix, UII->sockfd);
    else
	trace (TR_INFO, UII->trace,
	       "listening for connections on port %d (socket %d)\n",
	       ntohs (port), UII->sockfd);

    select_add_fd_event ("uii_accept_connection", UII->sockfd, SELECT_READ, 
			 TRUE, NULL, uii_accept_connection, 0);
    return (1);
}

static int
uii_new_level (uii_connection_t * uii, int new_state)
{
    assert (uii->prev_level < UII_MAX_LEVEL - 1);
    uii->previous[++uii->prev_level] = uii->state;
    uii->state = new_state;;
    return (uii->state);
}

static int
uii_quit_level (uii_connection_t * uii)
{
    assert (uii->prev_level >= 0);
    if (uii->state == UII_CONFIG)
	end_config (uii);
    uii->state = uii->previous[uii->prev_level--];
    return (uii->state);
}

static int
uii_abort_exit (uii_connection_t * uii, int status)
{
    uii_send_data (uii, "Are you sure (yes/no)? ");
    if (uii_yes_no (uii)) {
        if (status < 0) {
    	    uii_send_data (uii, "Abort requested\n");
	    mrt_set_force_exit (MRT_FORCE_ABORT);
	}
        else {
    	    uii_send_data (uii, "Exit requested\n");
	    mrt_set_force_exit (MRT_FORCE_EXIT);
	}
    }
    return (0);
}

static int
uii_abort (uii_connection_t * uii)
{
    return (uii_abort_exit (uii, -1));
}

/*
 * uii_read_command
 */
int 
uii_read_command (uii_connection_t * uii)
{
    int n;
    char *cp;
    char linebuf[BUFSIZE];
    char *history;

    if (uii->sockfd < 0)
	return (-1);

#ifndef HAVE_LIBPTHREAD
    Timer_Set_Time (uii->timer, UII->timeout);
#endif	/* HAVE_LIBPTHREAD */

    if (uii->state == UII_PROMPT_MORE) {
	assert (uii->more);
	/* Grab an input to resume displaying */
	uii_send_bulk_data (uii);
	uii_send_data (uii, UII->prompts[uii->state]);
#ifndef HAVE_LIBPTHREAD
	select_enable_fd (uii->sockfd);
#endif	/* HAVE_LIBPTHREAD */
	return (1);
    }
    if ((n = recv (uii->sockfd, linebuf, sizeof (linebuf) - 1, 0)) <= 0) {
	if (n < 0)
	    trace (TR_ERROR, UII->trace, "read socket %d (%m)\n", uii->sockfd);
	uii_destroy_connection (uii);
	return (-1);
    }
    linebuf[n] = '\0';

#if 0
    {
	int i;
	fprintf (stderr, "uii receive: ");
	for (i = 0; i < n; i++)
	    fprintf (stderr, "[%d]", linebuf[i] & 0xff);
	fprintf (stderr, "\n");
    }
#endif

    cp = linebuf;
    while (n--) {
	if ((uii->telnet > 0) || (*(u_char *) cp == 0xff)) {
	    telnet_option_process (uii, *cp);
	    cp++;
	    continue;
	}
	if (uii->control == 1) {
	    if (*cp == '[') {
		uii->control = 2;
		cp++;
	    } else {
		uii->control = 0;
		cp++;
	    }
	} else if (uii->control == 2) {
	    uii->control = 0;

	    /* up \020 */
	    if (uii->state >= UII_NORMAL && *cp == 'A') {
	up:
		history = LL_GetPrev (uii->ll_history, uii->history_p);

	replace:
		if (history) {
		    int len, i, m;

		    if (uii->pos > 0)
			uii_bs (uii, uii->pos);
		    buffer_adjust (uii->buffer, 0);
		    buffer_puts (history, uii->buffer);
		    len = buffer_data_len (uii->buffer);
		    SAFE_WRITE (uii, buffer_data (uii->buffer), len);
		    if ((m = uii->end - len) > 0) {
			for (i = 0; i < m; i++)
			    SAFE_WRITE (uii, " ", 1);
			uii_bs (uii, m);
		    }
		    uii->pos = uii->end = len;
		    uii->history_p = history;
		} else {
		    uii_bell (uii);
		}
	    }
	    /* down \016 */
	    else if (uii->state >= UII_NORMAL && *cp == 'B') {
	down:
		history = LL_GetNext (uii->ll_history, uii->history_p);
		goto replace;
	    }
	    /* left */
	    else if (*cp == 'D') {
	left:
		if (uii->pos <= 0) {
		    uii_bell (uii);
		} else {
		    uii->pos--;
		    uii_bs (uii, 1);
		}
	    }
	    /* right */
	    else if (*cp == 'C') {
	right:
		if (uii->pos >= uii->end) {
		    uii_bell (uii);
		} else {
		    SAFE_WRITE (uii, buffer_data (uii->buffer) + uii->pos, 1);
		    uii->pos++;
		}
	    }
	    cp++;
	}
	/***** end if (uii->control == 2) **************/
	 /* ^B */ 
	else if (*cp == 'B' - '@') {
	    goto left;
	}
	/* ^F */
	else if (*cp == 'F' - '@') {
	    goto right;
	}
	/* ^N */
	else if (uii->state >= UII_NORMAL && *cp == 'N' - '@') {
	    goto down;
	}
	/* ^P */
	else if (uii->state >= UII_NORMAL && *cp == 'P' - '@') {
	    goto up;
	}
	/* ^K */
	else if (*cp == 'K' - '@') {
	    int i, m;

	    if ((m = uii->end - uii->pos) > 0) {
		for (i = 0; i < m; i++)
		    SAFE_WRITE (uii, " ", 1);
		uii_bs (uii, m);
		uii->end = uii->pos;
		buffer_adjust (uii->buffer, uii->end);
	    }
	}
	/* ^U */
	else if (*cp == 'U' - '@') {
	    int i, m;

	    if ((m = uii->end) > 0) {
		uii_bs (uii, uii->pos);
		for (i = 0; i < m; i++)
		    SAFE_WRITE (uii, " ", 1);
		uii_bs (uii, m);
		uii->pos = uii->end = 0;
		buffer_adjust (uii->buffer, 0);
	    }
	}
	/* ^D */
	else if (*cp == 'D' - '@') {

	    if (uii->pos >= uii->end) {
		uii_bell (uii);
	    } else {
		buffer_delete (uii->buffer, uii->pos);
		uii->end--;
		SAFE_WRITE (uii, buffer_data (uii->buffer) + uii->pos,
			    uii->end - uii->pos);
		SAFE_WRITE (uii, " ", 1);
		uii_bs (uii, uii->end - uii->pos + 1);
	    }
	}
	/* Control-z */
	else if (uii->state >= UII_CONFIG && *cp == 'Z' - '@') {
	    buffer_adjust (uii->buffer, 0);
	    uii->pos = 0;
	    uii->end = 0;
	    while (uii_quit_level (uii) >= UII_CONFIG);
	    uii_send_data (uii, "\n");
	    uii_send_data (uii, UII->prompts[uii->state]);
	}
	/* go to beginning of line */
	else if (*cp == 'A' - '@') {
	    uii_bs (uii, uii->pos);
	    uii->pos = 0;
	}
	/* goto end of line */
	else if (*cp == 'E' - '@') {
	    while (uii->pos < uii->end) {
		char *star = "*";
		if (uii->state == UII_UNPREV) {
		    SAFE_WRITE (uii, star, 1);	/* password -- don't echo */
		} else {
		    SAFE_WRITE (uii, buffer_data (uii->buffer) + uii->pos, 1);
		}
		uii->pos++;
	    }
	} else if (*cp == '\b' || *cp == '\177') {

	    /* no more to delete -- send bell */
	    if (uii->pos <= 0) {
		uii_bell (uii);
	    } else {
		uii_bs (uii, 1);
		buffer_delete (uii->buffer, uii->pos - 1);
		uii->pos--;
		uii->end--;
		SAFE_WRITE (uii, buffer_data (uii->buffer),
			    uii->end - uii->pos);
		SAFE_WRITE (uii, " ", 1);
		uii_bs (uii, uii->end - uii->pos + 1);
	    }
	} else if (uii->state >= UII_NORMAL && *cp == '?') {
	    int alone = (uii->pos <= 0 ||
			   buffer_peek (uii->buffer, uii->pos - 1) == ' ');
	    uii_command_help (uii, alone);
	    uii_send_data (uii, UII->prompts[uii->state]);
	    SAFE_WRITE (uii, buffer_data (uii->buffer), uii->pos);
	} else if (*cp == '\t') {
	    uii_command_tab_complete (uii,
				      (uii->pos <= 0 || buffer_peek (uii->buffer, uii->pos - 1) == ' '));
	}
	/* control character (escape followed by 2 bytes */
	else if (*cp == '\033') {
	    uii->control = 1;
	    cp++;
	}
	/* ^C */
	else if (*cp == 'C' - '@') {
	    uii->pos = uii->end = 0;
	    buffer_adjust (uii->buffer, 0);
	    goto xxxx;
	} else if (*cp == '\r') {
	    char *data;

	if (uii->end > 0) {
	    /* remove tailing spaces */
	    data = cp = buffer_data (uii->buffer) + uii->end - 1;
	    while (cp >= data && isspace((int)*cp)) {
		uii->end--;
		cp--;
	    }
	    buffer_adjust (uii->buffer, uii->end);

	    /* remove heading spaces */
	    cp = data;
	    while (*cp && isspace((int)*cp)) {
		cp++;
	    }
	    if (data != cp) {
		uii->end = uii->end - (cp - data);
		memcpy (data, cp, uii->end);
		buffer_adjust (uii->buffer, uii->end);
	    }
	    uii->pos = 0;
	}
    xxxx:
	    uii_send_data (uii, "\n");

	    if (uii->end > 0) {
		if (uii->state >= UII_NORMAL /* not passwd */ ) {
		    history = strdup (buffer_data (uii->buffer));
		    LL_Append (uii->ll_history, history);
		    uii->history_p = NULL;
		}
	    }

	    if (uii->state == UII_INPUT) {
		uii->data_ready = 1;
	    }
	    else if (uii->end > 0) {
	        uii_process_command (uii);
	        if (uii->state == UII_EXIT) {
		    /* user has quit or we've unexpetdly terminated */
		    /* delete uii memory here */
		    uii_destroy_connection (uii);
		    return (-1);
	        }
	        buffer_adjust (uii->buffer, 0);
	    }

	    uii->pos = 0;
	    uii->end = 0;
#ifndef HAVE_LIBPTHREAD
	    select_enable_fd (uii->sockfd);
#endif	/* HAVE_LIBPTHREAD */
	    /* uii_send_data (uii, "[%2d] ", pthread_self ()); */
	    uii_send_data (uii, UII->prompts[uii->state]);
	    return (1);
	} else if (*(u_char *) cp == 0xff) {
	    /* printf ("telnet sequences\n"); */
	    cp += 3;
	    n -= 2;
	} else {
	    char *star = "*";
	    if (*cp >= ' ' && *cp <= '~') {
		if (uii->state == UII_UNPREV)
		     /* no echo -- password state */ {
		    SAFE_WRITE (uii, star, 1);
		    }
		else {
		    SAFE_WRITE (uii, cp, 1);
		}
		if (uii->end > uii->pos) {
		    SAFE_WRITE (uii, buffer_data (uii->buffer) + uii->pos,
				uii->end - uii->pos);
		    uii_bs (uii, uii->end - uii->pos);
		}
		buffer_insert (*cp, uii->buffer, uii->pos);
		uii->pos++;
		uii->end++;
	    }
	    cp++;
	}
    }
#ifndef HAVE_LIBPTHREAD
    select_enable_fd (uii->sockfd);
#endif	/* HAVE_LIBPTHREAD */

    return (1);
}

/* get a line from uii (non-block w/o thread) */
static int
uii_input (uii_connection_t *uii)
{
#ifndef HAVE_LIBPTHREAD
    int save;
#endif	/* HAVE_LIBPTHREAD */
    uii_new_level (uii, UII_INPUT);
    buffer_adjust (uii->buffer, 0);
    uii->pos = 0;
    uii->end = 0;
    uii->data_ready = 0;
#ifdef HAVE_LIBPTHREAD
    for (;;) {
        if (uii_input_ready (uii) < 0)
	    return (-1);
	if (uii_read_command (uii) < 0)
	    return (-1);
	if (uii->data_ready)
	    break;
    }
#else /* HAVE_LIBPTHREAD */
    select_enable_fd (uii->sockfd);
    save = uii->schedule->can_pass;
    /* another uii even has to proceed */
    uii->schedule->can_pass = 1;
    mrt_busy_loop (&uii->data_ready, 0);
    uii->schedule->can_pass = save;
#endif	/* HAVE_LIBPTHREAD */
    uii_quit_level (uii);
    return (buffer_data_len (uii->buffer));
}

int
uii_yes_no (uii_connection_t *uii)
{
    if (uii_input (uii) > 0) {
	if (strncasecmp (buffer_data (uii->buffer), "y", 1) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

/*
 * read command from socket and call appropriate handling function.
 */
int 
uii_process_command (uii_connection_t * uii)
{
    char *line, *token, *bp;
    uii_command_t *candidate = NULL;
    char *word;
    char *start_quote, *end_quote = NULL;
    int ret = -1;

    line = buffer_data (uii->buffer);
    uii->cp = line;
    word = alloca (buffer_data_len (uii->buffer) + 1);
    /* for the worst case */

    /* eat up initial white spaces */
    while ((isspace((int)*uii->cp)) && *uii->cp)
	uii->cp++;

    if (uii->state >= UII_NORMAL) {

      if ((start_quote = strchr (uii->cp, '\"')) != NULL)
	end_quote = strchr (start_quote, '\"');
      
      /* comment in the middle of line */
      if (((start_quote == NULL) || (end_quote == NULL)) &&
	  (strlen (uii->cp) > 1 && uii->cp[0] != '!' && uii->cp[0] != '#')
	  && ((bp = strchr (uii->cp + 1, '!')) ||
	      (bp = strchr (uii->cp + 1, '#')))) {
	*bp++ = '\0';
	uii->comment = bp;
      }
      if ((bp = strchr (uii->cp, '>')) && (uii->state == UII_ENABLE)) {
	/* redirection */
	int flags = O_WRONLY | O_TRUNC | O_CREAT;

	*bp++ = '\0';
	if (*bp == '>') {
	  flags |= O_APPEND;
	  flags &= ~O_TRUNC;
	  bp++;
	}
	if ((token = uii_parse_line2 (&bp, word)) && token[0]) {
	  char *xp;
	  
	  if ((xp = strrchr (token, '/')))
	    xp = xp + 1;
	  else
	    xp = token;
	  
	  if (UII->redirect) {
	    char *name;

	    name = alloca (strlen (UII->redirect) + 1 + strlen (xp) + 1);
	    sprintf (name, "%s/%s", UII->redirect, xp);

	    if ((uii->sockfd_out =
		 open (name, flags, 0666)) < 0) {
	      uii_send_data (uii, "%s: %m\n", name);
	      return (-1);
	    } else {
	      char *tmps;
	      tmps = alloca (strlen (name) + 2 + 2 + 1);
	      if (BIT_TEST (flags, O_APPEND))
		lseek (uii->sockfd_out, 0, SEEK_END);
	      sprintf (tmps, "[%s]\n", name);
	      SAFE_WRITE (uii, tmps, strlen (tmps));
	    }
	  } else {
	    uii_send_data (uii,
			   "No redirection allowed! "
			   "Use redirect in configuration\n");
	    return (-1);
	  }
	}
      }
    }
    bp = uii->cp + strlen (uii->cp) - 1;
    while (bp >= uii->cp && *bp && isspace((int)*bp))
	*bp-- = '\0';

    if (uii->state == UII_UNPREV) {
	ret = uii_check_passwd (uii);
#define UII_MAX_PASSWORD_RETRY 3
	if (uii->retries >= UII_MAX_PASSWORD_RETRY) {
	    uii_send_data (uii, "Bad passwords\n");
	    uii_quit_level (uii);
	    ret = -1;
	}
    }
    /* blank line */
    else if ((token = uii_parse_line2 (&line, word)) == NULL) {
	ret = 1;
    } else {

	/*
	 * find a candidate command -- returns NULL if no command, or
	 * ambiguous
	 */
	candidate = uii_match_command (uii);

	if (candidate) {

	    /* uii->cp += strlen (candidate->string); */
	    trace (TR_TRACE, UII->trace, "command %s\n", candidate->string);
	    uii->negative = 0;
	    if (strncasecmp (candidate->string, "no ", 3) == 0)
		uii->negative++;

	    ret = uii_call_callback_fn (uii, candidate);
	}
    }

    if (uii->sockfd_out >= 0) {
	close (uii->sockfd_out);
	uii->sockfd_out = -1;
    }
    /* default */
    return (ret);
}

/*
 * uii_call_callback_fn The config or user input has matched a command. Rally
 * up the arguments (if any and call the callback function of the selected
 * command -- It is the responsibility of called routine to delete memory of
 * arguments
 */
static int 
uii_call_callback_fn (uii_connection_t * uii, uii_command_t * candidate)
{
    char *ctoken, *utoken, *cp;
    void *args[20];
    int nargs = 0, ret = 0;
    int optional_arg = -1, optional = 0;

    ctoken = LL_GetHead (candidate->ll_tokens);
    utoken = LL_GetHead (uii->ll_tokens);

    while (ctoken != NULL) {
	char *csaved = NULL;
	char *usaved = NULL;
	char *defval = NULL;

	cp = ctoken;

	/* optional argument */
	if (cp[0] == '[') {
	    if (optional_arg < 0) {
	         optional_arg = nargs;
	         nargs++;
	    }
	    if (strchr (cp + 1, '|') == NULL) {
	        cp++;
		if (utoken)
	            optional++;
		else
		    goto skip;
	    }
	    else {
		csaved = ctoken;
		cp = ctoken = strdup (ctoken);
		cp++;
		defval = strrchr (cp, '|');
		*defval++ = '\0';
	        optional++;
		/* check without default */
		if (utoken == NULL || !uii_token_match (cp, utoken)) {
		    usaved = utoken;
		    /* remove trailing ']' */
		    defval [strlen (defval) - 1] = '\0';
		    utoken = defval;
		}

		/* a special keyword "null" for default */
		if (utoken == defval && strcasecmp (utoken, "null") == 0) {
		    args[nargs++] = NULL;
		    goto skip;
		}
		else if (utoken == defval && strcasecmp (utoken, "nil") == 0) {
		    args[nargs++] = (void *)NIL;
		    goto skip;
		}
	    }
	    /* literal optional */
	    if (cp[0] != '%') {
		if (utoken)
	            args[nargs++] = strdup (utoken);
		goto skip;
	    }
	}

	if (*cp == '%' && *(cp + 1) == 'S') {
	    /* everything else -- leave in uii->cp for backword compat */

	    if (utoken == NULL) {
	        args[nargs++] = NULL;
	    }
	    else {

	    /* I don't know which program expects this -- masaki */
	    uii->cp = strstr (buffer_data (uii->buffer), utoken);
	    /* Instead, I need this. I don't want to use strstr() */
	    {
	        buffer_t *buffer = New_Buffer (0);
		buffer_puts (utoken, buffer);
		while ((utoken = LL_GetNext (uii->ll_tokens, utoken)) != NULL) {
		    buffer_puts (" ", buffer);
		    buffer_puts (utoken, buffer);
		}
		args[nargs++] = strdup (buffer_data (buffer));
		Delete_Buffer (buffer);
	    }
	    }
	}

	/* argument of a set */
	else if (cp[0] == '(') {
	    char *save = NULL;
	    char *buf = strdup ((cp +1));
	    /* a user may provide incomplete string to match an item.
	       in such a case, grab one from the definition. */
	    /* skip %?, which has be replaced one user provides */
	    if (cp[1] != '%') {
#ifdef HAVE_STRTOK_R
    	        char *last;
#endif /* HAVE_STRTOK_R */
    	        int okay = 0;
	        char *ptr = buf;
    	        ptr[strlen(ptr)-1] = '\0'; /* get rid of terminating ) */
    	        ptr = strtok_r (ptr, "|", &last);
    	        /* complete utoken with ctoken */
    	        while (ptr != NULL) {
      	            if ((ret = uii_token_match (ptr, utoken))) {
		        if (okay < ret) {
	    	            okay = ret;
			    save = ptr;
		        }
		        break;
      	            }
      	            ptr = strtok_r (NULL, "|", &last);
    	        }
	    }
	    args[nargs++] = (save)? strdup (save): strdup (utoken);
    	    irrd_free(buf);
	}
	else if (*cp == '%') {
	    assert (utoken);
	    if (*(cp + 1) == 'p' || *(cp + 1) == 'a')	/* prefix */
		args[nargs++] = ascii2prefix (AF_INET, utoken);
	    else if (*(cp + 1) == 'P' || *(cp + 1) == 'A')	/* prefix */
		args[nargs++] = ascii2prefix (AF_INET6, utoken);
	    else if (*(cp + 1) == 's' || *(cp + 1) == 'n')	/* string */
		args[nargs++] = strdup (utoken);
	    else if (*(cp + 1) == 'd' || *(cp + 1) == 'D')	/* integer */
		args[nargs++] = (void*) atoi(utoken); 
	    else if (*(cp + 1) == 'm' || *(cp + 1) == 'M') {	/* ipv4 or ipv6 prefix */
		if (strchr (utoken, ':'))
		    args[nargs++] = ascii2prefix (AF_INET6, utoken);
		else
		    args[nargs++] = ascii2prefix (AF_INET, utoken);
	    }
	}
    skip:
	if (csaved) {
	    free (ctoken);
	    ctoken = csaved;
	}
	ctoken = LL_GetNext (candidate->ll_tokens, ctoken);

	if (utoken == defval) {
	    /* usaved may be null */
	    utoken = usaved;
	}
	else if (utoken) {
	    utoken = LL_GetNext (uii->ll_tokens, utoken);
	}
    }

    /*
     * insert the number of optional arguments before any of the optional
     * arguments
     */
    //char optional_str[64];
    //snprintf(optional_str, 64, "%d", optional);
    if (optional_arg >= 0)
	args[optional_arg] = (void*) optional;

    assert (candidate->call_fn);
    assert (nargs <= 10);	/* XXX */
    if (!(candidate->usearg)) {
	if (candidate->schedule) {
	    if (nargs == 0)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				    (void_fn_t) candidate->call_fn, 1, uii);
	    else if (nargs == 1)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 2, uii,
					       args[0]);
	    else if (nargs == 2)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 3, uii,
					       args[0], args[1]);
	    else if (nargs == 3)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 4, uii,
					       args[0], args[1], args[2]);
	    else if (nargs == 4)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 5, uii,
					args[0], args[1], args[2], args[3]);
	    else if (nargs == 5)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4]);
	    else if (nargs == 6)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4], args[5]);
	    else if (nargs == 7)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4], args[5], args[6]);
	    else if (nargs == 8)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4], args[5], args[6], args[7]);
	    else if (nargs == 9)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4], args[5], args[6], args[7],
					 args[8]);
	    else if (nargs == 10)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
				     (void_fn_t) candidate->call_fn, 6, uii,
					 args[0], args[1], args[2], args[3],
					       args[4], args[5], args[6], args[7],
					 args[8], args[9]);
	} else {
	    if (nargs == 0)
		ret = candidate->call_fn (uii);
	    else if (nargs == 1)
		ret = candidate->call_fn (uii, args[0]);
	    else if (nargs == 2)
		ret = candidate->call_fn (uii, args[0], args[1]);
	    else if (nargs == 3)
		ret = candidate->call_fn (uii, args[0], args[1], args[2]);
	    else if (nargs == 4)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3]);
	    else if (nargs == 5)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4]);
	    else if (nargs == 6)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4],
					       args[5]);
	    else if (nargs == 7)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4],
					       args[5], args[6]);
	    else if (nargs == 8)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4],
					       args[5], args[6], args[7]);
	    else if (nargs == 9)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4],
					       args[5], args[6], args[7], args[8]);
	    else if (nargs == 10)
		ret = candidate->call_fn (uii, args[0], args[1], args[2], args[3], args[4],
					       args[5], args[6], args[7], args[8], args[9]);
	}
    } else {
	if (candidate->schedule) {
	    if (nargs == 0)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 1,
					       candidate->arg, uii);
	    else if (nargs == 1)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 2,
					       candidate->arg, uii, args[0]);
	    else if (nargs == 2)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 3,
				     candidate->arg, uii, args[0], args[1]);
	    else if (nargs == 3)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 4,
				      candidate->arg, uii, args[0], args[1],
					       args[2]);
	    else if (nargs == 4)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 5,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3]);
	    else if (nargs == 5)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4]);
	    else if (nargs == 6)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4], args[5]);
	    else if (nargs == 7)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4], args[5],
					  args[6]);
	    else if (nargs == 8)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4], args[5],
					  args[6], args[7]);
	    else if (nargs == 9)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4], args[5],
					  args[6], args[7], args[8]);
	    else if (nargs == 10)
		ret = schedule_event_and_wait ("uii command", candidate->schedule,
					  (void_fn_t) candidate->call_fn, 6,
				      candidate->arg, uii, args[0], args[1],
					       args[2], args[3], args[4], args[5],
					  args[6], args[7], args[8], args[10]);
	} else {
	    if (nargs == 0)
		ret = candidate->call_fn (candidate->arg, uii);
	    else if (nargs == 1)
		ret = candidate->call_fn (candidate->arg, uii, args[0]);
	    else if (nargs == 2)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1]);
	    else if (nargs == 3)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2]);
	    else if (nargs == 4)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3]);
	    else if (nargs == 5)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4]);
	    else if (nargs == 6)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4], args[5]);
	    else if (nargs == 7)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4], args[5], args[6]);
	    else if (nargs == 8)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4], args[5], args[6], args[7]);
	    else if (nargs == 9)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4], args[5], args[6], args[7],
					  args[8]);
	    else if (nargs == 9)
		ret = candidate->call_fn (candidate->arg, uii, args[0], args[1], args[2],
					  args[3], args[4], args[5], args[6], args[7],
					  args[8], args[9]);
	}
    }

    /*
     * if return == 2 then add to the list of modules. We call this function
     * again when we do a show config
     */
    if (ret == 2) {
	/* add to config module list */
    }

    /* in case the function left something */
    /* previously, all commands theiselves called uii_send_bulk_data()
       before their return. To avoid uii from being called from other threads
       like rip (rip commands run under the rip thread), commads leave
       output data in uii->answer then just return without calling
       uii_send_bulk_data(). */
    /* to be compatible, check sockfd to see if being called during file read
       and check to see if answer is filled and not bein under more mode. */
    if (uii->sockfd >= 0 && uii->answer && buffer_data_len (uii->answer) > 0
	    && uii->state != UII_PROMPT_MORE)
        uii_send_bulk_data (uii);
    return (ret);
}

/*
 * uii_add_command Add a command (i.e. "show rip") and accompagnying call
 * function to list maintaned by user interface object
 */
int 
uii_add_command (int state, char *string, uii_call_fn_t call_fn)
{
    uii_add_command2 (state, 0, string, call_fn, "No Explanation");
    return (1);
}

int
uii_add_command_schedule (int state, int flag, char *string,
	    uii_call_fn_t call_fn, char *explanation, schedule_t * schedule)
{
    return uii_add_command_schedule_i (state, flag, string, call_fn, NULL,
				       FALSE, explanation, schedule);
}

/*
 * uii_add_command_schedule_arg as uii_add_command_schedule except arg will
 * be passed back to call_fn
 */
int
uii_add_command_schedule_arg (int state, int flag, char *string,
			uii_call_fn_t call_fn, void *arg, char *explanation,
			      schedule_t * schedule)
{
    return uii_add_command_schedule_i (state, flag, string, call_fn, arg,
				       TRUE, explanation, schedule);
}

/*
 * uii_add_command_schedule_i the i is for "internal". Not part of the public
 * interface.
 */
int
uii_add_command_schedule_i (int state, int flag, char *string,
	    uii_call_fn_t call_fn, void *arg, int usearg, char *explanation,
			    schedule_t * schedule)
{
    uii_command_t *command;
    char *token;

    command = irrd_malloc(sizeof(uii_command_t));
    command->string = string;
    command->call_fn = call_fn;
    command->state = state;
    if (explanation)
	command->explanation = explanation;
    else
	command->explanation = strdup ("No Explanation");
    command->flag = flag;
    command->ll_tokens = LL_Create (LL_DestroyFunction, free, 0);
    command->schedule = schedule;
    command->arg = arg;
    command->usearg = usearg;

    /* tokenize */
    while ((token = uii_parse_line2 (&string, NULL))) {
	LL_Add (command->ll_tokens, token);
    }

    /*
     * we should really iterate through and make sure a command isn't being
     * added twice...
     */
    pthread_mutex_lock (&UII->mutex_lock);
    LL_Add (UII->ll_uii_commands, command);
    pthread_mutex_unlock (&UII->mutex_lock);

    return (1);
}

/*
 * uii_add_command2 add a command to list of accept commands the flag
 * specifies: 0=default  1=don't display  2=display, but only
 * information--don't accept as a command
 */
int
uii_add_command2 (int state, int flag, char *string,
		  uii_call_fn_t call_fn, char *explanation)
{
    return (uii_add_command_schedule (state, flag, string,
				      call_fn, explanation, NULL));
}

/*
 * uii_add_command_arg as uii_add_command2, except arg will be passed back to
 * call_fn
 */
int 
uii_add_command_arg (int state, int flag, char *string,
		     uii_call_fn_t call_fn, void *arg,
		     char *explanation)
{
    return (uii_add_command_schedule_arg (state, flag, string,
					  call_fn, arg, explanation,
					  NULL));
}

/*
 * uii_destroy_connection
 */
int 
uii_destroy_connection (uii_connection_t * uii)
{
    uii_connection_t * u;

    pthread_mutex_lock (&UII->mutex_lock);
    LL_Iterate (UII->ll_uii_connections, u) {
	if (u == uii) {
	    LL_RemoveFn (UII->ll_uii_connections, uii, NULL);
	    break;
	}
    }
    pthread_mutex_unlock (&UII->mutex_lock);
    /* already deleted */
    if (u == NULL)
	return (0);

    /* check if terminal monitor output is going through us */
    /* this should be the first to avoid a call from trace again */
    if (MRT->trace->uii == uii) {
	MRT->trace->uii = NULL;
    }
    assert (uii != NULL);

    trace (TR_INFO, UII->trace, "Closing a connection from %a\n",
	   uii->from);

    /* this is not called from other threads */
    assert (uii->schedule->self == pthread_self ());

    if (uii->sockfd >= 0) {
#ifdef HAVE_LIBPTHREAD
	close (uii->sockfd);
#else
	/*
	 * Craig said that close should be called in mrt_select(), but our
	 * code is not built so. -- masaki
	 */
	/* Craig's: select_delete_fd2 (uii->sockfd); */
	select_delete_fdx (uii->sockfd);	/* this calls close() */
#endif	/* HAVE_LIBPTHREAD */
    }
    if (uii->sockfd_out >= 0) {
	close (uii->sockfd_out);
	uii->sockfd_out = -1;
    }

    /* in case this uii kepps holding config mode lock */
    if (uii->mutex_lock_p)
	pthread_mutex_unlock (uii->mutex_lock_p);

    if (uii->answer)
	Delete_Buffer (uii->answer);

    if (uii->buffer)
	Delete_Buffer (uii->buffer);

    if (uii->ll_history)
	LL_Destroy (uii->ll_history);

#ifndef HAVE_LIBPTHREAD
    if (uii->timer)
	Destroy_Timer (uii->timer);
#endif	/* HAVE_LIBPTHREAD */

    Deref_Prefix (uii->from);
#ifndef HAVE_LIBPTHREAD
   if(uii->schedule->is_running > 0)
     /* if we running as a scheduled event, can't destroy schedule yet */
     delete_schedule (uii->schedule);
   else
     /* it's safe to destroy the schedule */
     destroy_schedule (uii->schedule);
#else
     /* okay to destroy schedule as the thread will exit below */
    destroy_schedule (uii->schedule);
#endif	/* HAVE_LIBPTHREAD */
    if (uii->ll_tokens != NULL)
	LL_Destroy (uii->ll_tokens);
    irrd_free(uii);

    mrt_thread_exit ();
    return (1);
}

/*
 * set_trace
 */
int
set_uii (uii_t * tmp, int first,...)
{
    va_list ap;
    enum UII_ATTR attr;
    int state, len;
    char *strarg;

    if (tmp == NULL)
	return (-1);

    pthread_mutex_lock (&tmp->mutex_lock);
    /* Process the Arguments */
    va_start (ap, first);
    for (attr = (enum UII_ATTR) first; attr;
	 attr = va_arg (ap, enum UII_ATTR)) {
	switch (attr) {
	case UII_PORT:
	    tmp->port = va_arg (ap, int);
	    break;
	case UII_ADDR:
	    if (tmp->prefix)
		Deref_Prefix (tmp->prefix);
	    tmp->prefix = Ref_Prefix (va_arg (ap, prefix_t *));
	    break;
	case UII_PROMPT:
	    state = va_arg (ap, int);
	    strarg = va_arg (ap, char *);
	    if ((state < 0) || state > UII_MAX_STATE) {
		return (-1);
	    }
	    len = strlen (strarg);
	    if (tmp->prompts[state])
		irrd_free(tmp->prompts[state]);

	    if (UII->hostname) {
		char *strbuf;
		char *hostname, *cp;
		hostname = strdup (UII->hostname);
		if ((cp = strchr (hostname, '.')) != NULL)
		    *cp = '\0';
		strbuf = irrd_malloc(sizeof(char) * (strlen (hostname) + 1 + len + 1));
		sprintf (strbuf, "%s %s", hostname, strarg);
		free (hostname);
		tmp->prompts[state] = strbuf;
	    } else {
		tmp->prompts[state] = strdup (strarg);
	    }
	    /*
	     * XXX THIS IS A BUG at the caller Many routines call here like
	     * set_uii (UII, UII_PROMPT, 1, "MRT-1.3A> "); but it should have
	     * been like set_uii (UII, UII_PROMPT, 1, "MRT-1.3A> ", 0); AT
	     * the moment, multiple setting is disabled
	     */
	    goto quit;		/* XXX */
	    break;
	case UII_ACCESS_LIST:
	    tmp->access_list = va_arg (ap, int);
	    goto quit;		/* XXX */
	    break;
	case UII_PASSWORD:
	    if (UII->password)
		irrd_free(UII->password);
	    UII->password = va_arg (ap, char *);
	    if (UII->password)
		UII->password = strdup (UII->password);
#ifdef notdef
	    else {
		/* disable UII connections if password set to none */
		select_disable_fd (UII->sockfd);
	    }
#endif
	    goto quit;		/* XXX */
	    break;
	case UII_ENABLE_PASSWORD:
	    if (UII->enable_password)
		irrd_free(UII->enable_password);
	    UII->enable_password = va_arg (ap, char *);
	    if (UII->enable_password)
		UII->enable_password = strdup (UII->enable_password);
	    goto quit;		/* XXX */
	    break;
	case UII_INITIAL_STATE:
	    UII->initial_state = va_arg (ap, int);
	    goto quit;		/* XXX */
	    break;
	default:
	    break;
	}
    }
quit:
    va_end (ap);
    pthread_mutex_unlock (&tmp->mutex_lock);
    return (1);
}

int 
uii_add_bulk_output (uii_connection_t * uii, char *format,...)
{
    va_list args;

    if (uii->answer == NULL)
	uii->answer = New_Buffer (0);

    va_start (args, format);
    buffer_vprintf (uii->answer, format, args);
    va_end (args);
    return (1);
}

/*
 * uii_send_bulk_data A "more"  -- when we need to send pages of information
 * (i.e. BGP table dumps) out to the uii socket.
 * 
 * When we get around to it, we should figure out how to 1) get termcap string
 * to clear screen 2) and learn how big the screen is (how many lines). I
 * think this comes during the telnet setup negotiations?
 * 
 */
void 
uii_send_bulk_data (uii_connection_t * uii)
{
    char *newline, *cp;
    int len, lineno = 0;
    int only_one_line = 0;

    assert (uii->schedule->self == pthread_self ());

    if (uii->answer == NULL)
	return;

    if (uii->state == UII_PROMPT_MORE) {
	char c;

	assert (uii->more != NULL);
	cp = uii->more;
	uii->more = NULL;
	uii_quit_level (uii);

    do {
	if (recv (uii->sockfd, &c, 1, 0) != 1) {
	    buffer_reset (uii->answer);
	    return;
	}
	/* I don't know why \0 follows after \r */
    } while (c == '\0');
	/* uii_clear_screen (uii->sockfd); */
	uii_erase (uii, strlen (UII->prompts[UII_PROMPT_MORE]));

	/* quit */
	if (c == '\r')
	    only_one_line++;
	else if (c != ' ') {
	    buffer_reset (uii->answer);
	    return;
	}
    } else {
	cp = buffer_data (uii->answer);
    }

    if (cp == NULL) {
	buffer_reset (uii->answer);
	return;
    }

    while ((cp - buffer_data (uii->answer)) <
	   buffer_data_len (uii->answer)) {

	if ((newline = strchr (cp, '\n')) == NULL) {
	    len = strlen (cp);
	    if (uii->sockfd_out >= 0) {
		if (send (uii->sockfd_out, cp, len, 0) != len) {
	    	    uii->sockfd_out = -1;
	            uii_send_data (uii, "write error (%s).\n",
			           strerror (errno));
	            break;
	    }
	    } else if (uii->sockfd >= 0) {
		if (send (uii->sockfd, cp, len, 0) != len) {
		    uii_destroy_connection (uii);
		    break;
		}
	    }
	    break;
	}
	/*
	 * in the following lines, we need to handle "r\n" and "\n" because
	 * of the difference in a way how to code messages
	 */
	len = newline - cp + 1;

	if (uii->sockfd_out >= 0) {
	    if (len >= 2 && cp[len - 2] == '\r') {
		if (send (uii->sockfd_out, cp, len - 2, 0) != len - 2 ||
		    send (uii->sockfd_out, "\n", 1, 0) != 1) {
		    uii->sockfd_out = -1;
		    uii_send_data (uii, "write error (%s).\n",
				   strerror (errno));
		    break;
		}
	    } else {
		if (send (uii->sockfd_out, cp, len, 0) != len) {
		    uii->sockfd_out = -1;
		    uii_send_data (uii, "write error (%s).\n",
				   strerror (errno));
		    break;
		}
	    }
	} else if (uii->sockfd >= 0) {
	    if (len >= 2 && cp[len - 2] == '\r') {
		if (send (uii->sockfd, cp, len, 0) != len) {
		    uii_destroy_connection (uii);
		    break;
		}
	    } else {
		if (send (uii->sockfd, cp, len - 1, 0) != len - 1 ||
		    send (uii->sockfd, "\r\n", 2, 0) != 2) {
		    uii_destroy_connection (uii);
		    break;
		}
	    }
	}
	cp += len;
	lineno++;

	if (uii->sockfd_out >= 0)
	    continue;

	if (uii->sockfd >= 0) {
	    /*
	     * exceeded line limit at bottom of screen wait for user input
	     * and clear screen (what is the character code??)
	     * 
	     */
	    if (only_one_line || lineno >= (uii->tty_max_lines - 1)) {
		/* char ff = 0x0c;    form feed */
		/* SAFE_WRITE (uii, more, strlen (more)); */
		uii_new_level (uii, UII_PROMPT_MORE);
		uii->more = cp;
		return;
	    }
	}
    }
    buffer_reset (uii->answer);
}

int 
uii_check_passwd (uii_connection_t * uii)
{
    char *tmp, *cp;
    char *password = NULL;
    int enable;

    cp = tmp = alloca (strlen (uii->cp) + 1);
    tmp[0] = '\0';
    if (parse_line (uii->cp, "%S", tmp) >= 1) {
	cp = strip_spaces (tmp);
    }
    enable = (uii->prev_level >= 0 &&
	      uii->previous[uii->prev_level] == UII_NORMAL);
    password = (enable) ? UII->enable_password : UII->password;

    if (uii->state == UII_UNPREV) {
	if (password == NULL || strcmp (password, cp) == 0) {

	    /* don't need to save the previous state */
	    if (enable) {
		/* forget the previous state */
		uii->prev_level--;
		uii->state = UII_ENABLE;
	    } else {
		uii->state = (UII->enable_password)? UII_NORMAL: UII_ENABLE;
	    }
	    uii->retries = 0;
	    return (1);
	} else {
	    trace (TR_TRACE, UII->trace, "Bad passwords, retry = %d\n",
		   uii->retries);
	    uii->retries++;
	}
	return (0);
    }
    return (-1);
}

/* strip off the begining and trailing space characters */
char *
strip_spaces (char *tmp)
{
    char *cp = tmp, *pp;
    while (*cp && isspace((int)*cp))
	cp++;
    if (*cp) {
	pp = cp + strlen (cp) - 1;
	while (pp > cp && isspace((int)*pp))
	    pp--;
	pp[1] = '\0';
    }
    return (cp);
}

/*
 * uii_match_command we iterate through tokens for each command user enters
 * "sh ip bgp" show ip bgp show ipv6 bgp
 */
uii_command_t *
uii_match_command (uii_connection_t * uii)
{
    LINKED_LIST *ll_match;
    uii_command_t *command, *best_command;
    int state = uii->state;
    int level = uii->prev_level;
    char *token, *btoken;

    if (uii->ll_tokens != NULL) {
	LL_Destroy (uii->ll_tokens);
    }
    uii->ll_tokens = uii_tokenize (uii->cp, strlen (uii->cp));

again:
    ll_match = find_matching_commands (state, uii->ll_tokens, NULL, 0);
    best_command = LL_GetHead (ll_match);

    /* to see if it's unique */
if (LL_GetCount (ll_match) > 1) {

    LL_Iterate (ll_match, command) {
	int n_token = 0, n_best = 0;
	/* trace (TR_TRACE, UII->trace, "candidate: %s\n", command->string); */

    /* find shortest in the number of mandatry commands */

	LL_Iterate (command->ll_tokens, token) {
	    if (token[0] != '[' && strncmp (token, "%S", 2) != 0)
		n_token++;
	}
	LL_Iterate (best_command->ll_tokens, token) {
	    if (token[0] != '[' && strncmp (token, "%S", 2) != 0)
		n_best++;
	}

	if (n_token < n_best) {
	    best_command = command;
	}
	else if (n_token == n_best) {
	    btoken = LL_GetHead (best_command->ll_tokens);
	    LL_Iterate (command->ll_tokens, token) {
	        if (strchr ("%([", token[0]) == NULL &&
	                strchr ("%([", btoken[0]) == NULL &&
			strlen (token) < strlen (btoken)) {
	            best_command = command;
		    break;
	        }
	        btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    }
	}
    }

#ifdef notdef
    /* now see if smallest is a subset of all other commands */
    LL_Iterate (ll_match, command) {
	if (strncasecmp (command->string, best_command->string,
			 strlen (best_command->string)) != 0) {
	    config_notice (TR_INFO, uii, "Ambiguous Command: %s\n", uii->cp);
	    if (ll_match != NULL)
		LL_Destroy (ll_match);
	    return (NULL);
	}
    }
#endif

    /* check to see if strings portion of the best matches exactly */

	btoken = LL_GetHead (best_command->ll_tokens);
	LL_Iterate (uii->ll_tokens, token) {
	    if (*btoken == '[') {
		if (!uii_token_match (btoken, token)) {
		    btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    	    if (btoken == NULL)
			break;
		}
		/* OK */
	    }
	    else if (*btoken == '%' || *btoken == '(') {
		/* OK */
		if (btoken[0] == '%' && btoken[1] == 'S') {
		    /* don't care */
		    btoken = NULL;
		    token = NULL;
		    break;
		}
	    }
	    else if (strcasecmp (token, btoken) != 0)
		break;
	    btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    if (btoken == NULL) {
	        token = LL_GetNext (uii->ll_tokens, token);
		break;
	    }
	}

    if (btoken == NULL && token != NULL) {
	config_notice (TR_ERROR, uii, 
			"Too many arguments\n");
        if (ll_match != NULL)
	    LL_Destroy (ll_match);
        return (NULL);
    }

    /* in case literal tokens don't match exactly, check to see ambiguity */
    if (btoken != NULL || token != NULL) {

    /*
     * The above is not enough. It doesn't distinguish "show rip routes" and
     * "show ripng routes". 
     *    -> this will be not needed by improvement in find_matching_commands
     */
    LL_Iterate (ll_match, command) {
        btoken = LL_GetHead (best_command->ll_tokens);
        LL_Iterate (command->ll_tokens, token) {
	    /* each best command tokens are substrings of candidates */
	    /* in this case best command can be picked up */
	    /* doesn't take care of %-style here */
	    if (strchr ("%[(", token[0]) != NULL && token[0] == btoken[0]) {
		/* OK */
	    }
            else if (strncasecmp (token, btoken, strlen (btoken)) != 0) {
		config_notice (TR_INFO, uii, "Ambiguous Command: %s\n", 
			       uii->cp);
		LL_Iterate (ll_match, command) {
		    config_notice (TR_INFO, uii, "  %s\n", command->string);
		}
		if (ll_match != NULL)
		    LL_Destroy (ll_match);
		return (NULL);
	    }
	    btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    if (btoken == NULL)
		break;
	}
    }
    }
}
    if (best_command == NULL) {
	if (state > UII_CONFIG &&
	    level >= 0 && uii->previous[level] >= UII_CONFIG) {
	    state = uii->previous[level--];
	    irrd_free(ll_match);
	    goto again;
	}
	config_notice (TR_INFO, uii,
		       "Unrecognized Command: \"%s\"\n",
		       uii->cp, uii->state);
    } else {
	if (uii->prev_level > level) {
	    assert (level >= 0);
	    assert (state >= UII_CONFIG);
	    uii->state = state;
	    uii->prev_level = level;
	    trace (TR_TRACE, UII->trace, "New config state = %d\n", state);
	}
    }

    /* if format args, make sure an argument supplied */
    if (best_command) {
	btoken = LL_GetHead (best_command->ll_tokens);
	LL_Iterate (uii->ll_tokens, token) {
	    if (btoken[0] == '[') {
		if (!uii_token_match (btoken, token)) {
		    btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    	    if (btoken == NULL)
			break;
		}
		/* OK */
	    }
	    else if (btoken[0] == '%' && btoken[1] == 'S') {
		/* don't care */
		btoken = NULL;
		token = NULL;
		break;
	    }
	    else if (btoken[0] == '%') {
		/* OK */
	    }
	    else {
		/* OK */
	    }
	    btoken = LL_GetNext (best_command->ll_tokens, btoken);
	    if (btoken == NULL) {
	        token = LL_GetNext (uii->ll_tokens, token);
		break;
	    }
	}
	if (btoken) {
	    /* so some arguments are missing. check to see they are not
	       %-style that requires an argument */
	    do {
		if ((btoken[0] == '%' && btoken[1] != 'S') ||
		     btoken[0] == '(') {
		    config_notice (TR_ERROR, uii, 
				   "Missing arguments\n");
		    best_command = NULL;
		    break;
		}
	    } while ((btoken = LL_GetNext (best_command->ll_tokens, btoken)) != NULL);
	}
    }
    if (ll_match != NULL)
	LL_Destroy (ll_match);

    return (best_command);
}

int 
uii_show_timers (uii_connection_t * uii)
{
    mtimer_t *timer;
    time_t now;

    pthread_mutex_lock (&TIMER_MASTER->mutex_lock);

    time (&now);
    uii_add_bulk_output (uii, "Timer Master: Interval=(%d), NextFire=(%d)\n",
			 TIMER_MASTER->time_interval,
			 (TIMER_MASTER->time_next_fire - now));


    LL_Iterate (TIMER_MASTER->ll_timers, timer) {
	uii_add_bulk_output (uii, "Timer %s: Interval %d Base %d Exponent %d "
			     "Jitter %d [%d..%d] Flags 0x%x ",
			     timer->name, timer->time_interval,
			     timer->time_interval_base,
			     timer->time_interval_exponent,
			     timer->jitter,
			     timer->time_jitter_low, timer->time_jitter_high,
			     timer->flags);

	if (timer->time_next_fire > 0)
	    uii_add_bulk_output (uii, "Timeleft %d\n",
				 timer->time_next_fire - now);
	else
	    uii_add_bulk_output (uii, "OFF\n");
    }

    pthread_mutex_unlock (&TIMER_MASTER->mutex_lock);
    return (1);
}


int 
uii_show_help (uii_connection_t * uii)
{
    uii_send_data (uii, 
	"\n"
	"Help may be requested at any point in a command by entering\n"
    	"a question mark '?'.  If nothing matches, the help list will\n"
    	"be empty and you must backup until entering a '?' shows the\n"
    	"available options.\n"
    	"Two styles of help are provided:\n"
    	"1. Full help is available when you are ready to enter a\n"
    	"   command argument (e.g. 'show ?') and describes each possible\n"
    	"   argument.\n"
	"2. Partial help is provided when an abbreviated argument is entered\n"
    	"and you want to know what arguments match the input\n"
    	"(e.g. 'show pr?'.)\n"
	"\n");
    return (1);
}

/*
 * uii_command_tab_complete tab command completion
 */
void 
uii_command_tab_complete (uii_connection_t * uii, int alone)
{
    LINKED_LIST *ll_match, *ll_tokens, *ll_last_tokens;
    char *completion, *cp, *utoken;
    int partial = 0;
    char *tail;

    /* I want to use Delete in place of free, but it's a macro. */
    ll_last_tokens = LL_Create (LL_DestroyFunction, free, 0);

    ll_tokens = uii_tokenize (buffer_data (uii->buffer), uii->end);
    ll_match = find_matching_commands (uii->state, ll_tokens, ll_last_tokens,
				       alone);

    completion = NULL;
    tail = LL_GetTail (ll_tokens);

    LL_Iterate (ll_last_tokens, cp) {
	LINKED_LIST *ll = uii_tokenize_choices (cp, strlen (cp));
	char *cp2;
	LL_Iterate (ll, cp2) {
	    /* filter out non-matching items from (..|..|..) */
	    if (!alone && strncasecmp (cp2, tail, strlen (tail)) != 0)
		continue;
	    if (strcmp (cp2, "<cr>") == 0 || cp2[0] == '%')
		continue;
	    else if (completion == NULL) {
		completion = strdup (cp2);
	    } else {
		/* take the common string */
		char *cp3 = completion;
		char *cp4 = cp2;
		while (*cp3 && *cp4 && *cp3 == *cp4) {
		    cp3++;
		    cp4++;
		}
		if (*cp3 != '\0' || *cp4 != '\0')
		    partial++;
		if (*cp3 != '\0')
		    *cp3 = '\0';
	    }
	}
	LL_Destroy (ll);
    }

    /* nothing found */
    if (completion == NULL || *completion == '\0') {
	uii_bell (uii);
	LL_Destroy (ll_tokens);
	LL_Destroy (ll_last_tokens);
	LL_Destroy (ll_match);
	if (completion)
	    irrd_free(completion);
	return;
    }
    if (alone) {
	cp = completion;
    } else {
	utoken = LL_GetTail (ll_tokens);
	cp = completion + (strlen (utoken));
    }
    buffer_adjust (uii->buffer, uii->pos);
    buffer_printf (uii->buffer, (partial) ? "%s" : "%s ", cp);

    uii_send_data (uii, "%s", buffer_data (uii->buffer) + uii->pos);
    uii->pos = buffer_data_len (uii->buffer);
    uii->end = uii->pos;

    if (partial)
	uii_bell (uii);
    /* clean up memory used */
    LL_Destroy (ll_tokens);
    LL_Destroy (ll_last_tokens);
    LL_Destroy (ll_match);
    if (completion)
	irrd_free(completion);
}

/*
 * telnet_option_process
 */
static void 
telnet_option_process (uii_connection_t * uii_connection, char cp)
{

    /* store seqeunce */
    uii_connection->telnet_buf[uii_connection->telnet++] = cp;

    if (uii_connection->telnet == 1)
	return;

    /* DONT */
    if (uii_connection->telnet_buf[1] == 254) {
	if (uii_connection->telnet < 3)
	    return;
	uii_connection->telnet = 0;
	return;
    }
    /* DO */
    if (uii_connection->telnet_buf[1] == 253) {
	if (uii_connection->telnet < 3)
	    return;
	WRITE3 (uii_connection, 255, 251, cp);	/* pff agree to do whatever */
	uii_connection->telnet = 0;
	return;
    }
    /* WONT */
    if (uii_connection->telnet_buf[1] == 252) {
	if (uii_connection->telnet < 3)
	    return;
	uii_connection->telnet = 0;
	return;
    }
    /* WILL */
    if (uii_connection->telnet_buf[1] == 251) {
	if (uii_connection->telnet < 3)
	    return;
	uii_connection->telnet = 0;
	return;
    }
    /* SB -- subnegotiation */
    if (uii_connection->telnet_buf[1] == 250) {
	if (uii_connection->telnet < 9)
	    return;

	/*
	 * AC SB NAWS 0 80 0 64 IAC SE [A window 80 characters wide, 64
	 * characters high]
	 */
	uii_connection->tty_max_lines = uii_connection->telnet_buf[6];
	uii_connection->telnet = 0;

	trace (TR_TRACE, UII->trace, "set tty to %d\n", 
	       uii_connection->tty_max_lines);
	return;
    }
    uii_connection->telnet = 0;
}

/*
 * uii_translate_machine_command_to_human FIX -- we really need to do bounds
 * checking!!
 */
static char *
uii_translate_machine_token_to_human (char *token)
{
    static char tmp[BUFSIZE];
    char *cp;

    cp = tmp;

    if (token[0] != '%') {
	sprintf (cp, "%s ", token);
	cp += strlen (cp);
	return (tmp);
    }
    if (token[1] == 'd')
	sprintf (cp, "<0-99999> ");
    if (token[1] == 'D')
	sprintf (cp, "<1-%d> ", MAX_ALIST - 1);
    if (token[1] == 'p')
	sprintf (cp, "<xx.xx.xx.xx/xx> ");
    if (token[1] == 'P')
	sprintf (cp, "<xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx/xx> ");
    if (token[1] == 'a')
	sprintf (cp, "<xx.xx.xx.xx> ");
    if (token[1] == 'A')
	sprintf (cp, "<xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx> ");
    if (token[1] == 'm')
	sprintf (cp, "<V4 or V6 address/xx> ");
    if (token[1] == 'M')
	sprintf (cp, "<V4 or V6 address> ");
    if (token[1] == 's')
	sprintf (cp, "<string> ");
    if (token[1] == 'n')
	sprintf (cp, "<name> ");
    if (token[1] == 'S')
	sprintf (cp, "<string...> ");

    cp += strlen (cp);

    if (token[2] != '\0') {
	sprintf (cp, token + 2);
	cp += strlen (cp);
    }
    return (tmp);
}

typedef struct _pair_string_t {
    char *a;
    char *b;
}              pair_string_t;

/*
 * uii_command_help ? command expansion -- list commands that match
 */
static int 
uii_command_help (uii_connection_t * uii, int alone)
{
    uii_command_t *command;
    LINKED_LIST *ll_tokens, *ll_last, *ll_match, *ll_output;
    /* uii_command_t *uii_command = NULL; */
    char *cp, *tail;
    int found;
    pair_string_t *pp;

    ll_last = LL_Create (LL_DestroyFunction, free, 0);
    ll_output = LL_Create (LL_DestroyFunction, free, 0);
    ll_tokens = uii_tokenize (buffer_data (uii->buffer), uii->end);

    ll_match = find_matching_commands (uii->state, ll_tokens, ll_last, alone);

    /* make sure it is only added once */
    command = LL_GetHead (ll_match);
    tail = LL_GetTail (ll_tokens);
    LL_Iterate (ll_last, cp) {
	LINKED_LIST *ll;
	char *cp2;

	if (!BIT_TEST (command->flag, COMMAND_NODISPLAY)) {
	    ll = uii_tokenize_choices (cp, strlen (cp));
	    LL_Iterate (ll, cp2) {
		/* pick up matching ones from (..|..|..) */
		if (!alone && strncasecmp (cp2, tail, strlen (tail)) != 0)
		    continue;
		found = 0;
		LL_Iterate (ll_output, pp) {
		    if (strcmp (pp->a, cp2) == 0) {
			pp->b = NULL;
			found = 1;
			break;
		    }
		}
		if (!found) {
		    pp = irrd_malloc(sizeof(pair_string_t));
		    pp->a = strdup (cp2);
		    if (LL_GetCount (command->ll_tokens) <= (LL_GetCount (ll_tokens) + 1))
			pp->b = command->explanation;
		    LL_Add (ll_output, pp);
		}
	    }
	    LL_Destroy (ll);
	}
	command = LL_GetNext (ll_match, command);
    }

    uii_send_data (uii, "\n");
    LL_Iterate (ll_output, pp) {
	uii_send_data (uii, "  %-28s",
		       uii_translate_machine_token_to_human (pp->a));
	if (pp->b)
	    uii_send_data (uii, "  %s\n", pp->b);
	else
	    uii_send_data (uii, "  \n");
	irrd_free(pp->a);
    }

    /* clean up memory used */
    LL_Destroy (ll_tokens);
    LL_Destroy (ll_last);
    LL_Destroy (ll_output);
    LL_Destroy (ll_match);

    return (1);
}

/*
 * show_version Display version number. Usually called by UII
 */
static int 
show_version (uii_connection_t * uii)
{
  struct utsname name;
    
  int uptime = time (NULL) - MRT->start_time;
  uname(&name);


  uii_send_data (uii, "%s\n", MRT->version); 
  uii_send_data (uii, "  %s\n", name.nodename);
  uii_send_data (uii, "  %s %s %s %s\n", name.sysname, name.release, 
		 name.version, name.machine);
  uii_send_data (uii, "  Compiled on %s\n", MRT->date);
  uii_send_data (uii, "  UP for %d.%02d hours\n", uptime / 3600,
			(uptime % 3600) * 100 / 3600);
  return (1);
}

/*
 * show_threads Display thread and schedule info. Usually called by UII
 */
static int 
show_threads (uii_connection_t * uii)
{
    mrt_thread_t *mrt_thread;

#ifndef HAVE_LIBPTHREAD
    uii_add_bulk_output (uii, "*** NO THREAD SUPPORT ***\n");
    uii_add_bulk_output (uii, "(Running in degraded mode)\n");
#endif	/* HAVE_LIBPTHREAD */
    uii_add_bulk_output (uii, "Unix Process ID is %d\n", getpid ());

    uii_add_bulk_output (uii, "[%10s] %-35s %-12s %5s %5s\n", 
		   "Thread ID", "Name", "Status", "Queue", "Peak");

    pthread_mutex_lock (&MRT->mutex_lock);
    LL_Iterate (MRT->ll_threads, mrt_thread) {
	int t;
	char stmp[BUFSIZE];

	if (mrt_thread->schedule == NULL)
	    sprintf (stmp, "Resident");
	else if (mrt_thread->schedule->is_running)
	    sprintf (stmp, "Running");
	else if (mrt_thread->schedule->lastrun <= 0)
	    sprintf (stmp, "Stopped");
	else if ((t = time (NULL) - mrt_thread->schedule->lastrun) < 60)
	    sprintf (stmp, "Idle %dsecs", t);
	else if (t < 3600)
	    sprintf (stmp, "Idle %dmins", t / 60);
	else
	    sprintf (stmp, "Idle %dhurs", t / 3600);

	uii_add_bulk_output (uii, "[%10d] %-35s %-12s %5d %5d\n",
		       mrt_thread->thread, mrt_thread->name,
		       stmp, schedule_count (mrt_thread->schedule),
		 (mrt_thread->schedule) ? mrt_thread->schedule->maxnum : 0);

    }
    pthread_mutex_unlock (&MRT->mutex_lock);
    return (1);
}

static int 
show_sockets (uii_connection_t * uii)
{
    Descriptor_Struct *tmp;

    LL_Iterate (SELECT_MASTER->ll_descriptors, tmp) {
	uii_add_bulk_output (uii, "    [%s] socket %2d mask 0x%x\n",
			     tmp->name ? tmp->name : "noname",
			     tmp->fd,
			     tmp->type_mask);
    }
    return (1);
}

/* terminal monitor -- Ala cisco, divert logging to uii connection */
static int 
terminal_monitor (uii_connection_t * uii)
{
    if (MRT->trace->uii == uii)
	return (0);
    if (MRT->trace->uii != NULL) {
        uii_send_data (uii, "Logging in use\n");
	return (0);
    }
    MRT->trace->uii = uii;
    MRT->trace->uii_fn = uii_send_buffer;
    uii_send_data (uii, "Logging to this vtty connection...\n");
    return (1);
}

/*
 * no_terminal_monitor no terminal monitor
 */
static int 
no_terminal_monitor (uii_connection_t * uii)
{
    if (MRT->trace->uii == uii) {
        MRT->trace->uii = NULL;
        uii_send_data (uii, "Stop logging to this vtty connection.\n");
        return (1);
    }
    return (0);
}

void
init_uii_port (char *portname)
{
    struct servent *service;
    int port;

	if (portname == NULL) {
	    port = UII_DEFAULT_PORT;
	} else {
	    /* search /etc/services for port */
	    if ((service = getservbyname (portname, NULL)) == NULL) {
		int i = atoi (portname);	/* in case of number */
		port = (i > 0) ? i : UII_DEFAULT_PORT;
	    } else
		port = ntohs (service->s_port);
	}
        pthread_mutex_lock (&UII->mutex_lock);
	UII->port = port;
	UII->default_port = port;
        pthread_mutex_unlock (&UII->mutex_lock);
}

/*
 * initialize user interactive interface
 */
void
init_uii (trace_t * ltrace)
{
    struct utsname utsname;
    int i;

    assert (UII == NULL);
    UII = irrd_malloc(sizeof(uii_t));
    UII->ll_uii_commands = LL_Create (LL_CompareFunction, commands_compare,
				      LL_AutoSort, True, 0);
    UII->ll_uii_connections = LL_Create (0);
    UII->trace = trace_copy (ltrace);
    set_trace (UII->trace, TRACE_PREPEND_STRING, "UII", 0);
    UII->sockfd = -1;
    UII->port = -1;
    UII->default_port = -1;
    UII->prefix = NULL;
    UII->timeout = UII_DEFAULT_TIMEOUT;
    UII->access_list = -1;
    UII->login_enabled = 0;
    UII->initial_state = -1;
    if (uname (&utsname) >= 0)
	UII->hostname = strdup (utsname.nodename);

    /* initialize prompts */
    for (i = 0; i < UII_MAX_STATE; i++) {
	if (i == UII_PROMPT_MORE)
	    UII->prompts[i] = strdup (" --More-- ");
	else if (i == UII_INPUT)
	    UII->prompts[i] = strdup ("");
	else
	    UII->prompts[i] = strdup ("IRRd> ");
    }

    /* add some user interface hooks */
    uii_add_command2 (UII_ALL, COMMAND_NORM, "quit",
		      uii_quit_level, "Quit from the current level");
    uii_add_command2 (UII_ALL, COMMAND_NORM, "exit",
		      uii_quit_level, "Quit from the current level");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show threads",
		      show_threads,
		      "Show status of threads");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show version",
		      show_version,
		      "Show version and uptime");
    uii_add_command2 (UII_NORMAL, COMMAND_NODISPLAY, "show sockets",
		      show_sockets,
		      "Show sockets");
    uii_add_command2 (UII_ENABLE, COMMAND_NODISPLAY, "abort", uii_abort,
		      "Abort");
    uii_add_command2 (UII_ENABLE, COMMAND_NODISPLAY, "terminal monitor",
		      terminal_monitor,
		      "Set logging information to UII");
    uii_add_command2 (UII_ENABLE, COMMAND_NODISPLAY, "no terminal monitor",
		      no_terminal_monitor,
		      "Turn off logging information to UII");

    uii_add_command2 (UII_NORMAL, COMMAND_NODISPLAY, "help",
		      uii_show_help,
		      "Description of the interactive help system");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show timers",
		      uii_show_timers,
		      "Show status of timers");
}

void
print_error_list (uii_connection_t * uii, trace_t * tr)
{
    char *s;

    assert (tr);
    if (tr->error_list == NULL)
	return;

    pthread_mutex_lock (&tr->error_list->mutex_lock);
    LL_Iterate (tr->error_list->ll_errors, s) {
	uii_add_bulk_output (uii, "%s", s);
    }
    pthread_mutex_unlock (&tr->error_list->mutex_lock);
}
