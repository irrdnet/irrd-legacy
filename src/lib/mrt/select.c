/*
 * $Id: select.c,v 1.5 2001/07/13 18:02:44 ljb Exp $
 */

#include <mrt.h>

Select_Struct *SELECT_MASTER;

static void
Destroy_Descriptor (Descriptor_Struct *dsp)
{
    assert (dsp);
    if (dsp->name)
		Delete (dsp->name);
    Deref_Event (dsp->event);
    Delete (dsp);
}


/* init_select
*/
int 
init_select (trace_t * tr)
{
    int fd[2];
    ll_value_t lv;
	
    assert (SELECT_MASTER == NULL);
	
    SELECT_MASTER = New (Select_Struct);
    SELECT_MASTER->ll_descriptors = LL_Create (LL_DestroyFunction, 
		Destroy_Descriptor, 0);
    SELECT_MASTER->trace = tr;
    SELECT_MASTER->ll_close_fds = 
		LL_Create (LL_Intrusive, True, 
		LL_PointersOffset, LL_Offset (&lv, &lv.ptrs),
		LL_DestroyFunction, free, 0);
	
    pthread_mutex_init (&SELECT_MASTER->mutex_lock, NULL);
	
    pipe (fd);
    SELECT_MASTER->interrupt_fd[1] = fd[1];
    SELECT_MASTER->interrupt_fd[0] = fd[0];
    FD_SET (fd[0], &SELECT_MASTER->fdvar_read);
    /* setting nonblock is safer */
    socket_set_nonblocking (fd[1], 1);

    return (1);
}


#ifdef HAVE_LIBPTHREAD
static void mrt_select_loop (void);

int
start_select (void)
{
    if (mrt_thread_create ("Select Thread", NULL,
		(thread_fn_t) mrt_select_loop, NULL) == NULL) {
		return (-1);
    }
    return (0);
}
#endif /* HAVE_LIBPTHREAD */


#ifdef notdef
void 
__clear_select_interrupt (void)
{
    char c;
	
    printf ("\nhere in interrupt!\n");
    read (SELECT_MASTER->interrupt_fd[0], &c, 1);
    select_enable_fd (SELECT_MASTER->interrupt_fd[0]);
	
    /*exit (0); */
	
}
#endif


/* 
* Add a file descriptor to the list of fds which we are blocking on in a 
* select call.
*/

static int 
select_add_fd_event_timed_vp (char *name, int fd, u_long type_mask, int on, 
							  time_t timed,
							  schedule_t *schedule, event_fn_t call_fn, event_t *event)
{
    Descriptor_Struct *dsp;
    pthread_mutex_lock (&SELECT_MASTER->mutex_lock);
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		if (dsp->fd != fd)
			continue;
		if (type_mask && (dsp->type_mask & type_mask) == 0)
			continue;
		
		if (dsp->type_mask != type_mask) {
			trace (TR_ERROR, SELECT_MASTER->trace,
				"SELECT: %s on fd %d mask 0x%x -> 0x%x "
				"(mask can't be changed)\n", dsp->name,
				fd, dsp->type_mask, type_mask);
			pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
			return (-1);
		}
		if (name) {
            Delete (dsp->name);
            dsp->name = strdup (name);
		}
		if (dsp->event)
			Deref_Event (dsp->event);
		dsp->event = event; /* no count up */
		dsp->schedule = schedule;
		dsp->timed = timed;
		trace (TR_TRACE, SELECT_MASTER->trace,
			"SELECT: %s on fd %d mask 0x%x %s (update)\n",
			name, fd, type_mask, on? "on": "off");
		break;
    }
	
    if (dsp == NULL) {
        dsp = New (Descriptor_Struct);
        dsp->name = (name)? name: "noname";
        dsp->name = strdup (dsp->name);
        dsp->fd = fd;
        dsp->type_mask = type_mask;
        dsp->event = event; /* no count up */
        dsp->schedule = schedule;
        dsp->timed = timed;
        LL_Add (SELECT_MASTER->ll_descriptors, dsp);
		trace (TR_TRACE, SELECT_MASTER->trace,
			"SELECT: %s on fd %d mask 0x%x %s (update)\n",
			dsp->name, fd, type_mask, on? "on": "off");
    }
	
    if (on) {
        if (type_mask & SELECT_READ)
			FD_SET (fd, &SELECT_MASTER->fdvar_read);
        if (type_mask & SELECT_WRITE)
			FD_SET (fd, &SELECT_MASTER->fdvar_write);
        if (type_mask & SELECT_EXCEPTION)
			FD_SET (fd, &SELECT_MASTER->fdvar_except);
        write (SELECT_MASTER->interrupt_fd[1], "", 1);
    }

    pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
    return (1);
}


