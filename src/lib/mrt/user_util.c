/* 
 * $Id: user_util.c,v 1.7 2001/08/07 20:04:29 ljb Exp $
 */

#include <mrt.h>
#include <user.h>

#if 0
static void add_last_token (char *ctoken, char *last_ctoken, char *last_utoken,
			    LINKED_LIST *ll_last);
#endif

/* uii_tokenize
 * tokenize a command string (e.g. "show ip access")
 */
LINKED_LIST *uii_tokenize (char *buffer, int len) {
  char *command;
#ifdef notdef
#ifdef HAVE_STRTOK_R
  char *last, *utoken;
#endif /* HAVE_STRTOK_R */
#endif
  LINKED_LIST *ll;
  char *save, *cp;
  int in_quote =0;

  /* I want to use Delete in place of free, but it's a macro. */
  ll = LL_Create (LL_DestroyFunction, free, 0);

  command = alloca (len + 1);
  /* tokenize uii string */
  memcpy (command, buffer, len);
  command[len] = '\0';

#ifdef notdef
#ifdef HAVE_STRTOK_R
  utoken = strtok_r (command, " ", &last);
  while (utoken != NULL) {
    LL_Add (ll, strdup (utoken));
    utoken = strtok_r (NULL, " ", &last);
  }
#endif /* HAVE_STRTOK_R */
#endif


  save = cp = command;
  while (*cp != '\0') {
    while ((in_quote || !isspace (*cp)) && (*cp != '\0') && (*cp != '\n')) {
      if (*cp == '\"') {
	if (in_quote == 0) 
	  in_quote = 1;
	else
	  in_quote = 0;
      }
      cp++;
    }

    if ((*cp == '\0') || (*cp == '\n')) {
      *cp = '\0';
      LL_Add (ll, strdup (save));      
      save = cp;
    }
    else {
      *cp = '\0';
      LL_Add (ll, strdup (save));      
      cp++;
      save = cp;
    }
  }

  if (*save != '\0') {
    LL_Add (ll, strdup (save));          
  }


  /*Delete (command);*/
  return (ll);
}



/* uii_tokenize
 * tokenize a command string (e.g. "show ip access")
 */
LINKED_LIST *uii_tokenize_choices2 (char *buffer, int len, LINKED_LIST *ll) {
  char *command, *utoken, *cp;
#ifdef HAVE_STRTOK_R
  char *last;
#endif /* HAVE_STRTOK_R */

  if (ll == NULL)
      ll = LL_Create (LL_DestroyFunction, free, 0);
  command = alloca (len + 1);
  memcpy (command, buffer, len);
  command[len] = '\0';

  if (*buffer != '(' && *buffer != '[') {
    LL_Add (ll, strdup (command));
    return (ll);
  }
  /* remove closing paren */
  command[len - 1] = '\0';

  /* tokenize uii string */
  cp = command +1;

  utoken = strtok_r (cp, "|", &last);
  while (utoken != NULL) {
    LL_Add (ll, strdup (utoken));
   if (command[0] == '[')
	return (ll);
    utoken = strtok_r (NULL, "|", &last);
  }

  return (ll);
}

LINKED_LIST *uii_tokenize_choices (char *buffer, int len) {
    return (uii_tokenize_choices2 (buffer, len, NULL));
}


/* uii_token_match, nomatch -> 0, partial match -> 1, exact match -> 2
 */
