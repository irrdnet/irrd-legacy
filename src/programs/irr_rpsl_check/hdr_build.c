/* 
 * $Id: hdr_build.c,v 1.4 2001/07/13 18:53:35 ljb Exp $
 */

/*
 * All the routines in this module are designed to
 * build the header information that is used in
 * the pipeline communication between modules.
 *
 * Header structure:
 *
 * ... fill in when completely specified
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <irr_rpsl_check.h>

static char *append_list_attr (char *, char *);

/* JW must fix later 
int F_IR = -1, F_NH = -1, F_MN = -1;
*/

/* This routine concat's x and y, putting a space
 * beteen x and y.  It assumes that the
 * caller is placing the result in x, so the routine
 * free's x if it points to something.
 * 
 * The calling convention should be like this:
 * x = myconcat (x, y);
 *   x and/or y may be NULL and routine will work.
 */
char *my_concat (char *x, char *y, int make_space) {
  char buf[MAXLINE];

  if (x == NULL)
    buf[0] = '\0';
  else {
    strcpy (buf, x);
    free (x);
  }

  if (y != NULL) {
    if (buf[0] != '\0' && make_space)
      strcat (buf, " ");

    strcat (buf, y);
  }

  if (buf[0] == '\0')
    return NULL;
  else
    return strdup (buf);
}


char *append_list_attr (char *curr_list, char *new_val) {
  char *s, *p, *q;

  if (new_val == NULL)
    return curr_list;

  s = strdup (new_val);
  for (p = q = s; irrcheck_find_token (&p, &q) > 0;) {
    if (*(q - 1) == ',')
      *(q - 1) = '\0'; 
    curr_list = my_concat (curr_list, p, 1);
  }

  free (s);
  return curr_list;
}

void display_header_info (parse_info_t *hi) {
  fprintf (ofile, "%s\n", HDR_START);

  if (hi->errors)
    fprintf (ofile, "%s\n", SYNTAX_ERRORS);
  else if (hi->warns)
    fprintf (ofile, "%s\n", SYNTAX_WARNS);

  if (hi->type != NO_OBJECT)
    fprintf (ofile, "%s%s\n", OBJ_TYPE, obj_type[hi->type]);

  if (hi->obj_key != NULL) {
    fprintf (ofile, "%s%s", OBJ_KEY, hi->obj_key);
    if (hi->second_key != NULL)
      fprintf (ofile, " %s", hi->second_key);
    fprintf (ofile, "\n");
  }

  if (hi->op != NULL)
    fprintf (ofile, "%s%s\n", OP, hi->op); /* could be 'DEL'
					    * see syntax_attrs.c */

  if (hi->source != NULL)
    fprintf (ofile, "%s %s\n", SOURCE, hi->source);

  if (hi->mnt_by != NULL)
    fprintf (ofile, "%s %s\n", MNT_BY, hi->mnt_by);

  if (hi->override != NULL)
    fprintf (ofile, "%s %s\n", OVERRIDE, hi->override);

  if (hi->password != NULL)
    fprintf (ofile, "%s %s\n", PASSWORD, hi->password);

  if (hi->check_notify != NULL)
    fprintf (ofile, "%s %s\n", CHECK_NOTIFY, hi->check_notify);

  if (hi->check_mnt_nfy != NULL)
    fprintf (ofile, "%s %s\n", CHECK_MNT_NFY, hi->check_mnt_nfy);

  if (hi->keycertfn != NULL)
    fprintf (ofile, "%s %s\n", KEYCERTFN, hi->keycertfn);

  if (hi->cookies != NULL)
    fprintf (ofile, "%s", hi->cookies);


  fprintf (ofile, "%s\n", HDR_END);
}


void build_header_info (parse_info_t *obj, char *attr_val) {

  if (attr_val == NULL)
    return;

fprintf (dfile, "build_header_info () attr_val (%s)\n", attr_val);

  if (obj->obj_key == NULL   &&
      obj->type != NO_OBJECT &&
      obj->curr_attr >= 0    &&
      attr_is_key[obj->curr_attr] >= 0) {
    obj->obj_key = strdup (attr_val);
    return;
  }
  
  /* JW need to add back in when we add nic handles
  if ((obj->curr_attr == F_OR  ||
       obj->curr_attr == F_NH) &&
      obj->second_key == NULL) {
    obj->second_key = strdup (attr_val);
    return;
  }
  */
  if (obj->curr_attr == F_OR &&
      obj->second_key == NULL) {
    obj->second_key = strdup (attr_val);
    return;
  }

  if (obj->curr_attr == F_SO &&
      obj->source == NULL) {
    obj->source = strdup (attr_val);
    return;
  }

  if (obj->curr_attr == F_MB) {
    obj->mnt_by = append_list_attr (obj->mnt_by, attr_val);
    return;
  }

  if (obj->curr_attr == F_NY) {
    obj->check_notify = append_list_attr (obj->check_notify, attr_val);
    return;
  }

  /* JW need to add back in when we add maintainer objects
  if (obj->curr_attr == F_MN) {
    obj->check_mnt_nfy = append_list_attr (obj->check_mnt_nfy, attr_val);
    return;
  }
  */
}