int 
select_add_fd_event_timed (char *name, int fd, u_long type_mask, int on, time_t timed,
						   schedule_t *schedule, event_fn_t call_fn, int narg, ...)
{
    va_list ap;
    event_t *event;
    int i;
	
    va_start (ap, narg);
    event = New_Event (narg);
    event->description = strdup (name);
    for (i = 0;i < narg; i++)
        event->args[i] = va_arg (ap, void *);
    event->call_fn = call_fn;
    va_end (ap);
	
    return (select_add_fd_event_timed_vp (name, fd, type_mask, on, timed,
		schedule, call_fn, event));
}


int 
select_add_fd_event (char *name, int fd, u_long type_mask, int on,
					 schedule_t *schedule, event_fn_t call_fn, int narg, ...)
{
    va_list ap;
    event_t *event;
    int i;
	
    va_start (ap, narg);
    event = New_Event (narg);
    if (name == NULL)
		name = "";
    event->description = strdup (name);
    for (i = 0;i < narg; i++)
        event->args[i] = va_arg (ap, void *);
    event->call_fn = call_fn;
    va_end (ap);
	
    return (select_add_fd_event_timed_vp (name, fd, type_mask, on, 0,
		schedule, call_fn, event));
}


int 
select_add_fd (int fd, u_long type_mask, void (*call_fn) (), void *arg)
{
    return (select_add_fd_event ("select_add_fd", fd, type_mask, 1, NULL, call_fn, 
		1, arg));
}


int 
select_delete_fd_mask (int fd, u_long mask, int close_at_select)
{
    Descriptor_Struct *dsp, *prev;
    int found = 0;
	
   pthread_mutex_lock (&SELECT_MASTER->mutex_lock);
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		if (dsp->fd != fd)
			continue;
		if (mask && (dsp->type_mask & mask) == 0)
			continue;
		
		if (dsp->type_mask & SELECT_READ)
			FD_CLR (fd, &SELECT_MASTER->fdvar_read);
		if (dsp->type_mask & SELECT_WRITE)
			FD_CLR (fd, &SELECT_MASTER->fdvar_write);
		if (dsp->type_mask & SELECT_EXCEPTION)
			FD_CLR (fd, &SELECT_MASTER->fdvar_except);
		found++;
        trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: %s cleared fd %d mask 0x%x\n", dsp->name, fd, mask);
		
		if (close_at_select) {
			ll_value_t *vp;
			/* close will be done after select resumes */
			/* if we close here, the same fd may be assigned for open
			that follows imediately.  Then, select may get confused 
			to treat an event on the previous fd for the new one */
			LL_Iterate (SELECT_MASTER->ll_close_fds, vp) {
				if (vp->value == fd)
					break;
			}
			if (vp == NULL) {
				vp = New (ll_value_t);
				vp->value = fd;
				LL_Add (SELECT_MASTER->ll_close_fds, vp);
				trace (TR_TRACE, SELECT_MASTER->trace, 
					"SELECT: %s close fd %d scheduled\n", dsp->name, fd);
			}
		}
		prev = LL_GetPrev (SELECT_MASTER->ll_descriptors, dsp);
		LL_Remove (SELECT_MASTER->ll_descriptors, dsp);
		/* no close */
		dsp = prev;
    }
	
    if (found) {
        write (SELECT_MASTER->interrupt_fd[1], "", 1);
        pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
	return (1);
    }
