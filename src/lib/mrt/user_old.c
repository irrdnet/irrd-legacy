/* 
 * $Id: user_old.c,v 1.2 2001/07/13 18:02:45 ljb Exp $
 */

/* routines that will hopefully go away soon.... */

#include <mrt.h>
#include <user.h>

char *
uii_parse_line (char **line)
{
    char *word;
    THREAD_SPECIFIC_STORAGE (word);
    return (uii_parse_line2 (line, word));
}

int 
parse_line (char *line, char *format,...)
{
    va_list ap;
    void **arg;
    int *intarg;
    char *chararg;
    char *token, *fcp;
    u_int match, l;
    char word[BUFSIZE];

    int state;

    prefix_t *prefix;

    match = 0;
    state = 0;
    va_start (ap, format);

    fcp = format;

    while (*line && isspace (*line))
	line++;

    for ( ; *fcp != '\0'; fcp++) {

	/* eat up spaces */
	if (isspace (*fcp)) {
	    while (*line && isspace (*line))
		line++;
	    continue;
	}

	if (*line == '\n' || *line == '\0')
	    return (match);

	/* literal */
	if (*fcp != '%') {

	    if (tolower (*fcp) != tolower (*line))
		return (match);
	    while (*fcp && !isspace (*fcp) && 
			tolower (*fcp) == tolower (*line)) {
		/*printf ("\n%s %s", *fcp, *line); */
		fcp++;
		line++;
	    }
	    if ((isspace (*line) || *line == '\n' || *line == '\0') &&
		 (isspace (*fcp) || *fcp == '\0')) {
		match++;
		fcp--;
		continue;
	    }
	    else {
		return (match);
	    }
	}

	/* argument */
	else {
	    fcp++;
	    if (*fcp == 'p') {
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
parse_p_option:
		if (strchr (token, ':'))
		    goto finish;	/* assuming it ipv6 addr */
		if (!strchr (token, '.'))
		    goto finish;
		if ((prefix = ascii2prefix (AF_INET, token)) == NULL)
		    goto finish;
		match++;
		arg = va_arg (ap, void **);
		*arg = prefix;
	    }
	    else if (*fcp == 'P') {
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
parse_P_option:
		if (!strchr (token, ':'))
		    goto finish;	/* assuming it non ipv6 addr */
		if ((prefix = ascii2prefix (AF_INET6, token)) == NULL)
		    goto finish;
		match++;
		arg = va_arg (ap, void **);
		*arg = prefix;
	    }
	    else if (*fcp == 'm') { /* %p or %P */
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
parse_m_option:
		if (strchr (token, ':')) {
		    /* assuming it ipv6 addr */
		    if ((prefix = ascii2prefix (AF_INET6, token)) == NULL)
		        goto finish;
		}
		else if (strchr (token, '.')) {
		    if ((prefix = ascii2prefix (AF_INET, token)) == NULL)
		        goto finish;
		}
		else {
		    goto finish;
		}
		match++;
		arg = va_arg (ap, void **);
		*arg = prefix;
	    }
	    else if (*fcp == 'a') { /* %p without /xx */
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		if (strchr (token, '/'))
		    goto finish;
		goto parse_p_option;
	    }
	    else if (*fcp == 'A') { /* %A without /xx */
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		if (strchr (token, '/'))
		    goto finish;
		goto parse_P_option;
	    }
	    else if (*fcp == 'M') { /* %p or %P  without /xx */
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		if (strchr (token, '/'))
		    goto finish;
		goto parse_m_option;
	    }
	    else if (*fcp == 'n') { /* a name that starts with alpha and 
				  alpha-num follows */
		int i;
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		if (!isalpha (token[0]))
		    goto finish;
		for (i = 1; token[i]; i++) {
		    if (!isalnum (token[i]))
			goto finish;
		}
		chararg = va_arg (ap, char *);
		strcpy (chararg, token);
		match++;
	    }
	    else if (*fcp == 'd') {
		int i;

		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		for (i = 0; token[i]; i++) {
		    if (!isdigit (token[i]))
			goto finish;
		}
		intarg = va_arg (ap, int *);
		l = atol (token);
		memcpy (intarg, &l, 4);
		match++;
	    }
	    else if (*fcp == 'i') {
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		intarg = va_arg (ap, int *);
		if (sscanf (token, "%i", &l) != 1)
		    goto finish;
		memcpy (intarg, &l, 4);
		match++;
	    }
	    else if (*fcp == 's') {
		if ((token = uii_parse_line2 (&line, word)) == NULL)
		    goto finish;
		chararg = va_arg (ap, char *);
		strcpy (chararg, token);
		match++;
	    }
            /* directories like /var/spool */
            else if (*fcp == 'q') {
	        if ((token = uii_parse_line2 (&line, word)) == NULL) 
		    goto finish;
	        /* this may be so strict */
                if (!isalpha (*token) && (*token != '/')) 
		    goto finish; 
	         chararg = va_arg (ap, char*);
	         strcpy (chararg, token);
	         match++;
            } 
	    /* gobble up everything to the end of the line */
	    else if (*fcp == 'S') {
		/* if ((token = uii_parse_line2 (&line, word)) == NULL) 
		       {return (-1);} */
		chararg = va_arg (ap, char *);
		strcpy (chararg, line);
		match++;
		return (match);
	    }
	    else {
		assert (0);
	    }
	}
    }

  finish:
    return (match);
}
