/* 
 * $Id: prefix.c,v 1.4 2001/08/30 18:10:31 ljb Exp $
 */
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mrt.h>
#include <netdb.h>
#include <irrdmem.h>

#if !defined(__GLIBC__) && defined(__linux__)
#ifdef __osf__

/* apparently, OSF's gethostby{name,addr}_r's are different, broken, and
   deprecated! The normal versions are, in fact, thread safe (sayeth the
   fine man page), so we're defining to use those instead. --dogcow */

#define gethostbyname_r(a,b,c,d,e)   gethostbyname(a,b,c,d,e)
#define gethostbyaddr_r(a,b,c,d,e,f,g) gethostbyaddr(a,b,c)
#endif

struct hostent  *gethostbyname_r 
	(const char *, struct hostent *, char *, int, int *h_errnop);
struct hostent  *gethostbyaddr_r
        (const char *, int, int, struct hostent *, char *, int, int *h_errnop);

#else /* Linux GNU C here */

   /* I'd hope that this doesn't conflict with the above.
      gethostXX_r seems to be different among platforms.
      I need to learn more about them to write a more portable code.
      For the time being, this part tries to convert Linux glibc 2.X
      gethostXX_r into Solaris's that we use to code MRT. -- masaki
    */
#if __GLIBC__ >= 2 || (defined(__FreeBSD__) && defined(HAVE_GETHOSTBYNAME_R))
   /* Glibc 2.X

    int gethostbyname_r (const char *name, struct hostent *result_buf, 
			 char *buf, size_t buflen, struct hostent **result,
                         int *h_errnop);
    int gethostbyaddr_r (const char *addr, int len, int type,
                         struct hostent *result_buf, char *buf,
                         size_t buflen, struct hostent **result,
                         int *h_errnop));
    */

struct hostent *p_gethostbyname_r (const char *name,
    struct hostent *result, char *buffer, int buflen, int *h_errnop) {

    struct hostent *hp;
    if (gethostbyname_r (name, result, buffer, buflen, &hp, h_errnop) < 0)
	return (NULL);
    return (hp);
}

struct hostent *p_gethostbyaddr_r (const char *addr,
    int length, int type, struct hostent *result,
    char *buffer, int buflen, int *h_errnop) {

    struct hostent *hp;
    if (gethostbyaddr_r (addr, length, type, result, buffer, buflen, 
			 &hp, h_errnop) < 0)
	return (NULL);
    return (hp);
}

#define gethostbyname_r(a,b,c,d,e)     p_gethostbyname_r(a,b,c,d,e)
#define gethostbyaddr_r(a,b,c,d,e,f,g) p_gethostbyaddr_r(a,b,c,d,e,f,g)
#endif  /* __GLIBC__ >= 2 */

#endif /* __GLIBC__ */
 
int num_active_prefixes = 0;

/* 
   this is a new feature introduced by masaki. This is intended to shift to
   use a static memory as much as possible. If ref_count == 0, the prefix
   memory must be allocated in a static memory or on a stack instead of
   in the heap, i.e. not allocated by malloc(), that doesn't need to be freed
   later.
*/

prefix_t *
New_Prefix (int family, void *dest, int bitlen)
{
    prefix_t *prefix;

    if (family == AF_INET) {
      prefix = (prefix_t *) irrd_malloc(sizeof(v4_prefix_t));
      pthread_mutex_init (&prefix->mutex_lock, NULL);
      memcpy (&prefix->add.sin, dest, 4);
    }
    else
    if (family == AF_INET6) {
      prefix = (prefix_t *) irrd_malloc(sizeof(prefix_t));
      pthread_mutex_init (&prefix->mutex_lock, NULL);
      memcpy (&prefix->add.sin6, dest, 16);
    }
    else
      return (NULL);

    prefix->ref_count = 1;
    num_active_prefixes++;
    prefix->bitlen = bitlen;
    prefix->family = family;
    return (prefix);
}

/* name_toprefix() takes a hostname and returns a prefix with the 
 * appropriate address information.  This function uses gethostbyname_r,
 * so it should be thread-safe.
 */
