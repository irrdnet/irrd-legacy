#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>

#include "hdr_comm.h"

extern trace_t *default_trace;

int verbose;

/* local yokel's */

static u_int dup_token_field (char *line, trans_info_t *ti);
static u_int is_from_field (char *line, trans_info_t *ti);
static u_int is_list_field (trace_t *tr, char *line, trans_info_t *ti);
static u_int int_type_field (char *line, trans_info_t *ti);
static u_int list_members_field (trace_t *tr, char *line, trans_info_t *ti);
static int get_list_members (trace_t *tr, char *p, char *addr_buf, char **next);
static int present_in_list (char *list, char *last, char *target_str);
static int is_valid_op (char *);
static u_int is_new_list_field (trace_t *tr, char *line, trans_info_t *ti);
static char *astrconcat (char *x, char *y, int add_space);

/* Action is to initialize the trans_info struct
 * with the header information located at the begining
 * of the transaction and to mark the fields that were encountred.
 * Routine assumes the file pos pointer is on the field immediately 
 * after the 'HDR-START' field.
 *
 * Return:
 *    0 'HDR-END' is encountered
 *    1 no 'HDR-END' field found
 */
/* JW still need to put in checks for buffer overflow */
int parse_header (trace_t *tr, FILE *fin, long *offset, trans_info_t *ti) {
  char buf[MAXLINE];
  char *cp;
  u_int hdr_f, hdr_flds = 0;

  /* clear the hdr structure */
  memset ((char *) ti, 0, sizeof (*ti));
  ti->nnext = ti->notify_addrs;
  ti->fnext = ti->forward_addrs;
  ti->snext = ti->sender_addrs;

  while ((cp = fgets (buf, MAXLINE - 1, fin)) != NULL) {
    /*fprintf (stderr, "parse_header () line offset (%ld): %s", *offset, buf);*/
    
    /* exit loop when HDR-END field is encountered */
    if (!memcmp (HDR_END, buf, strlen (HDR_END))) {
      hdr_flds |= HDR_END_F;
      break;
    }

    if (!(hdr_f = dup_token_field (buf, ti))           &&
	!(hdr_f = list_members_field (tr, buf, ti))    &&
	!(hdr_f = int_type_field (buf, ti))            &&
	!(hdr_f = is_from_field (buf, ti))             &&
	!(hdr_f = is_list_field (tr, buf, ti))         &&
	!(hdr_f = is_new_list_field (tr, buf, ti))) {
      /* Yikes!  We should never get here. */
      trace (ERROR, tr, 
	     "ERROR: parse_header () illegal header field encountered \"%s\"\n", buf);
      continue;
    }
    
    hdr_flds |= hdr_f;
  }
  
  ti->hdr_fields = hdr_flds;
  *offset = ftell (fin);

  /* 'cp' is NULL means no 'HDR-END' found */
  return (cp == NULL);
}

/* 
 * Dup the token pointed to by *p
 */
char *dup_single_token (char *p) {
  char *q = p;

  for (;*q == ' ' || *q == '\t'; q++);
  newline_remove (q);

  if (*q == '\0')
    return NULL;

  return strdup (q);
}

/* Given a '"' delimeted string, return everything between
 * the '"'s, minus the "'s.
 *
 * Return:
 *   char * string of the char's between "'s
 *   NULL otherwise
 */
char *dup_string_field (char *p) {
  char *q, *r;

  if ((q = strchr (p, '"')) == NULL)
    return NULL;

  if ((r = strrchr (++q, '"')) == NULL)
    return NULL;

  *r = '\0';

  return strdup (q);
}