int uii_token_match (char *ctoken, char *user_token) {
  char *cp, *buf;
  int ret = 2;

  /* optional */
  if (ctoken[0] == '[') {
    cp = buf = strdup ((ctoken +1));
    cp[strlen(cp)-1] = '\0'; /* get rid of terminating ] */
    if ((cp = strchr (cp, '|')) != NULL)
	*cp++ = '\0';
    
    ret = uii_token_match (buf, user_token);
    Delete (buf);
    return (ret);
  }

  /* match one in the set */
  if (ctoken[0] == '(') {
#ifdef HAVE_STRTOK_R
    char *last;
#endif /* HAVE_STRTOK_R */
    int okay = 0;
    cp = buf = strdup ((ctoken +1));
    cp[strlen(cp)-1] = '\0'; /* get rid of terminating ) */
    cp = strtok_r (cp, "|", &last);
    
    while (cp != NULL) {
      if ((ret = uii_token_match (cp, user_token))) {
	if (okay < ret)
	    okay = ret;
	break;
      }
      cp = strtok_r (NULL, "|", &last);
    }

    Delete (buf);
    return (ret);
  }


  if (ctoken[0] == '%') {
    int i;

    switch (ctoken[1]) {
    case 'p': /* ipv4 prefix */
      if (is_ipv4_prefix (user_token))
	return (ret);
      break;
    case 'P': 
      if (is_ipv6_prefix (user_token)) 
	return (ret);
      break;
    case 'a': /* ipv4 prefix */
      if (is_ipv4_prefix (user_token) && !strchr (user_token, '/'))
	return (ret);
      break;
    case 'A': 
      if (is_ipv6_prefix (user_token) && !strchr (user_token, '/')) 
	return (ret);
      break;
    case 'M': 
      if (strchr (user_token, '/'))
	return (0);
    /* FALL THROUGH */
    case 'm': 
      if (is_ipv4_prefix (user_token))
	return (ret);
      if (is_ipv6_prefix (user_token)) 
	return (ret);
      break;
    case 'n': /* name */
      if (!isalpha (user_token[0]))
	return (0);
      for (i = 1; user_token[i]; i++) {
        if (!isalnum (user_token[i]))
	    return (0);
      }
      return (ret);
    case 's': /* any string */
      return (1); /* not exact matching? */
    case 'D': /* integer (1..MAX_ALIST) */
      for (i = 0; user_token[i]; i++) {
        if (!isdigit (user_token[i]))
	    return (0);
      }
      i = atoi (user_token);
      if (i > 0 && i < MAX_ALIST)
        return (ret);
      break;
    case 'd': /* integer */
      for (i = 0; user_token[i]; i++) {
        if (!isdigit (user_token[i]))
	    return (0);
      }
      return (ret);
    default:
      break;
    }
    return (0);
  }

  /* a literal */
  if (strcasecmp (ctoken, user_token) == 0)
    return (ret);

  /* a literal */
  if (strncasecmp (ctoken, user_token, strlen (user_token)) == 0) 
    return (1);

  return (0);
}


/* find_matching_commands
 */