prefix_t *
name_toprefix(char *name, trace_t *caller_trace)
{
    struct hostent *hostinfo;
    prefix_t *prefix;

#ifdef HAVE_GETHOSTBYNAME_R
    struct hostent result;
    int h_errno_r;
    char buf[1024];

    hostinfo = gethostbyname_r(name, &result, buf, 1024, &h_errno_r);
#else
    hostinfo = gethostbyname(name);
#endif

    if (hostinfo == NULL) {
#ifdef HAVE_GETHOSTBYNAME_R
      switch (h_errno) {
#else
      switch (errno) {
#endif 
            case HOST_NOT_FOUND:
                trace(NORM, caller_trace, " ** Host %s not found **\n", name);
		break;
        
            case NO_DATA:
            /* case NO_ADDRESS: */
                trace(NORM, caller_trace, 
                    " ** No address data is available for host %s ** \n",
                    name);
		break;

	    case TRY_AGAIN:
		trace(NORM, caller_trace,
		    " ** DNS lookup failure for host %s; try again later **\n",
		    name);
		break;

	    case NO_RECOVERY:
		trace(NORM, caller_trace,
		    "** Unable to get IP address from server for host %s **\n",
		    name);
		break;

	    default:
		trace(NORM, caller_trace, 
		    "** Unknown error while looking up IP address for %s **\n",
		    name);
		break;
	}
	return NULL;
    }

    prefix = New_Prefix (hostinfo->h_addrtype, hostinfo->h_addr, 
		8*hostinfo->h_length);

    if (!prefix)
	trace(NORM, caller_trace, 
	" ** Cannot allocate memory for prefix for host %s! \n", name);

    return prefix;
}

/* string_toprefix autodetects whether "name" is a hostname or an IP
 * address, and calls the appropriate prefix functions from the mrt 
 * library to create a prefix from it.  It returns the new prefix, or NULL
 * if an error occurs.
 * IPv4 and IPv6 are supported.
 * Uses the new universal socket interface extensions defined in RFC 2133.
 */

prefix_t *
string_toprefix(char *name, trace_t *caller_trace)
{
    u_char buf[16]; 	/* Buffer for address; max size = IPv6 = 16 bytes */
    prefix_t *dst;
    int status;

    if (isdigit((int)name[0])) {	/* Numerical address */
	if (!strchr(name, ':')) {	/* No ':' means IPv4 address */
	    if ((status = inet_pton(AF_INET, name, buf)) == 0) {
                trace(NORM, caller_trace, " ** Malformed IP address %s\n",
                    name);
                return NULL;
            } else if (status < 0) { /* Some other error */
		trace(NORM, caller_trace, " **** %s: %s ****\n", 
		    name, strerror(errno));
		return NULL;
	    }
            dst = New_Prefix(AF_INET, buf, 32);
            if (!dst) {
		trace(NORM, caller_trace, 
		    " ** Cannot allocate memory for prefix for host %s! \n",
		    name);
                return NULL;
	    } else
		return dst;
        } else {	/* IPv6 address */
	    if ((status = inet_pton(AF_INET6, name, buf)) == 0) {
                trace(NORM, caller_trace, " ** Malformed IPv6 address %s\n",
                    name);
                return NULL;
            } else if (status < 0) {
                trace(NORM, caller_trace, " **** %s: %s ****\n", 
                    name, strerror(errno));
                return NULL;
            }
	    dst = New_Prefix(AF_INET6, buf, 128);
            if (!dst) {
                trace(NORM, caller_trace, 
                   " ** Cannot allocate memory for prefix for host %s! \n",
                   name);
                return NULL;
            }
	}
    } else {	/* Host name */
	dst = name_toprefix(name, caller_trace);
    }

    return dst;
}

prefix_t *
Ref_Prefix (prefix_t * prefix)
{
    if (prefix == NULL)
	return (NULL);
    if (prefix->ref_count == 0) {
	/* make a copy in case of a static prefix */
        return (New_Prefix (prefix->family, &prefix->add, prefix->bitlen));
    }
    pthread_mutex_lock (&prefix->mutex_lock);
    prefix->ref_count++;
    pthread_mutex_unlock (&prefix->mutex_lock);
    return (prefix);
}

void 
Deref_Prefix (prefix_t * prefix)
{
    if (prefix == NULL)
	return;
    /* for secure programming, raise an assert. no static prefix can call this */
    assert (prefix->ref_count > 0);
    pthread_mutex_lock (&prefix->mutex_lock);

    prefix->ref_count--;
    if (prefix->ref_count <= 0) {
        pthread_mutex_destroy (&prefix->mutex_lock);
	irrd_free(prefix);
        num_active_prefixes--;
	return;
    }
    pthread_mutex_unlock (&prefix->mutex_lock);
}

/* ascii2prefix
 * Converts prefix in ASCII to prefix_t struct
 * May modify string if it contains invalid characters after a prefix
 * (will Null terminate the string at the of the prefix)
 */
prefix_t *
ascii2prefix (int family, char *string)
{
    u_long bitlen, maxbitlen;
    char *cp, *endptr, *prefixstr;
    struct in_addr sin;
    struct in6_addr sin6;
    prefix_t *return_prefix = NULL;

    /* easy way to handle both families */
    if (family == 0) {
       family = AF_INET;
       if (strchr (string, ':')) family = AF_INET6;
    }

    if (family == AF_INET) {
	maxbitlen = 32;
    }
    else if (family == AF_INET6) {
	maxbitlen = 128;
    } else
	return NULL;  /* unrecognized family */

    if ((cp = strchr (string, '/')) != NULL) { /* check for prefix */
	prefixstr = cp + 1; /* point to the prefix part */
	if (!isdigit((int)*prefixstr)) /* first char must be a digit */
	  return NULL; 
	bitlen = strtol (prefixstr, &endptr, 10);
	if (*endptr) /* if junk after prefix, terminate at end of prefix */
	  *endptr = '\0';
	if (bitlen > maxbitlen) /* check for a valid prefix length */
	  return NULL; 
	*cp = '\0';	/* temp. terminate string at "/" char for inet_pton */
    } else
	bitlen = maxbitlen;

    if (family == AF_INET) {
	if (my_inet_pton (AF_INET, string, &sin) > 0)
	  return_prefix = New_Prefix (AF_INET, &sin, bitlen);
    } else {
	if (inet_pton (AF_INET6, string, &sin6) > 0)
	  return_prefix = New_Prefix (AF_INET6, &sin6, bitlen);
    }
    if (cp)
      *cp = '/';   /* Restore "/" in string */

    return return_prefix;
}

/* 
 * print_prefix_list
 */
void 
print_prefix_list (LINKED_LIST * ll_prefixes)
{
    prefix_t *prefix;

    if (ll_prefixes == NULL)
	return;

    LL_Iterate (ll_prefixes, prefix) {
	printf ("  %-15s\n", prefix_toax (prefix));
    }
}

/*
 * print_pref_prefix_list
 */
void 
print_pref_prefix_list (LINKED_LIST * ll_prefixes, u_short *pref)
{
    prefix_t *prefix;
    int i = 0;

    if ((ll_prefixes == NULL) || (pref == NULL))
	return;

    LL_Iterate (ll_prefixes, prefix) {
	printf ("  %-15s %d\n", prefix_toax (prefix), pref[i++]);
    }
}

void 
print_prefix_list_buffer (LINKED_LIST * ll_prefixes, buffer_t * buffer)
{
    prefix_t *prefix;

    if (ll_prefixes == NULL)
	return;

    LL_Iterate (ll_prefixes, prefix) {
	buffer_printf (buffer, "  %-15p\n", prefix);
    }
}

/*
 * print_pref_prefix_list
 */
void 
print_pref_prefix_list_buffer (LINKED_LIST * ll_prefixes, u_short *pref, 
			       buffer_t * buffer)
{
    prefix_t *prefix;
    int i = 0;

    if ((ll_prefixes == NULL) || (pref == NULL))
	return;

    LL_Iterate (ll_prefixes, prefix) {
	buffer_printf (buffer, "  %-15p %d\n", prefix, pref[i++]);
    }
}

/*
 * returns 1 if the both prefixes are equal
 *    both prefix length are equal
 *    both addresses up to the length are equal
 * otherwise, returns 0
 */
int 
prefix_equal (prefix_t * p1, prefix_t * p2)
{
    if (p1->family != p2->family) {
	/* we can not compare in this case */
	return (FALSE);
    }
    return ((p1->bitlen == p2->bitlen) &&
	    (p1->bitlen == 0 || comp_with_mask (
		      prefix_tochar (p1), prefix_tochar (p2), p1->bitlen)));
}

/*
 * returns 1 if the both prefixes are equal
 *    both prefix length are equal
 *    both addresses up to the length are equal
 * otherwise, returns 0
 */
int 
prefix_compare (prefix_t * p1, prefix_t * p2)
{
    return (prefix_equal (p1, p2));
}

/*
 * compare addresses only
 *   both addresses must be totally equal regardless of their length
 * returns 0 if equal, otherwise the difference
 */
int
prefix_compare_wolen (prefix_t *p, prefix_t *q)
{
    int bytes = 4, i, difference;
    u_char *up = prefix_touchar (p);
    u_char *uq = prefix_touchar (q);

    if ((difference = (p->family - q->family)) != 0) {
	/* we can not compare in this case */
	return (difference);
    }
    if (p->family == AF_INET6)
        bytes = 16;
    for (i = 0; i < bytes; i++) {
	if ((difference = (up[i] - uq[i])) != 0)
	    return (difference);
    }
    return (0);
}

/*
 * compare addresses and then bitlen
 *   both addresses must be totally equal regardless of their length
 * returns 0 if equal, otherwise the difference
 */
int
prefix_compare_wlen (prefix_t *p, prefix_t *q)
{
    int r;

    if ((r = prefix_compare_wolen (p, q)) != 0)
	return (r);
    if ((r = (p->bitlen - q->bitlen)) != 0)
	return (r);
    return (0);
}

int
prefix_compare2 (prefix_t *p, prefix_t *q)
{
    return (prefix_compare_wlen (p, q));
}

/* prefix_toa
 */
char *
prefix_toa (prefix_t * prefix)
{
    return (prefix_toa2x (prefix, (char *) NULL, 0));
}

/*
 * prefix_toa with /length
 */
char *
prefix_toax (prefix_t *prefix)
{
    return (prefix_toa2x (prefix, (char *) NULL, 1));
}

/* prefix_toa2
 * convert prefix information to ascii string
 */
char *
prefix_toa2 (prefix_t *prefix, char *buff)
{
    return (prefix_toa2x (prefix, buff, 0));
}

/* 
 * convert prefix information to ascii string with length
 * thread safe and (almost) re-entrant implementation
 */
char *
prefix_toa2x (prefix_t *prefix, char *buff, int with_len)
{
    if (prefix == NULL)
	return ("(Null)");
    if (buff == NULL) {

        struct buffer {
            char buffs[16][48+5];
            u_int i;
        } *buffp;

	THREAD_SPECIFIC_DATA (struct buffer, buffp, 1);
	if (buffp == NULL) {
	    /* XXX should we report an error? */
	    return (NULL);
	}

	buff = buffp->buffs[buffp->i++%16];
    }
    if (prefix->family == AF_INET) {
	u_char *a;
	a = prefix_touchar (prefix);
	if (with_len) {
	    sprintf (buff, "%d.%d.%d.%d/%d", a[0], a[1], a[2], a[3],
		     prefix->bitlen);
	}
	else {
	    sprintf (buff, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	}
	return (buff);
    }
    else if (prefix->family == AF_INET6) {
	char *r;
	r = (char *) inet_ntop (AF_INET6, &prefix->add.sin6, buff,
				48 /* a guess value */ );
	if (r && with_len) {
	    sprintf (buff + strlen (buff), "/%d", prefix->bitlen);
	}
	return (buff);
    }
    else
	return (NULL);
}

void 
trace_prefix_list (char *msg, trace_t * ltrace, LINKED_LIST * ll_prefix)
{
    char buff[4][BUFSIZE];
    prefix_t *prefix = NULL;
    int n = 0;

    if (ll_prefix == NULL)
	return;

    LL_Iterate (ll_prefix, prefix) {
	n++;
	if (n == 4) {
	    sprintf (buff[n - 1], "%s/%d", prefix_toa (prefix),
		prefix->bitlen);
	    trace (TR_PACKET, ltrace, "%s %s %s %s %s\n",
		   msg, buff[0], buff[1], buff[2], buff[3]);
	    n = 0;
	    memset (buff[0], 0, BUFSIZE);
	    memset (buff[1], 0, BUFSIZE);
	    memset (buff[2], 0, BUFSIZE);
	    memset (buff[3], 0, BUFSIZE);
	}
	else {
	    sprintf (buff[n - 1], "%s/%d", prefix_toa (prefix),
		prefix->bitlen);
	}
    }

    if (n == 3)
	trace (TR_PACKET, ltrace, "%s %s %s %s\n",
	       msg, buff[0], buff[1], buff[2]);
    else if (n == 2)
	trace (TR_PACKET, ltrace, "%s %s %s\n",
	       msg, buff[0], buff[1]);
    else if (n == 1)
	trace (TR_PACKET, ltrace, "%s %s\n",
	       msg, buff[0]);
}

int
prefix_is_loopback (prefix_t *prefix)
{
    if (prefix->family == AF_INET)
	return (prefix_tolong (prefix) == htonl (0x7f000001));
#ifdef HAVE_IPV6
    if (prefix->family == AF_INET6)
	return (IN6_IS_ADDR_LOOPBACK (prefix_toaddr6 (prefix)));
#endif
    assert (0);
    return (0);
}

int 
comp_with_mask (void *addr, void *dest, u_int mask)
{

    if ( /* mask/8 == 0 || */ memcmp (addr, dest, mask / 8) == 0) {
	int n = mask / 8;
	int m = ((-1) << (8 - (mask % 8)));

	if (mask % 8 == 0 || (((u_char *)addr)[n] & m) == (((u_char *)dest)[n] & m))
	    return (1);
    }
    return (0);
}

int 
byte_compare (void *addr, void *dest, int bits, void *wildcard)
{
    int bytelen = bits / 8;
    int bitlen = bits % 8;
    int i, m;
    static u_char zeros[16];

    if (wildcard == NULL)
	wildcard = zeros;

    for (i = 0; i < bytelen; i++) {
        if ((((u_char *)addr)[i] | ((u_char *)wildcard)[i]) !=
            (((u_char *)dest)[i] | ((u_char *)wildcard)[i]))
	    return (0);
    }

    if (bitlen == 0)
	return (1);

    m = (~0 << (8 - bitlen));
    if (((((u_char *)addr)[i] | ((u_char *)wildcard)[i]) & m) !=
        ((((u_char *)dest)[i] | ((u_char *)wildcard)[i]) & m))
            return (0);

    return (1);
}

/* this allows imcomplete prefix */
int
my_inet_pton (int af, const char *src, void *dst)
{
    if (af == AF_INET) {
        int i, c, val;
        u_char xp[4] = {0, 0, 0, 0};

        for (i = 0; ; i++) {
	    c = *src++;
	    if (!isdigit(c))
		return (-1);
	    val = 0;
	    do {
		val = val * 10 + c - '0';
		if (val > 255)
		    return (0);
		c = *src++;
	    } while (c && isdigit(c));
            xp[i] = val;
	    if (c == '\0')
		break;
            if (c != '.')
                return (0);
	    if (i >= 3)
		return (0);
        }
	memcpy (dst, xp, 4);
        return (1);
    } else if (af == AF_INET6) {
        return (inet_pton (af, src, dst));
    } else {
	errno = EAFNOSUPPORT;
	return -1;
    }
}

#ifdef notdef
/* is_prefix
 * Scan string and check if this looks like a prefix. Return 1 if prefix,
 * 0 otherwise
 */
int is_ipv4_prefix (char *string) {
  char *cp, *last, *prefix, *len, *copy;
  int octet = 0;

  copy = strdup (string);
  cp = strtok_r (copy, "/", &last);
  prefix = cp;

  if ((cp != NULL) && ((len = strtok_r (NULL, "/", &last)) != NULL)) {
    if ((atoi (len) < 0) || (atoi (len) > 32)) {
      irrd_free(copy);
      return (-1);
    }
  }

  cp = strtok_r (prefix, ".", &last);
  
  while (cp != NULL) {
    octet++;
    if ((atoi (cp) < 0) || (atoi (cp) > 255)) {irrd_free(copy); return (-1);}
    cp = strtok_r (NULL, ".", &last);
  }

  if ((octet > 4) || (octet <= 0)) {irrd_free(copy); return (-1);}

  irrd_free(copy);
  return (1);
}
#else
/*
 * it's difficult to make sure that the above code works correctly.
 * instead, I'll take an easy way as follows: -- masaki
 */
int is_ipv4_prefix (char *string) {

  u_char dst[4];
  char *p, save[BUFSIZE];

  if ((p = strchr (string, '/')) != NULL) {
      strcpy (save, string);
      save [p - string] = '\0';
      string = save;
  }
  return (my_inet_pton (AF_INET, string, dst) > 0);
}
#endif

/* is_prefix
 * ask inet_pton() to see if this looks like a prefix. Return 1 if prefix,
 * 0 otherwise
 */
int is_ipv6_prefix (char *string) {

  u_char dst[16];
  char *p, save[BUFSIZE];

  if ((p = strchr (string, '/')) != NULL) {
      strcpy (save, string);
      save [p - string] = '\0';
      string = save;
  }
  return (inet_pton (AF_INET6, string, dst) > 0);
}
