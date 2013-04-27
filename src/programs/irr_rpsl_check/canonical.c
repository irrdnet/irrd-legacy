/* 
 * $Id: canonical.c,v 1.10 2002/10/17 20:22:09 ljb Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <irr_rpsl_check.h>

extern short legal_attrs[MAX_OBJS][MAX_ATTRS];
extern char *attr_name[];
extern char string_buf[];
extern char *strp;
extern int int_size;
extern FILE *dfile;
extern int INFO_HEADERS_FLAG;
extern char parse_buf[];

static void swap (canon_info_t *canon_info, canon_line_t v[], int i, int j);
static void cqsort (canon_info_t *canon_info, canon_line_t v[], int left, int right);
static void init_attr_weights (canon_info_t *cn, parse_info_t *obj);
static void canon_put_str (canon_info_t *cn, char *strp, int str_len);
static FILE *flush_parse_buffer (canon_info_t *cn);
static void lput (long i, canon_line_t *lrec, canon_info_t *cn);
static int lget (long i, canon_line_t *lrec, canon_info_t *cn);
static void lappend (canon_line_t *lrec, canon_info_t *cn);
static void flush_line_info (parse_info_t *obj, canon_info_t *cn);
static void dump_current_canon_line (canon_info_t *ci);
static void new_line_record (canon_info_t *, parse_info_t *, canon_line_t *);

/* 
 * Empty/print the parse buffer to STDOUT.
 * The parse buffer could be a memory buffer or a file on disk.
 */
void display_canonicalized_object (parse_info_t *obj, canon_info_t *canon_info) {
  char buf[MAXLINE];
  int i, j, no_errors = 0;
  canon_line_t lrec;
  
  if (!canon_info->do_canon ||
      obj->num_lines == 0)
    return;
  
  /* Sort the object fields for proper ordering.
   * Don't sort if there are errors, else the
   * error line numbers will be incorrect.
   */

  if (!obj->errors && obj->type != NO_OBJECT) {
    init_attr_weights (canon_info, obj);
    if (obj->type == O_KC) {  /* check if "key-cert" */
      /* sort so that machine-generated attributes are not at the end */
      cqsort (canon_info, lineptr, 0, obj->num_lines - 1);
    }
    no_errors = 1;
  }

  /* prepend header information for pipeline communication */
  if (INFO_HEADERS_FLAG)
    display_header_info (obj);
  
  /* canon obj is in memory */
  if (canon_info->io == CANON_MEM) {
    if (canon_info->lio == CANON_MEM) {
      for (i = 0; i < obj->num_lines; i++) {
	if (no_errors) {
	  if (lineptr[i].wt >= SPEC_ATTR_WEIGHT)
	    break;
	  if (lineptr[i].skip_attr)
	    continue; 
	}
	fputs (lineptr[i].ptr, ofile);
      }
    }
    else {
      fseek (canon_info->lfd, 0, SEEK_SET);
      for (i = 0; i < obj->num_lines; i++) {
	fread (&lrec, sizeof (canon_line_t), 1, canon_info->lfd);
	if (no_errors) {
	  if (lrec.wt >= SPEC_ATTR_WEIGHT)
	    break;
	  if (lrec.skip_attr)
	    continue;
	}
	fputs (lrec.ptr, ofile);
      }      
    }
  }
  else { /* canon obj is on disk */
    if (canon_info->lio == CANON_MEM) { 
      for (i = 0; i < obj->num_lines; i++) {
	fseek (canon_info->fd, lineptr[i].fpos, SEEK_SET);
	if (no_errors) {
	  if (lineptr[i].wt >= SPEC_ATTR_WEIGHT)
	    break;
	  if (lineptr[i].skip_attr)
	    continue;
	}
	do { /* print a line */
	  if (fgets (buf, MAXLINE, canon_info->fd) == NULL)
	    break;
	  j = strlen (buf) - 1;
	  if (j < 0)
	    break;
	  fputs (buf, ofile);
	  if (buf[j] == '\n')
	    lineptr[i].lines--;	
	} while (lineptr[i].lines > 0);
      }
    }
    else {
      if (fseek (canon_info->lfd, 0, SEEK_SET))
        fprintf (dfile, "display_canon: IV fseek/rewind lfd error...\n");
      for (i = 0; i < obj->num_lines; i++) {
	fread (&lrec, sizeof (canon_line_t), 1, canon_info->lfd);
	if (no_errors) {
	  if (lrec.wt >= SPEC_ATTR_WEIGHT)
	    break;
	  if (lrec.skip_attr)
	    continue;
	}
        if (ferror (canon_info->lfd))
          fprintf (dfile, "fread (lfd) read error\n");
        if (verbose) fprintf (dfile, "seeking fpos (%ld)\n", lrec.fpos);
	if (fseek (canon_info->fd, lrec.fpos, SEEK_SET))
          fprintf (dfile, "IV seek error fpos (%ld)\n", lrec.fpos);
	do { /* print a line */
	  if (fgets (buf, MAXLINE, canon_info->fd) == NULL)
	    break;
	  j = strlen (buf) - 1;
	  if (j < 0)
	    break;
	  if (EOF == fputs (buf, ofile))
            fprintf (dfile, "IV fputs () error...\n");
	  if (buf[j] == '\n')
	    lrec.lines--;
	} while (lrec.lines > 0);
      }      
    }
  }
}