LINKED_LIST *find_matching_commands (int state, LINKED_LIST *ll_tokens,
				     LINKED_LIST *ll_last, int alone) {
  char *utoken, *ctoken;
  LINKED_LIST *ll_match;
  LINKED_LIST *ll_exact = NULL;
  int best_point = 0; /* exact +2, partial +1, no  +0, remain -1 */
  uii_command_t *uii_command;
  char *last_ctoken = NULL, *last_utoken = NULL;
  int skip = 0;
  int optional;
  int point;

  ll_match = LL_Create (0);
  /* help */
  if (ll_last == NULL)
	ll_exact = LL_Create (0);

  /* okay, now run through the list of commands and see who matches most */
  LL_Iterate (UII->ll_uii_commands, uii_command) {
    if (state >= UII_NORMAL && uii_command->state == UII_ALL) {
	/* OK */
    }
    else if (state >= UII_CONFIG && uii_command->state == UII_CONFIG_ALL) {
	/* OK */
    }
    else if (state == UII_ENABLE && uii_command->state == UII_NORMAL) {
	/* OK */
    }
    else if (state != uii_command->state)
      continue;

    last_ctoken = ctoken = LL_GetHead (uii_command->ll_tokens);
    last_utoken = utoken = LL_GetHead (ll_tokens);
    skip = 0;

    /* for password and comments -- see if any of the first part of command matches */
    if ((utoken != NULL) && (uii_command->flag & COMMAND_MATCH_FIRST)) {
      if (!strncasecmp (uii_command->string, utoken, strlen (uii_command->string))) {
	 LL_Add (ll_match, uii_command);
	 continue;
      }
    }

    /* command may be prefaced with a no -- special case */
    /* if ((utoken != NULL) && (!strcasecmp ("no", utoken)))*/
    /* last_utoken = utoken = LL_GetNext (ll_tokens, utoken);*/

    optional = 0;
    point = 0;
    while ((utoken != NULL) && (ctoken != NULL)) {
	int ret;

      /* accept everything else to end of line*/
      if (strncmp (ctoken, "%S", 2) == 0) {
        last_ctoken = ctoken;
        last_utoken = utoken;
        /* ctoken = NULL;*/ /* shouldn't follow anything */
        utoken = NULL; /* simulate eating up to the last */
	break;
      }
      
	ret = uii_token_match (ctoken, utoken);
	point += ret;
      /* optional command */
      if (ctoken[0] == '[') {
	if (ret == 0)
	  goto keep_utoken;
      }
      /* mandatory command */
      else if (ret == 0) {
	skip = 1;
	break;
      }

      last_utoken = utoken;
      utoken = LL_GetNext (ll_tokens, utoken);
    keep_utoken:
      last_ctoken = ctoken;
      ctoken = LL_GetNext (uii_command->ll_tokens, ctoken);
    }

    if (skip == 1) continue;
    if ((utoken != NULL) && (ctoken == NULL)) continue;

    if (ctoken) {
	char *pivot = ctoken;
	/* scan through to the end to see if there is mandatory one */
	while (pivot) {
	    if (pivot[0] != '[' && strncmp (pivot, "%S", 2) != 0)
		point--; /* XXX */
	    pivot = LL_GetNext (uii_command->ll_tokens, pivot);
	}
    }

    if (ll_exact) {
	if (best_point < point) {
	    best_point = point;
	    if (LL_GetCount (ll_exact) > 0)
	        LL_Clear (ll_exact);
	}
	if (best_point == point)
	    LL_Add (ll_exact, uii_command);
    }
    LL_Add (ll_match, uii_command);

    if (ll_last == NULL)
      continue;

    if (alone) {
        assert (utoken == NULL);
        if (ctoken == NULL) {
            LL_Add (ll_last, strdup ("<cr>"));
        }
	else if (ctoken[0] == '[') {
      	    LINKED_LIST *ll = LL_Create (LL_DestroyFunction, free, 0);
	    int total = 0;
	    char *str, *concat;
	    
	    uii_tokenize_choices2 (ctoken, strlen (ctoken), ll);
	    do {
	        ctoken = LL_GetNext (uii_command->ll_tokens, ctoken);
		if (ctoken == NULL) {
            	    LL_Add (ll, strdup ("<cr>"));
		    break;
		}
	        uii_tokenize_choices2 (ctoken, strlen (ctoken), ll);
	    } while (ctoken[0] == '[');

	    LL_Iterate (ll, str) {
		if (total != 0)
		    total++; /* for '|' */
		total += strlen (str);
	    }

	    concat = NewArray (char, total + 2 + 1 +40);
	    total = 0;
	    concat[total++] = '(';
	    LL_Iterate (ll, str) {
		if (total != 0) {
		    concat[total++] = '|';
		}
		strcpy (concat + total, str);
		total += strlen (str);
	    }
	    concat[total++] = ')';
	    concat[total] = '\0';
	    LL_Add (ll_last, concat);
	}
	else {
	    LL_Add (ll_last, strdup (ctoken));
	}
    }
    else {
        assert (last_ctoken);
	LL_Add (ll_last, strdup (last_ctoken));
    }
  }

  if (ll_exact && LL_GetCount (ll_exact) > 0) {
	assert (ll_last == NULL);
	LL_Destroy (ll_match);
	return (ll_exact);
  }
  else {
	if (ll_exact)
	    LL_Destroy (ll_exact);
	return (ll_match);
  }
}


#ifdef notdef
static void add_last_token (char *ctoken, char *last_ctoken, char *last_utoken,
			    LINKED_LIST *ll_last) {

  /* done with current command -- get next */
  if ((last_utoken == NULL) ||
      ((last_ctoken != NULL) && 
       ((!strcmp (last_ctoken, last_utoken)) || (last_ctoken[0] == '%')))) {
    if (ctoken != NULL)
      LL_Add (ll_last, strdup (ctoken));
    else
      LL_Add (ll_last, strdup ("<cr>"));
    return;
  }

  if (last_ctoken == NULL) {
    LL_Add (ll_last, strdup ("<cr>"));
    return;
  }
      
  LL_Add (ll_last, strdup (last_ctoken));
  return;
}
#endif


int 
uii_send_buffer_del (uii_connection_t * uii, buffer_t *buffer)
{
    int ret;
    ret = uii_send_buffer (uii, buffer);
    Delete_Buffer (buffer);
    return (ret);
}


/* uii_send_data
 * send formatted data out a socket
 */
