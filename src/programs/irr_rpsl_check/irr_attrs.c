/* 
 * $Id: irr_attrs.c,v 1.14 2001/11/14 17:58:24 ljb Exp $
 */


/*
 * This file has the data structures used by Bison for
 * checking legal, multiple, and mandatory attributes.
 * It also has the long attribute field names array,
 * the object type names array, and the error message
 * buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <irr_rpsl_check.h>

/* Error and warning messages go here */
char error_buf[MAX_ERROR_BUF_SIZE];
/* string lex tokens go here */
int int_size;
FILE *ofile = NULL;
char *flushfntmpl =  "/var/tmp/irrcheck.XXXXXX";

char *too_many_errors = "Too many errors, turning off error messages!\n";
int too_many_size;
/* canonicalized strings go here */
char parse_buf[MAX_CANON_BUF_SIZE];
canon_line_t lineptr[CANON_LINES_MAX]; /* has info about each line in parse buf */
canon_info_t canon_obj; /* global info about objects that are being canonicalized */
/* This is the default value for canonicalization of objects.
 * Use '-c' command flag to turn on canonicalization.
 * Use '-q' to turn on quick check; no object output, report first error and stop
 * Use '-h' to turn on info headers which are used in pipeline fashion
 *          as input to other routines (ie, IRRd object processing)
 */
int CANONICALIZE_OBJS_FLAG = 0;
int QUICK_CHECK_FLAG       = 0;
int INFO_HEADERS_FLAG      = 0;
canon_info_t canon_obj;

const char ERROR_TOKEN[] = "<?>";

/* array of regular expressions */
regex_t re[MAX_ATTRS + REGEX_TOKEN_COUNT];

/* data structs from perl program */

/*--------Bison legal attr------*/
short legal_attrs[MAX_OBJS][MAX_ATTRS] = {
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 16, 2, 6, 15, 14, 5,
 13, 1, 17, 7, 8, 9, 10, 11, 12, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 20, 21,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 5, 6, 7, 8, 12, 2, 0, 11, 10, 0,
 9, 0, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 16, 17,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 13, 3, 0, 12, 11, 0,
 10, 0, 14, 0, 0, 0, 0, 0, 0, 1, 2, 7, 8, 9, 5, 6, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 17, 18,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 15, 2, 0, 14, 13, 0,
 12, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 1, 3, 4, 5,
 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 19, 20,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 10, 2, 0, 9, 8, 0,
 5, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 14, 15,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 10, 2, 0, 9, 8, 0,
 5, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 4, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 14, 15,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 10, 2, 0, 9, 8, 0,
 5, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 4, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 14, 15,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 9, 3, 0, 8, 7, 0,
 4, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 13, 14,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 9, 3, 0, 8, 7, 0,
 4, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 13, 14,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 9, 8, 0,
 7, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5,
 6, 0, 0, 0, 0, 0, 0, 1024, 1024, 14, 15,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 13, 0, 0, 12, 11, 0,
 10, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 5,
 7, 1, 6, 0, 0, 0, 0, 1024, 1024, 17, 18,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 11, 2, 0, 10, 9, 0,
 8, 0, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 1, 5, 6, 7, 1024, 1024, 15, 16,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 9, 0, 0, 8, 7, 0,
 6, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 13, 14,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6,
 7, 8, 9, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 16, 10, 0, 15, 14, 0,
 11, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 20, 21,},
{ 0, 0, 0, 0, 0, 0, 1, 2, 4, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 15, 3, 0, 14, 13, 0,
 12, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 19, 20,},
{ 1, 5, 6, 7, 8, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 13, 2, 0, 11, 10, 0,
 9, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 1024, 1024, 17, 18,},
};