/*
 * The next series of routines all put text into the parse buffer:
 *
 * add_canonical_error_line ()
 * canonicalize_key_attr ()
 * do_lc_canoncal ()
 * 
 */

/* Write out the error causing line verbatim (ie, as the user typed it in) and put
 * the error symbol '<?>' next to the error causing token.  The error line is in
 * the file pointed to by 'canon_info->efd'.  Because of line continuation '\' and
 * as-in/as-out, the line is saved to file as it can be arbitrarily large.  The 
 * basic steps here are to reset the canonicalize buffer back to the beginning or 
 * the line and write over any partially canonicalized line with the error line.  
 * Next the position within the line to put the error symbol is calculated and then 
 * the error line is written to the canonicalize buffer.
 *
 * Return:
 *   void
 */
void add_canonical_error_line (parse_info_t *obj, canon_info_t *canon_info, int add_line_feed) {
  int i, j, k, l;
  char c;
  canon_line_t lrec;

/*
  if (verbose)
*/
    fprintf (dfile, "1 error-pos (%d) line-len (%d) num_lines (%d) \n", obj->error_pos, obj->eline_len, obj->num_lines);

  if (!canon_info->do_canon)
    return;
  
  /* Reset the parse buffer pointer to the beginning of 
   * the line to overwrite any fragment of the canonicalized
   * line we have already parsed.  Want to write the error 
   * line verbatim with the error symbol at the error token 
   * position.
   */
  dump_current_canon_line (canon_info);

  /* reset the number of lines */
  if (canon_info->lio == CANON_MEM)
    lineptr[obj->num_lines].lines = obj->elines;
  else {
    lget (obj->num_lines, &lrec, canon_info);
    lrec.lines = obj->elines;
    lput (obj->num_lines, &lrec, canon_info);
  }

  /* Make the error symbol '<?>' abut the nearest token.
   * Find the position j to put '<?>'.  Remember, because
   * of '\' line continuation, we can have many '\n's in
   * a single error line.
   */
  if (fseek (canon_info->efd, 0, SEEK_SET) != EOF) {
    j = l = -1;
    i = obj->error_pos;
    k = 0;
    for (;j < 0 || l < 0 || (l <= j); k++) {
      if (k >= obj->eline_len                  ||
	  (c = fgetc (canon_info->efd)) == EOF ||
	  c == '\0')
	break;
      else if (c != ' ' && c != '\t' && c != '\n')
	j = k;
      else if ((j >= 0 && (j == (k - 1)) && k == i) ||
	       k > i)
	l = k;
    }
    
    if (j >= 0)
      j++;
    else
      j = 0;

    /* JW debug */
fprintf (dfile, "\nerror_line (): error_pos (%d) j (%d) l (%d) k (%d)\n", obj->error_pos, j, l, k);
    fseek (canon_info->efd, 0, SEEK_SET);
    for (i = 0; i < obj->eline_len && (c = fgetc (canon_info->efd)) != EOF; i++) {
      if (i % 10 == 0)
	fprintf (dfile, "|");
      fprintf (dfile, "%c", c);
    }
fprintf (dfile, "\n i (%d)\n", i);
/* JW end debug */

    /* write out the error line, inserting '<?>' at position j */
    fseek (canon_info->efd, 0, SEEK_SET);
    for (i = 0; i < obj->eline_len && (c = fgetc (canon_info->efd)) != EOF; i++) {
      if (i == j)
	parse_buf_add (canon_info, "%s", ERROR_TOKEN);
      parse_buf_add (canon_info, "%c", &c);
    } 

  }
  else {
    parse_buf_add (canon_info, "%s", "INTERNAL ERROR: could not read error line!\n");
    add_line_feed = 0;
  }


  if (add_line_feed)
    parse_buf_add (canon_info, "\n");

  /* terminate the line */
  parse_buf_add (canon_info, "%z");

  return;
}

