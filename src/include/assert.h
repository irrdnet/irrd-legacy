/*
 * $Id: assert.h,v 1.1.1.1 2000/02/29 22:28:41 labovit Exp $
 */

#include <config.h>
#ifndef _ASSERT_H
#define _ASSERT_H
#include <string.h>
#include <mrt.h>
#include <trace.h>

#undef assert
#define assert(exp) \
    ((exp)?0: trace (FATAL, MRT->trace, \
        "MRT: assertion (%s) failed at line %d file %s\n",  \
	#exp, __LINE__, __FILE__))

#define perror(s) \
    trace (ERROR, MRT->trace, "MRT: %s: %s\n", s, strerror (errno));

#endif /* _ASSERT_H */
