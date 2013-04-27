/************************************************************************
 *** Statusfiles intentionally look like the old Windows ini file     ***
 *** format.  The right thing to do if this functionality is ported   ***
 *** to Windows is to replace most of this code with a wrapper to the ***
 *** appropriate calls to use the registry.                           ***
 ***                                                                  ***
 *** The files contain sections delimited by '[section name]' on a    ***
 *** line by itself.  Individual lines are of the form                ***
 *** 'variable=value' Strings are quoted in double quotes.  We're     ***
 *** allowing quote characters to be escaped by the backslash         ***
 *** character.  A semicolon outside of a string is considered a      ***
 *** comment until EOL.  An implicit strtrim is done on every read.   ***
 ***                                                                  ***
 *** Comments are NOT preserved, nor is line ordering.  This is       ***
 *** different from the windows ini routines.  There isn't enough     ***
 *** reason to add in this functionality at the moment.  (If we don't ***
 *** tell the users, they wont try to use comments...)                ***
 ***                                                                  ***
 *** All public functions return 1 on success, 0 on failure.  The     ***
 *** global string STATUSFILE_ERROR will contain error text, if any.  ***
 ***                                                                  ***
 *** All variables are stored in hashes.  Essentially we have a hash  ***
 *** of hashes, the first hash containing the sections hashes, the    ***
 *** second being standard var/value pairs.                           ***
 ************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <mrt.h>
#include <glib.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif /* HAVE_LIBGEN_H */

#include "irrd.h"

/**
 ** Constants
 **/

#define HASHSIZE_SECTIONS 101
#define HASHSIZE_VARS     47

#ifdef TESTJMH
trace_t *default_trace;
irr_t   IRR;
#endif

void add_value_to_list(gpointer key, gpointer value, GList *data);

/**********************************************************************
 **
 ** Private Functions
 **
 **********************************************************************/

/**
 ** String functions
 **/

static int
StrTrim (char *buf)
{
  char *p;

  /* Strip trailing whitespace */
  p = buf + strlen(buf) - 1;
  while ((p >= buf) && isspace((int)*p)) *p-- = '\0';

  /* Remove any leading white space */
  p = buf;
  while (isspace((int)*p)) p++;
  if (p != buf)
    memmove(buf, p, strlen(p)+1);

  if (strlen(buf) > 0)
    return (1);
  return (0);
}

/*
 * Strip characters after the ; character, but only if the ; character
 * doesn't have a " character in the buffer before it or the " character
 * isn't present. 
 */
static int
CmtTrim (char *buf)
{
char *p, *q;

/* String leading comments before " (if any) */
  if ((p = strchr(buf, '"')) != NULL) {
    q = strchr(buf, ';');
    if (q != NULL && q < p) *q = '\0';
  }
  else {
    if ((p = strchr(buf, ';')) != NULL) 
      *p = '\0';
  }

  if (strlen(buf) > 0)
    return (1);
  return (0);
}

/* 
 * Discards comments, (delimited by ; and to the left of " if present)
 * Throws out initial whitespace, trailing whitespace and whitespace
 * between the = lines
 * Whitespace includes CR and LF characters.
 */
static int
VarTrim (char *buf)
{
  char *p, *q;

  /* Make sure we have an =, otherwise we might as well bail */
  if ((p = strchr(buf, '=')) == NULL)
    return (0);

  if (!CmtTrim(buf)) return (0);
  if (!StrTrim(buf)) return (0);

  /* Remove white space after = */
  if ((p = strchr(buf, '=')) != NULL) {
    p++; q = p;
    while (isspace((int)*q)) q++;
    if (p != q)
      memmove (p, q, strlen(q)+1);
  }

  /* Remove white space before = */
  if ((p = q = strchr(buf, '=')) != NULL) {
    if (q != buf) {
      while (((p-1) >= buf) && isspace((int)*(p-1))) p--;
      if (p != q)
	 memmove(p, q, strlen(q)+1);
    }
  }

  if (strlen(buf) > 0) return (1);

  return (0);
}

/**
 ** Hash maintenance functions
 **/

static void
HashItemDestroy (hash_item_t *h)
{
if (h == NULL) return;

if (h->key) irrd_free(h->key);
if (h->value) irrd_free(h->value);
irrd_free(h);
  
return;
}

static void
HashSectionDestroy (section_hash_t *h)
{
if (h == NULL) return;

if (h->key) irrd_free(h->key);
if (h->hash_vars) g_hash_table_destroy(h->hash_vars);
irrd_free(h);
  
return;
}