#ifdef notdef
    /* we agreed not to close in this case */
    else if (close_at_select) {
		/* close fd anyway, this is expected */
        trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: close fd %d unregistered\n", fd);
		close (fd);
    }
#endif
	
    pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
#ifdef MRT_DEBUG
    /* this could happen when called from bgp */
    trace (TR_ERROR, SELECT_MASTER->trace, "SELECT: no fd %d (delete)\n", fd);
#endif /* MRT_DEBUG */
    return (-1);
}


/*
* Delete everything associated with this socket descriptor!
*/
int 
select_delete_fd (int fd)
{
    return (select_delete_fd_mask (fd, 0, 1));
}


/* select_delete_fd2
* without closing fd
* Delete everything associated with this socket descriptor!
*/
int 
select_delete_fd2 (int fd)
{
    return (select_delete_fd_mask (fd, 0, 0));
}


/* wait on select and dispatch an event associated with the fd */
int 
mrt_select2 (int ms)
{
    fd_set fdvar_read, fdvar_write, fdvar_except;
    Descriptor_Struct *dsp;
    struct timeval *timeout, tv;
    LINKED_LIST *ll = NULL;
    int nfds;
    time_t now;
	
/* testing */
    /* process all pending closes */
    if (LL_GetCount (SELECT_MASTER->ll_close_fds) > 0) {
		ll_value_t *vp;
		LL_Iterate (SELECT_MASTER->ll_close_fds, vp) {
			close (vp->value);
            trace (TR_TRACE, SELECT_MASTER->trace, 
				"SELECT: closed fd %d\n", vp->value);
		}
		LL_Clear (SELECT_MASTER->ll_close_fds);
    }
	/* testing */ 

    if (ms < 0) {
        timeout = NULL;
#ifdef MRT_DEBUG
        trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: blocking in select (thread)\n");
#endif /* MRT_DEBUG */
    }
    else {
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        timeout = &tv;
#ifdef MRT_DEBUG
        trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: blocking in select (timeout %d ms)\n", ms);
#endif /* MRT_DEBUG */
    }
    fdvar_read = SELECT_MASTER->fdvar_read;
    fdvar_write = SELECT_MASTER->fdvar_write;
    fdvar_except = SELECT_MASTER->fdvar_except;
	
    if ((nfds = select (FD_SETSIZE, &fdvar_read,
		&fdvar_write, &fdvar_except, timeout)) < 0) {

	if (socket_errno () == EINTR)
	    return (0);
        trace (TR_ERROR, SELECT_MASTER->trace, "SELECT: (%m)\n");
	return (-1);

    }


    if (nfds <= 0) {
		/* timeout */
		return (0);
    }

    pthread_mutex_lock (&SELECT_MASTER->mutex_lock);

#ifdef MRT_DEBUG
    trace (TR_TRACE, SELECT_MASTER->trace, 
		"SELECT: processing select (active %d fds)\n", nfds);
#endif /* MRT_DEBUG */
	
#ifdef NOOO
sddsd
    /* process all pending closes */
    if (LL_GetCount (SELECT_MASTER->ll_close_fds) > 0) {
		ll_value_t *vp;
		LL_Iterate (SELECT_MASTER->ll_close_fds, vp) {
			close (vp->value);
            trace (TR_TRACE, SELECT_MASTER->trace, 
				"SELECT: closed fd %d\n", vp->value);
		}
		LL_Clear (SELECT_MASTER->ll_close_fds);
    }
#endif 

    /* okay, read hint to ourselves that a new socket has been added */
    if (FD_ISSET (SELECT_MASTER->interrupt_fd[0], &fdvar_read)) {
		char c;
		read (SELECT_MASTER->interrupt_fd[0], &c, 1);
#ifdef MRT_DEBUG
		trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: processing select (hint)\n");
#endif /* MRT_DEBUG */
		if (--nfds <= 0) {
			pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
	trace (TR_TRACE, SELECT_MASTER->trace, "unlocking select\n"); 
			return (0);
		}
    }

    time (&now);
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		/* this may be not accurate, but it would be ok for now */
		if (dsp->timed && dsp->timed <= now) {
			Descriptor_Struct *prev;
            prev = LL_GetPrev (SELECT_MASTER->ll_descriptors, dsp);
			close (dsp->fd);
            LL_Remove (SELECT_MASTER->ll_descriptors, dsp);
            trace (TR_TRACE, SELECT_MASTER->trace, 
				"SELECT: closed fd %d on timeout\n", dsp->fd);
            dsp = prev;
		}
		
		if (((dsp->type_mask & SELECT_READ) && 
			FD_ISSET (dsp->fd, &fdvar_read)) ||
			((dsp->type_mask & SELECT_WRITE) && 
			FD_ISSET (dsp->fd, &fdvar_write)) ||
			((dsp->type_mask & SELECT_EXCEPTION) && 
			FD_ISSET (dsp->fd, &fdvar_except))) {
			
			/* Clearing the bit is required because sometimes this routine is called 
			again before reading data waiting. */
			if (dsp->type_mask & SELECT_READ)
				FD_CLR (dsp->fd, &SELECT_MASTER->fdvar_read);
			if (dsp->type_mask & SELECT_WRITE)
				FD_CLR (dsp->fd, &SELECT_MASTER->fdvar_write);
			if (dsp->type_mask & SELECT_EXCEPTION)
				FD_CLR (dsp->fd, &SELECT_MASTER->fdvar_except);
			
            /* schedule it right now while locking */
            if (dsp->schedule) {
                schedule_event3 (dsp->schedule, Ref_Event (dsp->event));
            }
			else {
			/* event is sometimes refered outside of locking
			so that reference counter is introduced.
				deletion may happen unexpectedly. */
				LL_Add2 (ll, Ref_Event (dsp->event));
			}
		}
    }

    pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
	
    /* this way should be fare, avoiding to server the same fd
	that frequently becomes ready */
    if (ll) {
		event_t *event;
		LL_Iterate (ll, event) {
			/* this derefs the event */
			schedule_event_dispatch (event);
		}
		LL_Destroy (ll);
    }
    else {
        /* timeout or no fds registered (may happen on closing) */
    }
    return (0);
}


