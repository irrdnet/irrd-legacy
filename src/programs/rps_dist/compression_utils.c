#include "rpsdist.h"
#include <stdarg.h>
#include <trace.h>
#include <regex.h>
#include <unistd.h>
#include <strings.h>
#include <irrd_ops.h>

extern trace_t * default_trace;

/* XXX - Look into libgz/libgzip */

/* 
 *  Call gzip to zip transaction (needs a TIMER)
 */
void zip_file(char * inflnm){
  int childpid;
  int  status;
 
  printf("Inside zip_stream\n");
  
  switch( (childpid = fork())) {
  case -1:
    trace(ERROR, default_trace, "Zip Stream: Fork Error\n");
    break;
    
  case 0:
    if( execlp("gzip", "gzip", "-f", inflnm, NULL) < 0 ){
      printf("execlp error");
      exit(1);
    }
    break;
    
  default:
    break;
    
  }
  while( (wait( &status)) != childpid);
}

/* 
 *  Call gunzip to unzip transaction (needs a TIMER)
 */
void unzip_file(char * inflnm){
  int childpid;
  int  status;

  switch( (childpid = fork())) {
  case -1:
    trace(ERROR, default_trace, "Unzip stream: Fork error\n");
    break;
  case 0:
    if( execlp("gunzip", "gunzip", inflnm, NULL) < 0 ){
      printf("execlp error");
      exit(1);
    }
    break;
  default:
    break;
    
  }
 
  while( (wait( &status)) != childpid);
}