static int
InsertVar (statusfile_t *sf, char *section, hash_item_t *h)
{
section_hash_t *h_sect;

/* Find the proper section */
if ((h_sect = g_hash_table_lookup(sf->hash_sections, section)) == NULL) {
   h_sect = irrd_malloc(sizeof(section_hash_t));
   h_sect->key = strdup(section);
   h_sect->hash_vars = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)HashItemDestroy);
   g_hash_table_insert(sf->hash_sections, h_sect->key, h_sect);
   }

g_hash_table_insert(h_sect->hash_vars, h->key, h);

return (1);
}

static hash_item_t *
LookupVar (statusfile_t *sf, char *section, char *variable)
{
hash_item_t    *p_hi;
section_hash_t *p_sh;

if ((p_sh = g_hash_table_lookup(sf->hash_sections, section)) != NULL) {
   if ((p_hi = g_hash_table_lookup(p_sh->hash_vars, variable)) != NULL) {
      return (p_hi);
      }
   }

return (NULL);
}

static int
DeleteVar (statusfile_t *sf, char *section, char *variable)
{
hash_item_t    *p_hi;
section_hash_t *p_sh;

if ((p_sh = g_hash_table_lookup(sf->hash_sections, section)) != NULL) {
   if ((p_hi = g_hash_table_lookup(p_sh->hash_vars, variable)) != NULL) {
      g_hash_table_remove(p_sh->hash_vars, p_hi->key);
      return (1);
      }
   }

return (0);
}

static int
ProcessLine (statusfile_t *sf, char *section, char *line)
{
char tmp[BUFSIZE], *p, *q, *last;
hash_item_t    *h_var;

  h_var = irrd_malloc(sizeof(hash_item_t));

  tmp[BUFSIZE-1] = '\0';
  strncpy(tmp, line, BUFSIZE);

  if (VarTrim(tmp)) {
    /* Sanity check: no " before = */
    p = strchr(tmp, '"');
    q = strchr(tmp, '=');
    if (p && (p < q)) return (0);

    if ((p = strtok_r(tmp, "=", &last)) != NULL) {
      /* No " in the key */
      if ((q = strchr(p, '"')) != NULL) return (0);

      h_var->key = strdup(p);

      if ((p = strtok_r(NULL, "=", &last)) != NULL) {
	 /* The " must follow the = if any */
         if ((q = strchr(p, '"')) != NULL) {
	    if (q != p) return (0);
	    /* Truncate the string at the next " */
	    p = q + 1;
	    if ((q = strchr(p, '"')) != NULL) *q = '\0';
	    else return (0);
      }
    }

    h_var->value = strdup(p);

    return (InsertVar(sf, section, h_var));
    }
  }

  /* Failure cases */
  if (h_var->key) irrd_free(h_var->key);
  irrd_free(h_var);

  return (0);
}

static char *
IsSection (char *buf)
{
char *p, *q;
char tmp[BUFSIZE];

strncpy(tmp, buf, BUFSIZE-1);
CmtTrim(tmp);

p = tmp;

while (isspace((int)*p)) p++;
if (*p == '[') {
   p++;
   p = strdup(p);
   if ((q = strchr (p, ']')) != NULL) {
      *q = '\0';
      return (p);
      }
   else {
      free(p);
      }
   }

return (NULL);
}

/**
 ** File operations
 **/

static int
ReadStatusFile (statusfile_t *sf)
{
FILE *fp;
char *section = NULL, *new_section = NULL;
char buf[BUFSIZE];
int length;
int ret = 0;
int c;

if ((fp = fopen (sf->filename, "r")) != NULL) {
   while (fgets (buf, BUFSIZE, fp) != NULL) {
      length = strlen (buf);
      if (((length >= 1) && (buf[length - 1] == '\n')) ||
	  ((length >= 2) && (buf[length - 1] == '\r') && (buf[length - 2] == '\n'))) {
	 /* Canonicalize on Unix style EOL */
	 if ((new_section = IsSection(buf)) != NULL) {
	    if (section) free(section);
	    section = new_section;
	    }
	 else {
	    if (!ProcessLine(sf, section, buf)) {
	       /* Ignore blank lines */
	       if (StrTrim(buf))
		  trace(INFO, default_trace, "ERROR: Failed to process line [%s]\n", buf);
	       }
	    }
	 continue;
	 }
      else {
         StrTrim(buf);
	 trace (INFO, default_trace, "ERROR: Overflowed line for statusfile %s [%s]\n", sf->filename, buf);
	 do {
	    c = fgetc(fp);
	    } while ((c != '\n') && (c != EOF));
         if (c == EOF) break;
         continue;
	 }
      }
   ret = 1;
   }

return (ret);
}