/*============================================================*/



void rpsl_lncont (parse_info_t *obj, canon_info_t *canon_info, int add_line_feed) {
  int i, j = 0, state = 0;
  char c, d;
  canon_line_t lrec;

  /*
  if (verbose)
  */
    fprintf (dfile, "1 error-pos (%d) line-len (%d) num_lines (%d) \n", obj->error_pos, obj->eline_len, obj->num_lines);

  if (!canon_info->do_canon)
    return;
  
  /* Reset the parse buffer pointer to the beginning of 
   * the line to overwrite any fragment of the canonicalized
   * line we have already parsed. 
   */
  dump_current_canon_line (canon_info);

  /* reset the number of lines */
  if (canon_info->lio == CANON_MEM)
    lineptr[obj->num_lines].lines = obj->elines;
  else {
    lget (obj->num_lines, &lrec, canon_info);
    lrec.lines = obj->elines;
    lput (obj->num_lines, &lrec, canon_info);
  }

  if (fseek (canon_info->efd, 0, SEEK_SET) != EOF) {
    /* put in the attribute ID */
    canonicalize_key_attr (obj, canon_info, 1);
    for (i = 0; i < obj->eline_len && 
	   (c = fgetc (canon_info->efd)) != EOF; i++) {
      if (state == 0) {                   /* initial state, skip past attr */
	if (c == ':')
	  state = 1;
      }
      else if (state == 1) {              /* at the ATTR_ID_LEN, 
					   * get rid of spaces */
	if (c == ' ' || c == '\t') continue;

	parse_buf_add (canon_info, "%c", &c);
	state = 2;
      }
      else if (state == 2) {              /* print non-blank characters */
	if (c == ' ' || c == '\t') { /* go to string trim state */
	  j = 1;
	  state = 3;
	  continue;
	}

	if (c == '\n')
	  state = 4;

	parse_buf_add (canon_info, "%c", &c);
      }
      else if (state == 3) {         /* string trim, remove trailing blanks */
	if (c == ' ' || c == '\t') {
	  j++;
	  continue;
	}

	if (c == '\n')
	  state = 4;
	else {                            /* print non-blank char and spaces */
	  d = ' ';
	  for (; j > 0; j--)
	    parse_buf_add (canon_info, "%c", &d);
	  state = 2;
	}
	parse_buf_add (canon_info, "%c", &c);
      }
      else if (state == 4) { /* print line continuation char 
			      * and pad with space */
	parse_buf_add (canon_info, "%c", &c);
	c = ' ';
	for (j = 1;(ATTR_ID_LENGTH - j) > 0; j++)
	  parse_buf_add (canon_info, "%c", &c);
	state = 1;
      }	
    }
  }
  else {
    parse_buf_add (canon_info, "%s", "INTERNAL ERROR: could not read error line!\n");
    add_line_feed = 0;
  }


  if (add_line_feed)
    parse_buf_add (canon_info, "\n");

  /* terminate the line */
  parse_buf_add (canon_info, "%z");

  return;
}

/* 
 * Print the attribute identifier at the beginning of the line
 */