u_int dup_token_field (char *line, trans_info_t *ti) {

  if (!strncmp (SOURCE, line, strlen (SOURCE)) &&
      (ti->source = dup_single_token (line + strlen (SOURCE))) != NULL)
    return SOURCE_F;
  
  if (!strncmp (OBJ_TYPE, line, strlen (OBJ_TYPE)) &&
      (ti->obj_type = dup_single_token (line + strlen (OBJ_TYPE))) != NULL)
    return OBJ_TYPE_F;
  
  if (!strncmp (OBJ_KEY, line, strlen (OBJ_KEY)) &&
      (ti->obj_key = dup_single_token (line + strlen (OBJ_KEY))) != NULL)
    return OBJ_KEY_F;
  
  if (!strncmp (OP, line, strlen (OP)) &&
      (ti->op = dup_single_token (line + strlen (OP))) != NULL &&
      is_valid_op (ti->op))
    return OP_F;

  /* subject line can be blank */
  if (!strncmp (SUBJECT, line, strlen (SUBJECT))) {
    ti->subject = dup_single_token (line + strlen (SUBJECT));
    return SUBJECT_F;
  }

  if (!strncmp (DATE, line, strlen (DATE)) &&
      (ti->date = dup_single_token (line + strlen (DATE))) != NULL)
    return DATE_F;

  if (!strncmp (MSG_ID, line, strlen (MSG_ID)) &&
      (ti->msg_id = dup_single_token (line + strlen (MSG_ID))) != NULL)
    return MSG_ID_F;

  if (!strncmp (PGP_KEY, line, strlen (PGP_KEY)) &&
      (ti->pgp_key = dup_single_token (line + strlen (PGP_KEY))) != NULL)
    return PGP_KEY_F;

  if (!strncmp (OVERRIDE, line, strlen (OVERRIDE)) &&
      (ti->override = dup_single_token (line + strlen (OVERRIDE))) != NULL)
    return OVERRIDE_F;

  if (!strncmp (KEYCERTFN, line, strlen (KEYCERTFN)) &&
      (ti->keycertfn = dup_single_token (line + strlen (KEYCERTFN))) != NULL)
    return KEYCERTFN_F;

  if (!strncmp (PASSWORD, line, strlen (PASSWORD)) &&
      (ti->crypt_pw = dup_string_field (line + strlen (PASSWORD))) != NULL)
    return PASSWORD_F;

  if (!strncmp (OTHERFAIL, line, strlen (OTHERFAIL)) &&
      (ti->otherfail = dup_single_token (line + strlen (OTHERFAIL))) != NULL)
    return OTHERFAIL_F;

  if (!strncmp (OLD_OBJ_FILE, line, strlen (OLD_OBJ_FILE)) &&
      (ti->old_obj_fname = 
       dup_single_token (line + strlen (OLD_OBJ_FILE))) != NULL)
    return OLD_OBJ_FILE_F;

  return 0;
}

u_int is_from_field (char *line, trans_info_t *ti) {
  char *p;

  if (!strncmp (FROM, line, strlen (FROM))) {
    p = line + strlen(FROM);
    for (;*p == ' ' || *p == '\t'; p++);
    newline_remove (p);      
    if (*p != '\0')
      strcpy (ti->sender_addrs, p);
    else
      strcpy (ti->sender_addrs, TCP_USER);

    ti->snext += strlen (ti->sender_addrs) + 1;

    return FROM_F;
  }

  return 0;
}

u_int is_new_list_field (trace_t *tr, char *line, trans_info_t *ti) {
  char *p, *q, **r;
  enum HDR_FLDS_T fld;

  if (!strncmp (CHECK_NOTIFY, line, strlen (CHECK_NOTIFY))) {
    p = line + strlen (CHECK_NOTIFY);
    r = &(ti->check_notify);
    fld = CHECK_NOTIFY_F;
  }
  else if (!strncmp (CHECK_MNT_NFY, line, strlen (CHECK_MNT_NFY))) {
    p = line + strlen (CHECK_MNT_NFY);
    r = &(ti->check_mnt_nfy);
    fld = CHECK_MNT_NFY_F;
  }
  else
    return 0;

  *r = NULL;
  q = p;
  while (find_token (&p, &q) > 0) {
    *q = '\0';
    *r = myconcat (*r, p);
    *q = ' ';
  }

  if (*r == NULL)
    return 0;

  return fld;
}