/*
 * We're going to let this also serve as a copy function.  If filename
 * is NULL, then simply use the filename from the statusfile_t.  Otherwise,
 * write to the new file.
 * 
 * In the case of a copy, we just write the file in place to the new file.
 * Its up to the caller to verify that we're not going to clobber something.
 * 
 * Otherwise we write the file to <handle filename>.tmp and then rotate
 * it in by moving the old one to <handle filename>.old and moving
 * <handle filename>.tmp to <handle filename>.
 *
 * We make an attempt to re-move the .old back if one of the moves fail,
 * but one can only do so much.
 */

static int
WriteStatusFile (statusfile_t *sf, char *filename)
{
char *base, *file, *fn;
char tmp[BUFSIZE], tmp_old[BUFSIZE];
FILE *fp;
hash_item_t    *p_hi;
section_hash_t *p_sh;
int ret = 0;

tmp[BUFSIZE-1] = '\0';

if (filename == NULL) fn = sf->filename;
else {
   if (!strcmp(filename, sf->filename)) {
      fn = sf->filename;
      }
   else {
      fn = filename;
      }
   }

if (fn == sf->filename) {
   strcpy(tmp, fn);
   base = strdup(dirname(tmp));
   strcpy(tmp, fn);
   file = strdup(basename(tmp));

   strncpy(tmp, base, BUFSIZE - 1);
   strncat(tmp, "/.", BUFSIZE - 1 - strlen(tmp));
   strncat(tmp, file, BUFSIZE - 1 - strlen(tmp));
   strncat(tmp, ".tmp", BUFSIZE - 1 - strlen(tmp));

   strncpy(tmp_old, base, BUFSIZE - 1);
   strncat(tmp_old, "/.", BUFSIZE - 1 - strlen(tmp));
   strncat(tmp_old, file, BUFSIZE - 1 - strlen(tmp));
   strncat(tmp_old, ".old", BUFSIZE - 1 - strlen(tmp));

   free(base);
   free(file);
   }
else {
   strcpy(tmp, fn);
   }

if ((fp = fopen(tmp, "w")) != NULL) {
   GList *list = NULL;
   GList *iter = NULL;
   g_hash_table_foreach(sf->hash_sections, (GHFunc)add_value_to_list, list);
   
   iter = list;
   /* Print the file */
   while (iter) {
      p_sh = iter->data;
      fprintf (fp, "[%s]\n", p_sh->key);
     
      GList *list2 = NULL;
      GList *iter2 = NULL;
      g_hash_table_foreach(p_sh->hash_vars, (GHFunc)add_value_to_list, list2);
      
      iter2 = list2;
      while(iter2) {
        p_hi = iter2->data;
	 /* If the data contains anything other than what is useful for
	    numbers, print it with quotes.  We somewhat sloppily assume
	    that numbers will contain no whitespace. */
         char *p = p_hi->value;
	 int is_num = 1;
	 int decimal_count = 0;

	 if (*p && (*p == '-')) p++;
	 while (*p && is_num && decimal_count <= 1) {
	    if (!isdigit((int)*p) && (*p == '.')) decimal_count++;
	    else if (!isdigit((int)*p)) is_num = 0;
	    p++;
	    }
	 if ((decimal_count <= 1) && is_num) {
	    fprintf (fp, "%s=%s\n", p_hi->key, p_hi->value);
	    }
	 else {
	    fprintf (fp, "%s=\"%s\"\n", p_hi->key, p_hi->value);
	    }
   iter2 = iter2->next;
	 }
      g_list_free(list2);
      fprintf(fp, "\n");
      iter = iter->next;
      }
   g_list_free(list);
   fclose (fp);

   /* Rotate the files */
   if (fn == sf->filename) {
      FILE *fp_tmp = NULL;
      unsigned char  b_file_dne = 1;

      if ((fp_tmp = fopen(sf->filename, "r")) != NULL) {
         fclose(fp_tmp);
	 b_file_dne = 0;
	 }
      
      if (!b_file_dne) 
	 if (rename(sf->filename, tmp_old) != 0) {
	    trace (INFO, default_trace, "ERROR: Unable to move new status file %s to %s\n", sf->filename, tmp_old);
	    goto FAIL;
	    }

      if (rename(tmp, sf->filename) != 0) {
	 trace (INFO, default_trace, "ERROR: Failed to rotate in new statusfile: %s->%s\n", tmp, sf->filename);

	 /* Attempt to put file back */
	 if (b_file_dne || (rename(tmp_old, sf->filename) != 0)) {
	    trace (INFO, default_trace, "ERROR: Failed to put back original statusfile: %s->%s\n", tmp_old, sf->filename);
	    goto FAIL;
	    }
	 goto FAIL;
	 }

      if (!b_file_dne)
	 if (unlink(tmp_old) != 0) {
	    /* This isn't really fatal since the new file really is in place,
	       however, the next rotate may fail */
	    trace (INFO, default_trace, "WARNING: Failed to unlink old statusfile %s\n", tmp_old);
	    }

      }
   ret = 1;
   }
else {
   trace (INFO, default_trace, "ERROR: Failed to write new statusfile %s\n", tmp);
   }

FAIL:

return (ret);
}