void canonicalize_key_attr (parse_info_t *obj, canon_info_t *canon_info,
			    int lncont) {
  char buf[64];
  int num_spaces;
  canon_line_t lrec;

  /*
fprintf (stderr,"canonicalize_key_attr: attr=(%s)\n", attr_name[obj->curr_attr]);
  */
  if (!canon_info->do_canon)
    return;

  num_spaces = ATTR_ID_LENGTH - strlen (attr_name[obj->curr_attr]) - 1;

  if (num_spaces > 0)
    sprintf(buf, "%%s:%%b%d", num_spaces);    
  else
    sprintf(buf, "%%s:");
  parse_buf_add (canon_info, buf, attr_name[obj->curr_attr]);

  /* if we are doing line continuation then exit 
   * as we have already done the next part
   */
  if (lncont)
    return;

  /* init the curr attribute type and count for this field */
  if (canon_info->lio == CANON_MEM) {
    lineptr[obj->num_lines].attr  = obj->curr_attr;
    lineptr[obj->num_lines].count = obj->attrs[obj->curr_attr] + 1;
  }
  else {
    lget (obj->num_lines, &lrec, canon_info);
    lrec.attr = obj->curr_attr;
    lrec.count = obj->attrs[obj->curr_attr] + 1;
    lput (obj->num_lines, &lrec, canon_info);
  }
}


void parse_buf_add (canon_info_t *canon_obj, char *format, ...) {
  char *p, *q;
  int i, num_spaces;
  va_list ap;

  if (!canon_obj->do_canon)
    return;

if (verbose && canon_obj->io == CANON_DISK) fprintf (dfile, "parse_buf_add:fpos (%ld)\n", ftell (canon_obj->fd));
if (verbose) fprintf (dfile, "parse_buf_add:format (%s)\n", format);

  va_start (ap, format);
  
  for (p = format; *p; p++) {
    if (verbose) fprintf (dfile, "$%c$\n", *p);
    if (*p != '%') {
      if (canon_obj->io == CANON_MEM &&
	  canon_obj->buf_space_left <= 1)
	canon_obj->fd = flush_parse_buffer (canon_obj);

      if (canon_obj->io == CANON_MEM) {
	canon_obj->buf_space_left--;
	*canon_obj->bufp++ = *p;
      }
      else
	fputc (*p, canon_obj->fd);

      continue;
    }
    switch (*++p) {
    case 's': /* print string */
      q = va_arg (ap, char *);
      if (q != NULL) { /* ignore NULL strings */
	i = strlen (q);
	canon_put_str (canon_obj, q, i + 1);
      }
      break;
    case 'b': /* print spaces/blanks */
      /* first calculate the number of spaces */
      num_spaces = 0;
      for (q = ++p; *q >= '0' && *p <= '9'; q++)
        num_spaces = 10 * num_spaces + (*q - '0');
      p = --q; /* p get's incremented by the outer for loop */
      if (verbose) fprintf (dfile, "print blanks (%d) \n", num_spaces);
      if (canon_obj->io == CANON_MEM &&
	  canon_obj->buf_space_left <= num_spaces)
	canon_obj->fd = flush_parse_buffer (canon_obj);

      /* now print the spaces */
      if (canon_obj->io == CANON_MEM) {
	for (i = 0; i < num_spaces; i++)
	  *canon_obj->bufp++ = ' ';
	canon_obj->buf_space_left -= num_spaces;
      }
      else {
	if (verbose) fprintf (dfile, "before spaces fpos (%ld)\n", ftell (canon_obj->fd));
	for (i = 0; i < num_spaces; i++)
	  fputc (' ', canon_obj->fd);
	if (verbose) fprintf (dfile, "after spaces fpos (%ld)\n", ftell (canon_obj->fd));
      }
      break;
    case 'c': /* print a character */
      q = va_arg (ap, char *);
      if (canon_obj->io == CANON_MEM &&
	  canon_obj->buf_space_left <= 1)
	canon_obj->fd = flush_parse_buffer (canon_obj);

      if (canon_obj->io == CANON_MEM) {
	canon_obj->buf_space_left--;
	*canon_obj->bufp++ = *q;
      }
      else
	fputc (*q, canon_obj->fd);
      break;
    case 'z': /* print a string terminator/zero byte */
      /* want mem file buffer strings terminated by a NULL byte 
       * so that fputs () stops at end of line/end of lines can be
       * detected
       */
      if (canon_obj->io == CANON_MEM &&
	  canon_obj->buf_space_left <= 1)
	canon_obj->fd = flush_parse_buffer (canon_obj);

      if (canon_obj->io == CANON_MEM) {
	  *canon_obj->bufp++ = '\0';
	  canon_obj->buf_space_left--;
      }
      /* JW take out
      else
        fputc ('\0', canon_obj->fd))
	*/
      break;
    default:
      if (canon_obj->io == CANON_MEM &&
	  canon_obj->buf_space_left <= 1)
	canon_obj->fd = flush_parse_buffer (canon_obj);

      if (canon_obj->io == CANON_MEM) {
	*canon_obj->bufp++ = *p;
	canon_obj->buf_space_left--;
      }
      else
	fputc (*p, canon_obj->fd);
      break;
    }
  }

 va_end (ap);

if (verbose && canon_obj->io == CANON_DISK) fprintf (dfile, "parse_buf_add:fpos (%ld)\n", ftell (canon_obj->fd));

/* 
if (verbose)
  fprintf (dfile, "exit: parse buf siz (%d) space left (%d)\n", canon_obj->bufp - parse_buf, canon_obj->buf_space_left);
  */
}