u_int is_list_field (trace_t *tr, char *line, trans_info_t *ti) {
  char *p;

  if (!strncmp (MAINT_NO_EXIST, line, strlen (MAINT_NO_EXIST))) {
    ti->maint_no_exist = NULL;
    p = line + strlen (MAINT_NO_EXIST);
    if ((p = strtok (p, " ")) == NULL) {
      fprintf (stderr, "ERROR: empty MAINT_NO_EXIST hdr field\n");
      return 0;
    }
    
    while (p != NULL) {
      if (*p != '\n')
	ti->maint_no_exist = myconcat (ti->maint_no_exist, p);
      p = strtok (NULL, " ");
    }
    
    if (ti->maint_no_exist != NULL)
      return MAINT_NO_EXIST_F;	
  }

  if (!strncmp (MNT_BY, line, strlen (MNT_BY))) {
    ti->mnt_by[0] = '\0';
    p = line + strlen (MNT_BY);
    if ((p = strtok (p, " ")) == NULL) {
      fprintf (stderr, "ERROR: empty MNT_BY hdr field\n");
      return 0;
    }
    
    while (p != NULL) {
      strcat (ti->mnt_by, " ");
      strcat (ti->mnt_by, p);
      p = strtok (NULL, " ");
    }
    
    newline_remove(ti->mnt_by);
    if (ti->mnt_by[0] != '\0')
      return MNT_BY_F;	
  }

  return 0;
}

u_int int_type_field (char *line, trans_info_t *ti) {

  if (!memcmp (SYNTAX_ERRORS, line, strlen (SYNTAX_ERRORS)))
    return ((u_int)(ti->syntax_errors = SYNTAX_ERRORS_F));

  if (!memcmp (SYNTAX_WARNS, line, strlen (SYNTAX_WARNS)))
    return ((u_int)(ti->syntax_warns = SYNTAX_WARNS_F));
  
  if (!memcmp (DEL_NO_EXIST, line, strlen (DEL_NO_EXIST)))
    return ((u_int)(ti->del_no_exist = DEL_NO_EXIST_F));
  
  if (!memcmp (AUTHFAIL, line, strlen (AUTHFAIL)))
    return ((u_int)(ti->authfail = AUTHFAIL_F));
  
  if (!memcmp (NEW_MNT_ERROR, line, strlen (NEW_MNT_ERROR)))
    return ((u_int)(ti->new_mnt_error = NEW_MNT_ERROR_F));

  if (!memcmp (DEL_MNT_ERROR, line, strlen (DEL_MNT_ERROR)))
    return ((u_int)(ti->del_mnt_error = DEL_MNT_ERROR_F));

  if (!memcmp (BAD_OVERRIDE, line, strlen (BAD_OVERRIDE)))
    return ((u_int)(ti->bad_override = BAD_OVERRIDE_F));

  if (!memcmp (UNKNOWN_USER, line, strlen (UNKNOWN_USER)))
    return ((u_int)(ti->unknown_user = UNKNOWN_USER_F));
  
  
  return 0;
}

u_int list_members_field (trace_t *tr, char *line, trans_info_t *ti) {

  if (!memcmp (NOTIFY, line, strlen (NOTIFY)) &&
      get_list_members (tr, line + strlen (NOTIFY), 
			ti->notify_addrs, &(ti->nnext)))
    return NOTIFY_F;
  
  if (!memcmp (MNT_NFY, line, strlen (MNT_NFY)) &&
      get_list_members (tr, line + strlen (MNT_NFY), 
			ti->notify_addrs, &(ti->nnext)))
    return MNT_NFY_F;
  
  if (!memcmp (UPD_TO, line, strlen (UPD_TO)) &&
      get_list_members (tr, line + strlen (UPD_TO), 
			ti->forward_addrs, &(ti->fnext)))
    return UPD_TO_F;

  return 0;
}

/* return index of element in list or return -1 */
int present_in_list (char *list, char *last, char *target_str) {
  char *p;
  int i = 0;

  for (p = list; p < last; p += strlen (p) + 1, i++)
    if (!strcasecmp (target_str, p))
      return i;

  return -1;
}

/*
 * Get the list of notifiers (ie, list of email addr's) and
 * place into the buffer pointed to by *next
 * (ie, forward or notify buffer).
 *   Return 1 if field is non-empty, else return 0.
 */
int get_list_members (trace_t *tr, char *p, char *addr_buf, char **next) {
  char *q;

  if ((q = strtok (p, " ")) == NULL) {
    fprintf (stderr, "ERROR: empty list hdr field\n");
    return 0;
  }
  
  while (q != NULL) {
    newline_remove(q);
    if (*q != '\0' &&
	present_in_list (addr_buf, *next, q) < 0) {
      strcpy (*next, q);
      *next += strlen (q) + 1;
    }
      q = strtok (NULL, " ");
  }

  return 1;
}