#define MRT_SELECT_WAIT 1000
int 
mrt_select (void) {
	return (mrt_select2 (MRT_SELECT_WAIT));
}


#ifdef HAVE_LIBPTHREAD
static void 
mrt_select_loop (void)
{
    SELECT_MASTER->self = pthread_self ();
    init_mrt_thread_signals ();
	
    while (1)
		mrt_select2 (-1);
}
#endif /* HAVE_LIBPTHREAD */

/* #define MRT_DEBUG 1 */


/* given a mask (SELECT_READ| SELECT_WRITE_ | SELECT_EXCEPTION),
* disable a socket that matches the fd AND the mask if mask != 0
*/
int 
select_disable_fd_mask (int fd, u_long mask)
{
    Descriptor_Struct *dsp;
    int changed = 0;

    pthread_mutex_lock (&SELECT_MASTER->mutex_lock);
	
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		if (dsp->fd != fd)
			continue;
		if (mask && (dsp->type_mask & mask) == 0)
			continue;
		
		if (dsp->type_mask & SELECT_READ)
			FD_CLR (fd, &SELECT_MASTER->fdvar_read);
		if (dsp->type_mask & SELECT_WRITE)
			FD_CLR (fd, &SELECT_MASTER->fdvar_write);
		if (dsp->type_mask & SELECT_EXCEPTION)
			FD_CLR (fd, &SELECT_MASTER->fdvar_except);
		
		changed++;
       	trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: %s disabled on %d mask 0x%x\n", dsp->name, dsp->fd, 
			dsp->type_mask);
    }
	
    if (changed) {
		write (SELECT_MASTER->interrupt_fd[1], "", 1);
        pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
		return (1);
    }
    pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
