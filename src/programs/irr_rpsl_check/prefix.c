/* 
 * $Id: prefix.c,v 1.3 2001/07/13 18:53:36 ljb Exp $
 * originally Id: prefix.c,v 1.1 1998/07/08 15:29:03 dogcow Exp 
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#include <ctype.h>
#include <defs.h>
#include <irr_rpsl_check.h>
#include <config.h>

#if defined(HAVE_DECL_INET_NTOP) && ! HAVE_DECL_INET_NTOP
const char *inet_ntop (int af, const void *src, char *dst, size_t size);
int inet_pton (int af, const char *src, void *dst);
#endif /* HAVE_INET_NTOP */

/* Given a short prefix (ie, no prefix length) or long prefix
 * (ie, prefix/length) return yes or no if 'string' is legal.
 * 'short_prefix' is a flag value describing 'string'.
 * First the prefix is checked and then the prefix length if 
 * it is a long prefix.  This routine *assumes* that the 
 * prefix has four well-formed octets/compponents from the
 * scanner.
 *
 * Return:
 *   1 if 'string' is well-formed
 *   0 otherwise
 */
int _is_ipv4_prefix (parse_info_t *obj, char *string, int short_prefix) {
  u_char dst[4];
  char *p = NULL, save[64];
  int val = 0, i, j;

  if ((p = strchr (string, '/')) != NULL) {
    if (short_prefix)
      return 0;

    strncpy (save, string, 64);
    j = p - string;
    if (j < 2 || j > 60) {
      error_msg_queue (obj, "Malformed prefix", ERROR_MSG);
      return 0;
    }
    save [j] = '\0';
    save [63] = '\0';  /* make sure we are null terminated */
    string = save;
  }
  else if (!short_prefix)
    return 0;

  /* check the ip part */
  if (irrd_inet_pton (AF_INET, string, dst) <= 0) {
    error_msg_queue (obj, "Malformed prefix", ERROR_MSG);
    return 0;
  }

  /* check the ip length part */
  if (!short_prefix) {
    for (i = 0; i < 3 && *++p; i++) {
      if (!isdigit (*p)) {
	error_msg_queue (obj, "Non-numeric value in prefix mask", ERROR_MSG);
	return 0;
      }
      else
	val = val * 10 + *p - '0';
    }

    /* don't want 005, 33, etc... */
    if (i == 0   ||
	i > 2    ||
	val > 32 ||
	(val < 10 && i == 2)) {
      error_msg_queue (obj, "Malformed prefix mask", ERROR_MSG);
      return 0;
    }
    
    /* check for prefix exceeding prefix mask */
    if ((j = (val / 8)) < 4) {
      val -= 8 * j;
      dst[j] <<= val;
      for (i = j; i < 4; i++) {
	if (dst[i] != 0) {
	  error_msg_queue (obj, "IP prefix exceeds prefix mask length", ERROR_MSG);
	  return 0;
	}
      }
    }
  }

  return 1;
}

/*
 * like _is_ipv4_prefix, except for V6 prefixes
 *
 * Return:
 *   1 if 'string' is well-formed
 *   0 otherwise
 */
int _is_ipv6_prefix (parse_info_t *obj, char *string, int short_prefix) {
  u_char dst[16];
  char *p = NULL, save[64];
  int val = 0, i, j;

  if ((p = strchr (string, '/')) != NULL) {
    if (short_prefix)
      return 0;

    strncpy (save, string, 64);
    j = p - string;
    if (j < 2 || j > 60) {
      error_msg_queue (obj, "Malformed prefix", ERROR_MSG);
      return 0;
    }
    save [j] = '\0';
    save [63] = '\0';  /* make sure we are null terminated */
    string = save;
  }
  else if (!short_prefix)
    return 0;

  /* check the ip part */
  if (irrd_inet_pton (AF_INET6, string, dst) <= 0) {
    error_msg_queue (obj, "Malformed prefix", ERROR_MSG);
    return 0;
  }

  /* check the ip length part */
  if (!short_prefix) {
    for (i = 0; i < 4 && *++p; i++) {
      if (!isdigit (*p)) {
	error_msg_queue (obj, "Non-numeric value in prefix mask", ERROR_MSG);
	return 0;
      }
      else
	val = val * 10 + *p - '0';
    }

    /* don't want 005, 129, etc... */
    if (i == 0   ||
	i > 3    ||
	val > 128 ||
	(val < 10 && i == 2) ||
	(val < 100 && i ==3) ) {
      error_msg_queue (obj, "Malformed prefix mask", ERROR_MSG);
      return 0;
    }
    
    /* check for prefix exceeding prefix mask */
    if ((j = (val / 8)) < 16) {
      val -= 8 * j;
      dst[j] <<= val;
      for (i = j; i < 16; i++) {
	if (dst[i] != 0) {
	  error_msg_queue (obj, "IP prefix exceeds prefix mask length", ERROR_MSG);
	  return 0;
	}
      }
    }
  }

  return 1;
}

/* this allows imcomplete prefix */ 
int irrd_inet_pton (int af, const char *src, void *dst) {
    if (af == AF_INET) {
        int i, c, val;
        u_char xp[4] = {0, 0, 0, 0};

        for (i = 0; ; i++) {
            c = *src++;
            if (!isdigit (c))
                return (-1);
            val = 0;
            do {
                val = val * 10 + c - '0';
                if (val > 255)
                    return (0);
                c = *src++;
            } while (c && isdigit (c));
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