void add_value_to_list(gpointer key, gpointer data, GList *list) {
  list = g_list_append(list, data);
}

/**********************************************************************
 **
 ** Public Functions
 **
 **********************************************************************/

/*
 * InitStatusFile
 *
 * Arguments: The filename that you want to write to.
 *            A valid trace handle.
 *
 * Returns: statusfile_t * or NULL if failed.
 *
 */

statusfile_t *
InitStatusFile (char *filename)
{
statusfile_t *sf;

 if ((sf = irrd_malloc(sizeof(statusfile_t))) != NULL) {
   sf->filename = strdup(filename);
   if (!(sf->hash_sections = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)HashSectionDestroy)))
      goto FAIL;
   ReadStatusFile(sf);
   pthread_mutex_init (&(sf->mutex_lock), NULL);
 }

 FAIL:

 return (sf);
}

/*
 * CloseStatusFile
 *
 * Pretty obvious what this does. :-)
 */

int
CloseStatusFile (statusfile_t *sf)
{

  irrd_free(sf->filename);
  sf->filename = NULL;

  if (!pthread_mutex_lock(&(sf->mutex_lock))) {
    if (sf->hash_sections) {
      g_hash_table_destroy(sf->hash_sections);
      sf->hash_sections = NULL;
    }
    pthread_mutex_unlock(&(sf->mutex_lock));
  }

/* There's not much we can do about failures in the above routines */
  return (1);
}

/*
 * GetStatusString
 * 
 * For a given statusfile handle, returns the value associated with a
 * given variable in a particular section.
 * 
 * Returns pointer to the live data value.
 * Thou Shalt Not Alter The String Returned Without Calling Strdup
 * Lest Thou Corrupt Thine Data.
 *
 * (It was decided to not return a strdup'ed variable by default
 *  because we often just want to ref it and we may do this a LOT.
 *  This would lead to hemorraghed memory.)
 */

char *
GetStatusString (statusfile_t *sf, char *section, char *variable)
{
hash_item_t *p_hi;
char *ret = NULL;

if (sf->filename == NULL) return (NULL);

if (!pthread_mutex_lock(&(sf->mutex_lock))) {
   if ((p_hi = LookupVar(sf, section, variable)) != NULL) {
      ret = p_hi->value;
      }
   pthread_mutex_unlock(&(sf->mutex_lock));
   }

return (ret);
}

/*
 * SetStatusString
 * 
 * For a given statusfile handle, sets the value of a variable in a
 * particular section. 
 *
 * If value == NULL, delete the variable 
 * 
 * Returns: 0 on failure, 1 on success.
 */

int 
SetStatusString (statusfile_t *sf, char *section, char *variable, char *value)
{
hash_item_t *p_hi;
int ret = 0;

if (sf->filename == NULL) return (0);

if (!pthread_mutex_lock(&(sf->mutex_lock))) {
   /* Do we delete the value? */
   if (value == NULL) {
      DeleteVar(sf, section, variable);
      ret = 1;
      }
   /* See if its already in there.  If it is, then replace the current val */
   else if ((p_hi = LookupVar(sf, section, variable)) != NULL) {
      if ((p_hi->value != NULL) && strcmp(p_hi->value, value)) {
         free(p_hi->value);
	 p_hi->value = strdup(value);
	 }
      ret = 1;
      }
   /* If its not, add a new value */
   else {
      p_hi = irrd_malloc(sizeof(hash_item_t));
      p_hi->key = strdup(variable);
      p_hi->value = strdup(value);
      if (InsertVar(sf, section, p_hi)) ret = 1;
      }

   ret = WriteStatusFile(sf, NULL);

   pthread_mutex_unlock(&(sf->mutex_lock));
   }

return (ret);
}

void
uii_show_statusfile (uii_connection_t *uii) 
{
if (!pthread_mutex_lock(&(IRR.statusfile->mutex_lock))) {
   if (IRR.statusfile->filename) 
      uii_add_bulk_output (uii, "Statusfile location: %s\r\n", IRR.statusfile->filename);
   else
      uii_add_bulk_output (uii, "No statusfile configured.\r\n");
   pthread_mutex_unlock(&(IRR.statusfile->mutex_lock));
   }
}

int
config_statusfile (uii_connection_t *uii, char *filename)
{
  if (IRR.statusfile) {
    CloseStatusFile(IRR.statusfile);
    irrd_free(IRR.statusfile);
  }
  IRR.statusfile = InitStatusFile(filename);

  return ((IRR.statusfile) ? 1 : -1);
}