/* 
 * Put the NULL terminated string pointed to by *strp 
 * into the canonical object buffer.
 */
void canon_put_str (canon_info_t *cn, char *strp, int str_len) {

  if (cn->io == CANON_MEM && cn->buf_space_left <= str_len)
    cn->fd = flush_parse_buffer (cn);
  
  if (cn->io == CANON_MEM) {
    memcpy (cn->bufp, strp, str_len);
    if (verbose)
      fprintf (dfile, "canon_put_str mem (%s) offset (%ld): exit\n", strp, cn->bufp - parse_buf);
    cn->bufp += str_len - 1; /* lex tokens are '\0' terminated str's */
    cn->buf_space_left -= str_len - 1;
  }
  else{
    if (verbose) fprintf (dfile, "canon_put_str disk (%s) fpos (%ld) ", strp, ftell(cn->fd));
    if (fputs (strp, cn->fd) == EOF && verbose)
      fprintf (dfile, "canon_put_str () error (%s)\n", strerror (errno));
    if (verbose) fprintf (dfile, "fpos (%ld)\n", ftell(cn->fd));
  }
}


/* Start a new canonical line which means to buffer the canonical object
 * to disk if the number of lines exceeds the maximum buffer capacity and 
 * initialize a new line information record with inital values.
 *
 * Return:
 *  void
 */
void start_new_canonical_line (canon_info_t *ci, parse_info_t *pi) {
  canon_line_t lrec;

  if (!ci->do_canon)
    return;

  if (ci->lio == CANON_MEM && pi->num_lines >= CANON_LINES_MAX) {
    if (verbose)
      fprintf (dfile, "(%d lines) Switching canon lineptr info to disk\n", CANON_LINES_MAX);
    flush_line_info (pi, ci);
  }
  
  /* set a new line record with initial values */
  if (ci->lio == CANON_MEM)
    new_line_record (ci, pi, &lineptr[pi->num_lines]);
  else
    new_line_record (ci, pi, &lrec);
}

/* Initialize a new line record.  After the object has been parsed there
 * will be an extra line record which does not point to an object line.
 * So the display routine must take this into account and stop processing
 * at the last line record.  It's easier to do things this way because the 
 * program is not sure at a lines end if there will be another line in 
 * the file or EOF.
 * 
 * Return:
 *   void
 */
void new_line_record (canon_info_t *ci, parse_info_t *pi, canon_line_t *lrec) {
  
  lrec->attr       = F_NOATTR;
  lrec->count      = 0;
  lrec->lines      = 1; /* each "line" can have multiple lines due to '\' */
  lrec->skip_attr = 0;
  
  /* set the beginning of line pointer */
  if (ci->io == CANON_MEM)
    lrec->ptr = ci->linep = ci->bufp;
  else {
    lrec->fpos = ci->flinep = ftell (ci->fd);
    if (verbose) 
      fprintf (dfile, "new_line_record (): num_lines (%d) beg line disk (%ld)\n", pi->num_lines, lrec->fpos);
  }
  
  if (ci->lio == CANON_DISK)
      lappend (lrec, ci);
}