void free_ti_mem (trans_info_t *ti) {

  if (ti->subject != NULL)
    free (ti->subject);

  if (ti->date != NULL)
    free (ti->date);

  if (ti->msg_id != NULL)
    free (ti->msg_id);

  if (ti->source != NULL)
    free (ti->source);

  if (ti->obj_type != NULL)
    free (ti->obj_type);

  /* causing memory problems... chl */
  if (ti->obj_key != NULL) 
    free (ti->obj_key);

  if (ti->op != NULL)
    free (ti->op);

  if (ti->check_notify != NULL)
    free (ti->check_notify);

  if (ti->check_mnt_nfy != NULL)
    free (ti->check_mnt_nfy);
}
    
void print_hdr_struct (FILE *fout, trans_info_t *ti) {
  char *p;
  
  fprintf (fout, "%s\n", HDR_START);
  
  /*
    fprintf (stderr, "trans_code-(%d)\n", ti->trans_success);
    */
  
  /* pgp and email processing */

  if (ti->sender_addrs[0] != '\0')
    fprintf (fout, "%s%s\n", FROM, ti->sender_addrs);
  
  if (ti->date != NULL)
    fprintf (fout, "%s%s\n", DATE, ti->date);
  
  if (ti->subject != NULL)
    fprintf (fout, "%s%s\n", SUBJECT, ti->subject);
  
  if (ti->msg_id != NULL)
    fprintf (fout, "%s%s\n", MSG_ID, ti->msg_id);
  
  if (ti->pgp_key != NULL)
    fprintf (fout, "%s%s\n", PGP_KEY, ti->pgp_key);
  
  /* irr_check */

  if (ti->op != NULL)
    fprintf (fout, "%s%s\n", OP, ti->op);
  
  if (ti->obj_type != NULL)
    fprintf (fout, "%s%s\n", OBJ_TYPE, ti->obj_type);
  
  if (ti->obj_key != NULL)
    fprintf (fout, "%s%s\n", OBJ_KEY, ti->obj_key);
  
  if (ti->source != NULL)
    fprintf (fout, "%s%s\n", SOURCE, ti->source);
  
  if (ti->syntax_errors)
    fprintf (fout, "%s\n", SYNTAX_ERRORS);
  
  if (ti->syntax_warns)
    fprintf (fout, "%s\n", SYNTAX_WARNS);
  
  if (ti->mnt_by[0] != '\0')
    fprintf (fout, "%s%s\n", MNT_BY, ti->mnt_by);
  
  if (ti->override != NULL)
    fprintf (fout, "%s%s\n", OVERRIDE, ti->override);

  if (ti->keycertfn != NULL)
    fprintf (fout, "%s%s\n", KEYCERTFN, ti->keycertfn);

  if (ti->crypt_pw != NULL)
    fprintf (fout, "%s\"%s\"\n", PASSWORD, ti->crypt_pw);

  /* irr_auth */

  if (ti->otherfail != NULL)
    fprintf (fout, "%s%s\n", OTHERFAIL, ti->otherfail);

  if (ti->authfail)
    fprintf (fout, "%s\n", AUTHFAIL);
    
  if (ti->del_no_exist)
    fprintf (fout, "%s\n", DEL_NO_EXIST);
  
  if (ti->maint_no_exist != NULL)
    fprintf (fout, "%s%s\n", MAINT_NO_EXIST, ti->maint_no_exist);
  
  if (ti->old_obj_fname != NULL)
    fprintf (fout, "%s%s\n", OLD_OBJ_FILE, ti->old_obj_fname);

  if (ti->notify != NULL)
    fprintf (fout, "%s%s\n", NOTIFY, ti->notify);

  if (ti->forward != NULL)
    fprintf (fout, "%s%s\n", UPD_TO, ti->forward);

  if (ti->new_mnt_error)
    fprintf (fout, "%s\n", NEW_MNT_ERROR);

  if (ti->del_mnt_error)
    fprintf (fout, "%s\n", DEL_MNT_ERROR);

  if (ti->bad_override)
    fprintf (fout, "%s\n", BAD_OVERRIDE);

  if (ti->unknown_user)
    fprintf (fout, "%s\n", UNKNOWN_USER);

  fprintf (fout, "%s\n", HDR_END);

  
  return;

  fprintf (fout, "NOTIFY---\n");
  if (ti->nnext != ti->notify_addrs) {
    p = ti->notify_addrs;
    for (;p < ti->nnext; p += strlen (p) + 1)
      fprintf (fout, "  %s", p);
    fprintf (fout, ":\n");
  }
  
  fprintf (fout, "FORWARD---\n");
  if (ti->fnext != ti->forward_addrs) {
    p = ti->forward_addrs;
    for (; p < ti->fnext; p += strlen (p) + 1)
      fprintf (fout, "  %s", p);
    fprintf (fout, ":\n");
  }

  fprintf (fout, "----------\n");
}

