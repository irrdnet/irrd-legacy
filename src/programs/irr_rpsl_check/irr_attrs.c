/* 
 * $Id: irr_attrs.c,v 1.14 2001/11/14 17:58:24 ljb Exp $attrs.rpsl
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

#include "attrs.rpsl"

char RPSL_dictionary[] =
"dictionary: RPSL\nrp-attribute: pref operator=(integer[0, 65535])\nrp-attribute: med operator=(union integer[0, 65535], enum[igp_cost])\nrp-attribute: dpa operator=(integer[0, 65535])\nrp-attribute: aspath prepend(as_number, ...)\ntypedef: community_elm union integer[1, 4294967295], enum[internet, no_export, no_advertise]\ntypedef: community_list list of community_elm\nrp-attribute: community operator=(community_list) operator.=(community_list) append(community_elm, ...) delete(community_elm, ...) contains(community_elm, ...) operator()(community_elm, ...) operator==(community_list)\nrp-attribute: next-hop operator=(union ipv4_address, ipv6_address, enum[self])\nrp-attribute: cost operator=(integer[0, 65535])\nprotocol: BGP4\n MANDATORY asno(as_number) OPTIONAL flap_damp() OPTIONAL flap_damp(integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535])\nprotocol: MPBGP\n MANDATORY asno(as_number) OPTIONAL flap_damp() OPTIONAL flap_damp(integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535],integer[0,65535])\nprotocol: OSPF\nprotocol: RIP\nprotocol: IGRP\nprotocol: IS-IS\nprotocol: STATIC\nprotocol: RIPng\nprotocol: DVMRP\nprotocol: PIM-DM\nprotocol: PIM-SM\nprotocol: CBT\nprotocol: MOSPF\nafi: ipv4\nafi: ipv4.unicast\nafi: ipv4.multicast\nafi: ipv6\nafi: ipv6.unicast\nafi: ipv6.multicast\nafi: any\nafi: any.unicast\nafi: any.multicast\ndescr: Initial RPSL Dictionary\ntech-c: IRRD-US\nchanged: ljb@merit.edu 20061103\nsource: RADB\n\n";