int 
uii_send_data (uii_connection_t * uii, char *format, ...)
{
    va_list ap;
    buffer_t *buffer;

    assert (uii != NULL);
    if (uii->sockfd_out < 0 && uii->sockfd < 0)
	return (-1);

    /* assert (uii->schedule->self == pthread_self ()); */
    buffer = New_Buffer (0);
    va_start (ap, format);
    buffer_vprintf (buffer, format, ap);
    va_end (ap);
    return (uii_send_buffer_del (uii, buffer));
}


int 
uii_send_buffer (uii_connection_t * uii, buffer_t *buffer)
{
    char *newline;
    char *cp;

    assert (uii != NULL);
    assert (buffer != NULL);
    if (uii->sockfd_out < 0 && uii->sockfd < 0)
	return (-1);

    if (uii->schedule->self && (uii->schedule->self != pthread_self ())) {
	buffer_t *temp = New_Buffer (buffer_data_len (buffer));
	/* when called from other threads, copy the buffer */
	buffer_puts (buffer_data (buffer), temp);
	/* schedule myself to be run by my own thread */
	schedule_event2 ("uii_send_buffer_del", uii->schedule, 
			 (event_fn_t) uii_send_buffer_del, 2, uii, temp);
	return (0);
    }

    cp = buffer_data (buffer);

    while ((newline = strchr (cp, '\n'))) {
        int len = newline - cp + 1;
	if (len >= 2 && cp[len-2] == '\r') {
            if (uii->sockfd_out >= 0) {
	        if (send (uii->sockfd_out, cp, len-2, 0) < len-2 ||
		    send (uii->sockfd_out, "\n", 1, 0) < 1) {
		    uii->sockfd_out = -1; /* turn off redirection */
		    return (-1);
	        }
            }
            else if (uii->sockfd >= 0) {
		if (send (uii->sockfd, cp, len, 0) < len) {
		    uii_destroy_connection (uii);
		    return (-1);
	        }
    	    }
	}
	else {
    	    if (uii->sockfd_out >= 0) {
		if (send (uii->sockfd_out, cp, len, 0) < len) {
		    uii->sockfd_out = -1; /* turn off redirection */
		    return (-1);
	        }
            }
            else if (uii->sockfd >= 0) {
	        if (send (uii->sockfd, cp, len-1, 0) < len-1 ||
	            send (uii->sockfd, "\r\n", 2, 0) < 2) {
		    uii_destroy_connection (uii);
		    return (-1);
	        }
    	    }
	}
	cp = newline + 1;
    }
    if (uii->sockfd_out >= 0) {
	int len = strlen (cp);
	if (send (uii->sockfd_out, cp, len, 0) < len) {
	    uii->sockfd_out = -1; /* turn off redirection */
	    return (-1);
	}
    }
    else if (uii->sockfd >= 0) {
	int len = strlen (cp);
        if (send (uii->sockfd, cp, len, 0) < len) {
	    uii_destroy_connection (uii);
	    return (-1);
        }
    }
    return (1);
}


/* user_notice
 * Convenience function to send error message both to log file
 * and out to uii
 */
void
user_vnotice (int trace_flag, trace_t *tr, uii_connection_t *uii, char *format,
	      va_list args)
{
    char *tmp1, *tmp2, *cp1, *cp2;
    buffer_t *buffer = New_Buffer (0);

    buffer_vprintf (buffer, format, args);
    tmp1 = buffer_data (buffer);
    tmp2 = alloca (buffer_data_len (buffer) * 2 + 1);
    /* tmp2 needs double for the worst case */

    /* we should take a more good way to handle a "\r\n" for a console
       just a "\n" for a file -- masaki */
    for (cp2 = tmp2, cp1 = tmp1; *cp1; cp1++) {
	if (*cp1 == '\r')
	    continue;
	*cp2++ = *cp1;
    }
    *cp2 = '\0';
    trace (trace_flag, tr, "%s", tmp2);

    for (cp2 = tmp2, cp1 = tmp1; *cp1; cp1++) {
	if (*cp1 == '\n' && (cp1 <= tmp1 || cp1[-1] != '\r'))
	    *cp2++ = '\r';
	*cp2++ = *cp1;
    }
    *cp2 = '\0';
    if (uii->sockfd >= 0)
      uii_send_data (uii, "%s", tmp2);
    Delete_Buffer (buffer);
}


void
user_notice (int trace_flag, trace_t *tr, uii_connection_t *uii, 
	     char *format, ...)
{
    va_list args;

    va_start (args, format);
    user_vnotice (trace_flag, tr, uii, format, args);
    va_end (args);
}
