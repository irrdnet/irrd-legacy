/*
 * $Id: ntconfig.h,v 1.2 2000/08/18 18:42:12 labovit Exp $
 */


/* Set configuration options by hand for NT because configure does not
 * work.
 */

#ifndef _NTCONFIG_H
#define _NTCONFIG_H

#define IFNAMSIZ				1024	/* interface size */
#define MAX_MSG_SIZE            8192	/* IPC message size */

// IPv6 Switches
//define HAVE_IPV6			1
//#define HAVE_INET_NTOP		1

#define NT_NOV4V6   /* NT IPv6 stack is not dual, and there is no IPv6 telnet so don't use IPv6 UII */

#define	HAVE_MEMMOVE			1
#define HAVE_STRERROR			1

#define IPV6MR_INTERFACE_INDEX	1

#define	mode_t		int
#define IFF_POINTOPOINT	IFF_POINTTOPOINT
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#define fcntl	ioctlsocket
#define assert	ASSERT
#define getpid	_getpid
#define getcwd	_getcwd
#define pipe	_pipe
#define ioctl	ioctlsocket

#define ssize_t int
#define DEF_DBCACHE "C:\database"
#define mkstemp(a) mkstemps(a,5)

#define write	_write
#define read	_read
#define lseek	_lseek
#define alloca	malloc
//#define open	_open

#define	NO_GETOPT	1

#define EWOULDBLOCK					WSAEWOULDBLOCK

//#define INET6_ADDRSTRLEN			46
//#define IPV6MR_INTERFACE_INDEX		1

//#define IN6_IS_ADDR_UNSPECIFIED		In6_IS_ADDR_UNSPECIFIED
//#define IN6_IS_ADDR_V4MAPPED		In6_IS_ADDR_V4MAPPED
//#define IN6_IS_ADDR_V4MAPPED		In6_IS_ADDR_V4MAPPED
//#define IN6_IS_ADDR_MC_LINKLOCAL	In6_IS_ADDR_MC_LINKLOCAL
//#define IN6_IS_ADDR_LOOPBACK		In6_IS_ADDR_LOOPBACK
//#define IN6_IS_ADDR_LINKLOCAL		In6_IS_ADDR_LINKLOCAL
//#define IN6_IS_ADDR_V4COMPAT		In6_IS_ADDR_V4COMPAT
//#define IN6_IS_ADDR_MULTICAST		In6_IS_ADDR_MULTICAST
//#define IN6_IS_ADDR_MC_SITELOCAL    In6_IS_ADDR_MC_SITELOCAL
//#define IN6_IS_ADDR_SITELOCAL		In6_IS_ADDR_SITELOCAL
#endif /* _NTCONFIG_H */			