/*
 * See if *op is a valid operation 
 */
int is_valid_op (char *op) {

  if (strcmp (op, ADD_OP)     &&
      strcmp (op, DEL_OP)     &&
      strcmp (op, REPLACE_OP) &&
      strcmp (op, NOOP_OP))
    return 0;
  
  return 1;
}

char *free_mem (char *p) {

  if (p != NULL)
    free (p);
  
  return NULL;
}

/* Given the transaction struct for this update, determine if there
 * are any errors.  Updates that do not have errors will be sent to
 * IRRd in a !us...!ue DB update.
 *
 * Return:
 *   1 if any error is found
 *   0 if no errors were found
 */
int update_has_errors (trans_info_t *ti) {

  return (ti->hdr_fields & ERROR_FLDS);
}

/* This routine finds a token in the string.  *x will
 * point to the first character in the string and *y will
 * point to the first character after the token.  A token
 * is a printable character string.  A '\n' is not considered
 * part of a legal token.  
 *   This function is rpsl-capable.  It will look for '#'s in
 * the string and assume everything after is a comment.
 * Invoke this routine by setting 'x' and 'y' to the beginning
 * of the target string like this: 'x = y = target_string;
 * if (find_token (&x, &y) < 0) ...
 * each successive call to find_token () will move the pointer
 * along.
 *
 * Return:
 *   1 if a token is found (*x points to token, *y first token after)
 *  -1 if no token is found (*x and *y are to be ignored)
 */
int find_token (char **x, char **y) {

  /* It's possible the target string is NULL 
   * or we are in a rpsl comment 
   */
  if (*y == NULL || **y == '#')
    return -1;

  *x = *y;
  /* fprintf (stderr, "p-(%c)\n",**x); */
  /* find the start of a token, ie, first printable character */
  while (**x == ' ' || **x == '\t' || **x == '\n') (*x)++;

  if (**x == '\0' || **x == '#')
    return -1;

  /* find the first space at the end of the token */
  *y = *x + 1;
  while (**y != '\0' && (isgraph ((int) **y) && **y != '#')) (*y)++;

  return 1;  
}

/* This routine concat's x and y, optionally  putting a space
 * beteen x and y.  It assumes that the
 * caller is placing the result in x, so the routine
 * free's x if it points to something.
 * 
 * The calling convention should be like this:
 * x = astrconcat (x, y, add_space);
 *   x and/or y may be NULL and routine will work.
 *   if add_space is 1, a space is added in between strings
 */
char *astrconcat (char *x, char *y, int add_space) {
  char buf[MAXLINE];
  int lenx = 0, total_len;

  if (x == NULL)
    buf[0] = '\0';
  else {
    lenx = strlen(x);
    if (lenx >= MAXLINE) {
        trace(ERROR, default_trace, "myconcat: Length exceeds buffer size - %d\n", MAXLINE);
        return NULL;
    }
    strcpy (buf, x);
    free_mem (x);
  }

  if (y != NULL) {
    total_len = lenx + strlen(y);
    if (add_space)
       total_len++;
    if (total_len >= MAXLINE) {
        trace(ERROR, default_trace, "myconcat: Length exceeds buffer size - %d\n", MAXLINE);
        return NULL;
    }
    if (add_space && buf[0] != '\0') {
      buf[lenx++] = ' ';
    }
    strcpy (buf + lenx, y);
  }

  if (buf[0] == '\0')
    return NULL;
  else
    return strdup (buf);
}

/* concat strings, sticking a space in between */
char *myconcat (char *x, char *y) {

  return astrconcat(x, y, 1);
}

/* same as above, buy don't stick a space between strings */
char *myconcat_nospace (char *x, char *y) {

  return astrconcat(x, y, 0);
};