void set_skip_attr (canon_info_t *ci, parse_info_t *pi) {
  canon_line_t lrec;

  if (ci->lio == CANON_DISK) {
    lget (pi->num_lines, &lrec, ci);
    lrec.skip_attr = 1;
    lput (pi->num_lines, &lrec, ci);
  }
  else
    lineptr[pi->num_lines].skip_attr = 1;
}

/* Reset the canonicalize next token pointer to the beginning 
 * of line. The effect will be to erase/expunge the current
 * canonicalized line and the next token added to the canoncialize 
 * buffer will be added to the beginning of the line.
 *
 * Return:
 *   void
 */
void dump_current_canon_line (canon_info_t *ci) {

  if (ci->io == CANON_MEM) {
    /* reset the space left pointer, add back
     * the space from the current line
     */
    ci->buf_space_left += (ci->bufp - ci->linep);
    ci->bufp = ci->linep;
  }
  else {
    if (fseek (ci->fd, ci->flinep, SEEK_SET))
      fprintf (dfile, "ERROR: seek beginning of line on disk buffer...\n");
    if (verbose) fprintf (dfile, "dump_current_line (): exit disk fpos (%ld)\n", ftell (ci->fd));
  }
}

void cqsort (canon_info_t *cn, canon_line_t v[], int left, int right) {
  int i, last;
  canon_line_t line1, line2;
  
  if (left >= right)
    return;
  
  swap (cn, v, left, (left + right)/2);
  
  last = left;
  for (i = left + 1; i <= right; i++) {
    if (cn->lio == CANON_MEM) {
      if (v[i].wt < v[left].wt)
	swap (cn, v, ++last, i);
    }
    else {
      lget (i, &line1, cn);
      lget (left, &line2, cn);
      if (line1.wt < line2.wt)
	swap (cn, v, ++last, i);
    }
  }

  swap   (cn, v, left, last);
  cqsort (cn, v, left, last - 1);
  cqsort (cn, v, last + 1, right);
}

void swap (canon_info_t *cn, canon_line_t v[], int i, int j) {
  canon_line_t temp, line2;

  if (cn->lio == CANON_MEM) {
    temp = v[i];
    v[i] = v[j];
    v[j] = temp;
  }
  else {
    lget (i, &temp, cn);
    lget (j, &line2, cn);
    lput (j, &temp, cn);
    lput (i, &line2, cn);
  }
}

/* Object type may not be known until last field in object
 * is parsed.
 */
void init_attr_weights(canon_info_t *cn, parse_info_t *obj) {
  float x;
  long fpos;
  int i;
  canon_line_t lrec;
  
  if (verbose) fprintf (dfile, "\nJW: assigning wts.....\n");
  if (cn->lio == CANON_MEM) { /* line ptr info is in memory */
    for (i = 0; i < obj->num_lines; i++) {
      x = legal_attrs[obj->type][lineptr[i].attr];
      lineptr[i].wt = x - (1.0 / (float) lineptr[i].count);
      /*
fprintf (stderr, "(%d) %s: init wt (%f) count (%d) wt (%f)\n",lineptr[i].attr, attr_name[lineptr[i].attr],x, lineptr[i].count, lineptr[i].wt);
*/
    }
  }
  else {
    fseek (cn->lfd, 0, SEEK_SET);
    for (i = 0; i < obj->num_lines; i++) {
      fpos = ftell (cn->lfd);
      fread (&lrec, sizeof (canon_line_t), 1, cn->lfd);
      x = legal_attrs[obj->type][lrec.attr];
      lrec.wt = x - (1.0 / (float) lrec.count);
      fseek (cn->lfd, fpos, SEEK_SET);
      fwrite (&lrec, sizeof (canon_line_t), 1, cn->lfd);
      fflush (cn->lfd);
    }
  }
}

/* Our memory parse buffer is not large enough to hold the
 * canonicalized object, flush it to disk.  Function opens up
 * a temp parse file 'flushfn' to hold the object.  Each of 
 * the lines in the memory buffer are copied to the disk buffer.
 *
 * Return:
 *   pointer to the disk streams pointer.
 */
