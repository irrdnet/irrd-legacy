#include <config.h>
#include "rpsdist.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <unistd.h>
#include <string.h>
#include <strings.h>

extern trace_t *default_trace;

/*
 * Once initialized, this loops infintely looking for new connections
 */
void rps_select_loop(struct select_master_t * Select_Master){
  fd_set reads;
  int nfds;
  pthread_t t;
  pthread_attr_t attr;
  rps_connection_t * tmp;
  
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  while(1){
    reads = Select_Master->read_set;
    if( (nfds = select(Select_Master->max_fd_value, &reads, NULL, NULL, NULL)) > 0 ){
      pthread_mutex_lock(&Select_Master->mutex_lock);

      /* check for new additions */
      if( FD_ISSET (Select_Master->interrupt_fd[0], &reads) ){
	char c;
	read (Select_Master->interrupt_fd[0], &c, 1);
	if( --nfds <= 0 ){
	  pthread_mutex_unlock(&Select_Master->mutex_lock);
	  continue;
	}
      }
     
      /* process rest */
      tmp = Select_Master->active_links;
      while( tmp ){
	if( FD_ISSET (tmp->sockfd, &reads) ){
	  FD_CLR(tmp->sockfd, &Select_Master->read_set);       /* re-activate later */
	  pthread_create(&t, &attr, Select_Master->call_func, (int*)tmp->sockfd);
	  nfds--;
	}
	tmp = tmp->next;
      }
      pthread_mutex_unlock(&Select_Master->mutex_lock);
    }
  }
}
 
/* 
 * when we get a new connection, we need to add it into the fd_set
 * the select is waiting on, this does it for us
 */
rps_connection_t *
rps_select_add_peer(int fd, struct select_master_t * Select_Master){
  rps_connection_t * tmp, *tmp_lock;
  pthread_mutex_lock(&Select_Master->mutex_lock);
  tmp = Select_Master->active_links;

  if( !tmp ){
    if( (tmp = (rps_connection_t *) malloc(sizeof(rps_connection_t))) == NULL){
      trace(ERROR, default_trace, "rps_select_add_peer: malloc error\n");
      return NULL;
    }
    bzero(tmp, sizeof(rps_connection_t));
    Select_Master->active_links = tmp;
    trace(INFO, default_trace, "rps_select_add_peer: adding new fd(%d)\n", fd);
  }
  else{
    /* find end of list (or fd) */
    while( tmp->next ){
      if(tmp->sockfd == fd){
	FD_SET(fd, &Select_Master->read_set);
	if( write(Select_Master->interrupt_fd[1], "c", 1) <= 0 )
	  trace(ERROR, default_trace, "rps_select_add_peer: Error writing %s \n", strerror(errno));
	pthread_mutex_unlock(&Select_Master->mutex_lock);
	return NULL;
      }
      tmp_lock = tmp;
      tmp = tmp->next;
    }
    /* check the very last node */
    if( tmp->sockfd == fd ){
      FD_SET(fd, &Select_Master->read_set);
      if( write(Select_Master->interrupt_fd[1], "c", 1) <= 0 )
	trace(ERROR, default_trace, "rps_select_add_peer: Error writing %s \n", strerror(errno));
      pthread_mutex_unlock(&Select_Master->mutex_lock);
      return NULL;
    } 
    trace(INFO, default_trace, "rps_select_add_peer: adding fd(%d)\n", fd);
    if( (tmp->next = (rps_connection_t *) malloc(sizeof(rps_connection_t))) == NULL){
      trace(ERROR, default_trace, "rps_select_add_peer: malloc error\n");
      return NULL;
    }
    bzero(tmp->next, sizeof(rps_connection_t));
    tmp->next->prev = tmp;
    tmp = tmp->next;
  }
  tmp->buffer[0] = '\0';
  tmp->tmp[0] = '\0';
  tmp->cp = tmp->buffer;
  tmp->end = tmp->buffer;
  tmp->sockfd = fd;
  FD_SET(fd, &Select_Master->read_set);
  if( fd+1 > Select_Master->max_fd_value )
    Select_Master->max_fd_value = fd + 1;
  if( write(Select_Master->interrupt_fd[1], "c", 1) <= 0 )
    trace(ERROR, default_trace, "rps_select_add_peer: Error writing %s \n", strerror(errno));
  pthread_mutex_unlock(&Select_Master->mutex_lock);
  return tmp;
}

/*
 * remove for the list, remove from the select list, etc.
 */
void
rps_select_remove_peer(int fd, struct select_master_t * Select_Master){
  rps_connection_t * tmp;
  pthread_mutex_lock(&Select_Master->mutex_lock);
  tmp = Select_Master->active_links;

  trace(INFO, default_trace, "rps_select_remove_peer: removing peer(%d)\n", fd);

  while(tmp){
    if( tmp->sockfd == fd ){
      if(tmp->prev)
	tmp->prev->next = tmp->next;
      else
	Select_Master->active_links = tmp->next;
      if(tmp->next)
	tmp->next->prev = tmp->prev;
      close(tmp->sockfd);
      free(tmp);
      FD_CLR(fd, &Select_Master->read_set);
      break;
    }
    tmp = tmp->next;
  }
  if( write(Select_Master->interrupt_fd[1], "c", 1) <= 0 )
    trace(ERROR, default_trace, "rps_select_add_peer: Error writing %s \n", strerror(errno));
  pthread_mutex_unlock(&Select_Master->mutex_lock); 
}

/*
 * Always initialize your select_master's, else be sorry
 */
void
rps_select_init(void * in_func, struct select_master_t * Select_Master){
  pthread_t wait_thread;

  pthread_mutex_init(&Select_Master->mutex_lock, NULL);
  if( pipe(Select_Master->interrupt_fd) < 0 ){
    trace(ERROR, default_trace, "rps_select_init: Error setting up rps_select: pipe error\n");
    fprintf(stderr, "rps_select_init: Error setting up rps_select: pipe error\n");
    exit(EXIT_ERROR);
  }
  Select_Master->call_func = in_func;
  FD_ZERO(&Select_Master->read_set);
  FD_SET(Select_Master->interrupt_fd[0], &Select_Master->read_set);
  Select_Master->max_fd_value = Select_Master->interrupt_fd[0] + 1;
  Select_Master->active_links = NULL;
  pthread_create(&wait_thread, NULL, (void *)rps_select_loop, (void*)Select_Master);
}

/*
 * We know the int, where's the stinking object it belongs to?
 */
rps_connection_t *
rps_select_find_fd(int sockfd, struct select_master_t * Select_Master){
  rps_connection_t * tmp;
  pthread_mutex_lock(&Select_Master->mutex_lock); 
  tmp = Select_Master->active_links;

  while(tmp){
    if( tmp->sockfd == sockfd ){
      pthread_mutex_unlock(&Select_Master->mutex_lock); 
      return tmp;
    }
    tmp = tmp->next;
  }
  pthread_mutex_unlock(&Select_Master->mutex_lock); 
  return NULL;
}

rps_connection_t * 
rps_select_find_fd_by_host(unsigned long host, struct select_master_t * Select_Master){
  rps_connection_t * tmp;
  
  pthread_mutex_lock(&Select_Master->mutex_lock); 
  tmp = Select_Master->active_links;

  while(tmp){
    if( tmp->host == host ){
      pthread_mutex_unlock(&Select_Master->mutex_lock); 
      return tmp;
    }
    tmp = tmp->next;
  }
  pthread_mutex_unlock(&Select_Master->mutex_lock); 
  return NULL;
}