/*--------Bison multiple_attrs------*/
short mult_attrs[MAX_OBJS][MAX_ATTRS] = {
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0,
 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1,
 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
{ 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
};

/*--------attr_hash------*/
char *attr_hash[MAX_OBJS][MAX_ATSQ_LEN] = {
{"route", "descr", "admin-c", "tech-c", "origin", "holes", "member-of", "inject", "components", "aggr-bndry", "aggr-mtd", "export-comps", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"dictionary", "descr", "admin-c", "tech-c", "typedef", "rp-attribute", "protocol", "encapsulation", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"aut-num", "as-name", "descr", "admin-c", "tech-c", "member-of", "import", "export", "default", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"inet-rtr", "descr", "alias", "local-as", "ifaddr", "peer", "member-of", "rs-in", "rs-out", "admin-c", "tech-c", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"as-set", "descr", "members", "mbrs-by-ref", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"route-set", "descr", "members", "mbrs-by-ref", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"rtr-set", "descr", "members", "mbrs-by-ref", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"peering-set", "peering", "descr", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"filter-set", "filter", "descr", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"person", "address", "phone", "fax-no", "e-mail", "nic-hdl", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"role", "address", "phone", "fax-no", "e-mail", "trouble", "nic-hdl", "admin-c", "tech-c", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"mntner", "descr", "admin-c", "tech-c", "upd-to", "mnt-nfy", "auth", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"key-cert", "method", "owner", "fingerpr", "certif", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"repository", "query-address", "response-auth-type", "submit-address", "submit-auth-type", "repository-cert", "expire", "heartbeat-interval", "reseed-address", "descr", "remarks", "admin-c", "tech-c", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"inetnum", "netname", "descr", "country", "admin-c", "tech-c", "aut-sys", "ias-int", "gateway", "rev-srv", "status", "remarks", "notify", "mnt-by", "changed", "source", "delete", "override", "password", "cookie", ""},
{"domain", "descr", "admin-c", "tech-c", "zone-c", "nserver", "sub-dom", "dom-net", "remarks", "notify", "mnt-by", "refer", "changed", "source", "delete", "override", "password", "cookie", ""},
};


/*--------attr_map_field------*/
short attr_map_field[MAX_OBJS][MAX_ATSQ_LEN] = {
{ 41, 35, 29, 54, 39, 36, 43, 44, 45, 46, 47, 48, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 28, 35, 29, 54, 30, 31, 32, 33, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 49, 50, 35, 29, 54, 55, 51, 52, 53, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 56, 35, 57, 58, 59, 60, 61, 62, 63, 29, 54, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 64, 35, 65, 66, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 67, 35, 68, 66, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 69, 35, 70, 66, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 71, 72, 35, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 73, 74, 35, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 75, 76, 77, 78, 79, 80, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 81, 76, 77, 78, 79, 82, 80, 29, 54, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 83, 35, 29, 54, 84, 85, 86, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 23, 24, 25, 26, 27, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 14, 15, 16, 17, 18, 19, 20, 21, 22, 35, 40, 29, 54, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 6, 7, 35, 8, 29, 54, 9, 10, 11, 12, 13, 40, 38, 37, 34, 42, 87, 88, 89, 90, -1},
{ 0, 35, 29, 54, 1, 2, 3, 4, 40, 38, 37, 5, 34, 42, 87, 88, 89, 90, -1},
};

/*--------Bison mandatory's------*/
short mand_attrs[MAX_OBJS][MAX_MANDS] = {
{ 41, 35, 39, 37, 34, 42, -1},
{ 28, 35, 54, 37, 34, 42, -1},
{ 49, 50, 35, 29, 54, 37, 34, 42, -1},
{ 56, 58, 59, 54, 37, 34, 42, -1},
{ 64, 35, 54, 37, 34, 42, -1},
{ 67, 35, 54, 37, 34, 42, -1},
{ 69, 35, 54, 37, 34, 42, -1},
{ 71, 72, 35, 54, 37, 34, 42, -1},
{ 73, 74, 35, 54, 37, 34, 42, -1},
{ 75, 80, 76, 77, 79, 37, 34, 42, -1},
{ 81, 80, 76, 77, 79, 54, 37, 34, 42, -1},
{ 83, 54, 84, 86, 35, 37, 34, 42, -1},
{ 23, 27, 37, 34, 42, -1},
{ 14, 15, 16, 17, 18, 19, 20, 21, 22, 35, 54, 37, 34, 42, -1},
{ 6, 13, 29, 54, 34, 35, 8, 7, 37, 42, -1},
{ 0, 35, 29, 54, 1, 34, 42, -1},
};

/*--------attrs which are keys-----------*/
short attr_is_key[MAX_ATTRS] = {
15, -1, -1, -1, -1, -1, 14, -1, -1, -1, -1, -1, -1, -1, 13,
-1, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, -1, 1, -1,
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1,
-1, -1, -1, -1, 2, -1, -1, -1, -1, -1, -1, 3, -1, -1, -1,
-1, -1, -1, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, 8, -1,
9, -1, -1, -1, -1, -1, 10, -1, 11, -1, -1, -1, -1, -1, -1,
-1
};

/*-------long attributes char array------*/
char *attr_name[MAX_ATTRS] = {
    "domain", "zone-c", "nserver", "sub-dom", "dom-net",
    "refer", "inetnum", "netname", "country", "aut-sys",
    "ias-int", "gateway", "rev-srv", "status", "repository",
    "query-address", "response-auth-type", "submit-address", "submit-auth-type", "repository-cert",
    "expire", "heartbeat-interval", "reseed-address", "key-cert", "method",
    "owner", "fingerpr", "certif", "dictionary", "admin-c",
    "typedef", "rp-attribute", "protocol", "encapsulation", "changed",
    "descr", "holes", "mnt-by", "notify", "origin",
    "remarks", "route", "source", "member-of", "inject",
    "components", "aggr-bndry", "aggr-mtd", "export-comps", "aut-num",
    "as-name", "import", "export", "default", "tech-c",
    "member-of", "inet-rtr", "alias", "local-as", "ifaddr",
    "peer", "member-of", "rs-in", "rs-out", "as-set",
    "members", "mbrs-by-ref", "route-set", "members", "rtr-set",
    "members", "peering-set", "peering", "filter-set", "filter",
    "person", "address", "phone", "fax-no", "e-mail",
    "nic-hdl", "role", "trouble", "mntner", "upd-to",
    "mnt-nfy", "auth", "delete", "override", "password",
    "cookie"
};

/*-------short attributes char array------*/
char *attr_sname[MAX_ATTRS] = {
    "*dn", "*zc", "*ns", "*sd", "*di", "*rf", "*in", "*na", "*cy", "*ay",
    "*ii", "*gw", "*rz", "*su", "*re", "*qa", "*ru", "*sa", "*st", "*rc",
    "*ex", "*hi", "*ra", "*kc", "*mh", "*ow", "*fp", "*ct", "*dc", "*ac",
    "*td", "*rp", "*pl", "*en", "*ch", "*de", "*ho", "*mb", "*ny", "*or",
    "*rm", "*rt", "*so", "*mo", "*ij", "*co", "*ab", "*ar", "*ec", "*an",
    "*aa", "*ip", "*ep", "*df", "*tc", "*mf", "*ir", "*az", "*la", "*if",
    "*pe", "*mr", "*ri", "*rx", "*as", "*ms", "*my", "*rs", "*me", "*rr",
    "*mg", "*ps", "*pr", "*fs", "*fr", "*pn", "*ad", "*ph", "*fx", "*em",
    "*nh", "*ro", "*tb", "*mt", "*dt", "*mn", "*at", "*ud", "*uo", "*up",
    "*uc"
};

/*-------object types char array------*/
char *obj_type[MAX_OBJS] = {
    "route", "dictionary", "aut-num", "inet-rtr", "as-set",
    "route-set", "rtr-set", "peering-set", "filter-set", "person",
    "role", "mntner", "key-cert", "repository", "inetnum",
    "domain"
};

/* -------Countries char array------ */
char *countries[] = {
};

/* data structs from perl program */

char RPSL_dictionary[] =
"dictionary: RPSL\nrp-attribute: pref operator=(integer[0, 65535])\nrp-attribute: med operator=(union integer[0, 65535], enum[igp_cost])\nrp-attribute: dpa operator=(integer[0, 65535])\nrp-attribute: aspath prepend(as_number, ...)\ntypedef: community_elm union integer[1, 2147483647], enum[internet, no_export, no_advertise]\ntypedef: community_list list of community_elm\nrp-attribute: community operator=(community_list) operator.=(community_list) append(community_elm, ...) delete(community_elm, ...) contains(community_elm, ...) operator()(community_elm, ...) operator==(community_list)\nrp-attribute: next-hop operator=(union ipv4_address, enum[self])\nrp-attribute: cost operator=(integer[0, 65535])\nprotocol: BGP4\n MANDATORY asno(as_number) OPTIONAL flap_damp() OPTIONAL flap_damp(integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535])\nprotocol: OSPF\nprotocol: RIP\nprotocol: IGRP\nprotocol: IS-IS\nprotocol: STATIC\nprotocol: RIPng\nprotocol: DVMRP\nprotocol: PIM-DM\nprotocol: PIM-SM\nprotocol: CBT\nprotocol: MOSPF\ndescr: Initial RPSL Dictionary\ntech-c: IRRD-US\nchanged: gerald@merit.edu 19990618\nsource: RADB\n\n";