#ifdef MRT_DEBUG
    trace (FATAL, SELECT_MASTER->trace, "SELECT: no fd %d (disable)\n", fd);
#endif	/* MRT_DEBUG */
    return (-1);
}


/* there is no way to disable for write only */

int 
select_disable_fd (int fd)
{
    return (select_disable_fd_mask (fd, 0));
}


/*
* the mrt_select master must be notified that a socket is ready for selection 
* again after the select call routine has fired
*/
int 
select_enable_fd_mask (int fd, u_long mask)
{
    Descriptor_Struct *dsp;
    int changed = 0;
	
    assert (fd >= 0);

    pthread_mutex_lock (&SELECT_MASTER->mutex_lock);
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		if (dsp->fd != fd)
			continue;
		if (mask && (dsp->type_mask & mask) == 0)
			continue;
		
		if (dsp->type_mask & SELECT_READ)
			FD_SET (fd, &SELECT_MASTER->fdvar_read);
		if (dsp->type_mask & SELECT_WRITE)
			FD_SET (fd, &SELECT_MASTER->fdvar_write);
		if (dsp->type_mask & SELECT_EXCEPTION)
			FD_SET (fd, &SELECT_MASTER->fdvar_except);
		
		changed++;
       	trace (TR_TRACE, SELECT_MASTER->trace, 
			"SELECT: %s enabled on %d mask 0x%x\n", dsp->name, dsp->fd, 
			dsp->type_mask);
    }
	
    if (changed) {
		write (SELECT_MASTER->interrupt_fd[1], "", 1);
		pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
		return (1);
    }
    /* this fd does not exist! */
    pthread_mutex_unlock (&SELECT_MASTER->mutex_lock);
#ifdef MRT_DEBUG
    trace (FATAL, SELECT_MASTER->trace, "SELECT: no fd %d (enable)\n", fd);
#endif
    return (-1);
}


int 
select_enable_fd (int fd)
{
    return (select_enable_fd_mask (fd, 0));
}


#ifdef notdef
/* called on exit */
int
select_shutdown ()
{
    Descriptor_Struct *dsp;
	
    LL_Iterate (SELECT_MASTER->ll_descriptors, dsp) {
		trace (TR_TRACE, SELECT_MASTER->trace,  
			"SELECT: Shutting down %d mask 0x%x\n", dsp->fd, dsp->type_mask);
		close (dsp->fd);
    }
	
    return (1);
}
#endif


/* leave it disabled */
int 
select_add_fd_off (int fd, u_long type_mask, void (*call_fn) (), void *arg)
{
    return (select_add_fd_event ("select_add_fd_off", fd, type_mask, 0, NULL, 
				 call_fn, 1, arg));
}


/* this is here because of avoiding undefined without librib.a */
void
socket_set_nonblocking (int sockfd, int yes)
{
#ifdef FIONBIO
    if (ioctl (sockfd, FIONBIO, &yes) < 0) {
	trace (TR_ERROR, MRT->trace, "ioctl FIONBIO (%d) failed (%m)\n",
	       yes);
    }
#else
    if (fcntl (sockfd, F_SETFL, (yes)? O_NONBLOCK: 0) < 0) {
	trace (TR_ERROR, MRT->trace, "fcntl F_SETFL %s failed (%m)\n",
	       (yes)? "O_NONBLOCK": "0");
    }
#endif /* FIONBIO */
}











