/*
 * $Id: defs.h,v 1.2 2001/07/13 18:10:10 ljb Exp $
 */

#ifndef _DEFS_H
#define _DEFS_H
/* need HAVE_U_TYPES */
#include "config.h"
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif /* HAVE_SYS_BITYPES_H */
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif /* HAVE_INTTYPES_H */

#define	BUFSIZE 1024 /* a basic buffer size */

/* check for IPV6 support */
#ifdef AF_INET6
#ifdef IN6_IS_ADDR_V4MAPPED
#define HAVE_IPV6
#endif
#else
#define AF_INET6 24 /* XXX - may conflict with other value */
#endif

#define BIT_SET(f, b)   ((f) |= b)
#define BIT_RESET(f, b) ((f) &= ~(b))
#define BIT_FLIP(f, b)  ((f) ^= (b))
#define BIT_TEST(f, b)  ((f) & (b))
#define BIT_MATCH(f, b) (((f) & (b)) == (b))
#define BIT_COMPARE(f, b1, b2)  (((f) & (b1)) == b2)
#define BIT_MASK_MATCH(f, g, b) (!(((f) ^ (g)) & (b)))

#ifndef byte
#define byte u_char
#endif

#ifndef HAVE_U_TYPES
typedef unsigned char	u_char;
typedef unsigned int	u_int;
typedef unsigned short	u_short;
typedef unsigned long	u_long;
#endif /* HAVE_U_TYPES */

#endif /* _DEFS_H */