FILE *flush_parse_buffer (canon_info_t *cn) {
  char *p;
  int i, fd;
  FILE *fp;
  canon_line_t lrec;

  if (cn->fd != NULL) {
    fprintf (dfile, "Overflow buffer called a second time.  Abort!\n");
    exit (0);
  }
  
  strcpy (cn->flushfn, cn->flushfntmpl);
  fd = mkstemp (cn->flushfn);  
  if ((fp = fdopen (fd, "w+")) == NULL) {
    fprintf (stderr, "Cannot open disk parse buffer (%s).  Abort!\n", cn->flushfn);
    exit (0);
  }

  if (verbose)
    fprintf (dfile, "*****flush buffer to disk****\n:%s:\n",cn->buffer);

  /* Make sure last char is a NULL byte, we 
   * could be in the middle of a string 
   */
  *cn->bufp = '\0'; 

  if (cn->lio == CANON_DISK)
    fseek (cn->lfd, 0, SEEK_SET);
  
  for (i = 0, p = cn->buffer; p != NULL && p < cn->bufp; i++) {
    cn->flinep = ftell (fp);
    if (cn->lio == CANON_MEM)
      lineptr[i].fpos = cn->flinep;
    else {
      lget (i, &lrec, cn);
      lrec.fpos = cn->flinep;
if (verbose) fprintf (dfile, "flush buf line pos (%ld)\n", lrec.fpos);
      lput (i, &lrec, cn);
    }
    fputs (p, fp);
    if ((p = strchr (p, '\0')) != NULL)
      p++;
  }
  
  if (verbose)
    fprintf (dfile, "flush_parse_buffer () number of lines flushed to disk (%d)\n", i);
  
  /* set the begining of line pointer (ie, cn->flinep) */
  if (cn->linep == cn->bufp) { /* we are at the beginning of a line  */
    cn->flinep = ftell (fp);
    if (verbose) fprintf (dfile, "flush buf: we are at the beginning of a line...\n");
    if (cn->lio == CANON_MEM)
      lineptr[i].fpos = cn->flinep;
    else {
      lget (i, &lrec, cn);
      lrec.fpos = cn->flinep;
      if (verbose) fprintf (dfile, "flush buf line pos (%ld)\n", lrec.fpos);
      lput (i, &lrec, cn);
    }
  }

  cn->io = CANON_DISK;
  cn->buf_space_left = 0;
  return fp;
}

void flush_line_info (parse_info_t *obj, canon_info_t *cn) {
  long i;
  int fd;

  strcpy (cn->lflushfn, cn->flushfntmpl);
  fd = mkstemp (cn->lflushfn);  
  if ((cn->lfd = fdopen (fd, "w+")) == NULL) {
    fprintf (stderr, "Cannot open disk line info buffer (%s).  Abort!\n", cn->lflushfn);
    exit (0);
  }

  if (verbose)
    fprintf (dfile, "*****flush line buffer to disk****\n:%s:\n", cn->lflushfn);  
  fseek (cn->lfd, 0, SEEK_SET);
  for (i = 0; i < obj->num_lines; i++) {
    if (verbose)
      fprintf (dfile, "line (%ld): (%ld)\n", i, ftell (cn->lfd));
    fwrite (&lineptr[i], sizeof (canon_line_t), 1, cn->lfd);
  }

  if (verbose)
    fprintf (dfile, "*****exit flush line buf, wrote (%d) lines fpos (%ld)...\n", 
	     obj->num_lines, ftell (cn->lfd));

  cn->lio = CANON_DISK;
}

int lget (long i, canon_line_t *lrec, canon_info_t *cn) {

  if (fseek (cn->lfd, i * sizeof (canon_line_t), SEEK_SET))
    return -1;

  fread (lrec, sizeof (canon_line_t), 1, cn->lfd);
  return 1;
}

void lput (long i, canon_line_t *lrec, canon_info_t *cn) {

  fseek (cn->lfd, i * sizeof (canon_line_t), SEEK_SET);
  fwrite (lrec, sizeof (canon_line_t), 1, cn->lfd);
}

void lappend (canon_line_t *lrec, canon_info_t *cn) {

  fseek (cn->lfd, 0, SEEK_END);
  fwrite (lrec, sizeof (canon_line_t), 1, cn->lfd);
}
