/*
 * $Id: irrd_util.c,v 1.12 2002/10/17 20:02:30 ljb Exp $
 * originally Id: util.c,v 1.51 1998/08/07 19:48:58 gerald Exp 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <mrt.h>
#include <trace.h>
#include <time.h>
#include <signal.h>
#include <config_file.h>
#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <glib.h>

#include "irrd.h"

static int find_token (char **, char **);

irr_database_t *new_database (char *name) {
  irr_database_t *database;

  database = irrd_malloc(sizeof(irr_database_t));
  memset(database, 0, sizeof(irr_database_t));

  database->radix_v4 = New_Radix (32); 
  database->radix_v6 = New_Radix (128);
  database->hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)irr_hash_destroy);
  database->hash_spec = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify)irr_hash_destroy);

  database->name = strdup (name);
  database->mirror_fd  = -1;
  database->journal_fd = -1;
  database->max_journal_bytes = IRR_MAX_JOURNAL_SIZE;
  pthread_mutex_init (&database->mutex_lock, NULL);
  pthread_mutex_init (&database->mutex_clean_lock, NULL);
  /*rwl_init (&database->rwlock);*/
  return(database);
}

/* find_database
 * Given the name of a database (e.g. "mae-east") return a pointer to
 * the irr_database_t sructure, or NULL if it does not exist
 */
irr_database_t *find_database (char *name) {
  irr_database_t *db;
  int len;
  
  LL_Iterate (IRR.ll_database, db) {
    len = strlen (db->name);
    if (strlen (name) > len)
      len = strlen (name);

    if (!strncasecmp (db->name, name, len))
      return (db);
  }
  return (NULL);
}

/* irr_lock_all
 * Lock down all IRR databases used by this IRR connection
 */
void irr_lock_all (irr_connection_t *irr) {
  irr_database_t *database;

  /* Avoid deadlock, only 1 routine can get all locks at one time */
  if (pthread_mutex_lock (&IRR.lock_all_mutex_lock) != 0)
    trace (ERROR, default_trace, "Error locking --lock_all_mutex_lock--: %s\n", 
	   strerror (errno));

  LL_ContIterate (irr->ll_database, database) {
    irr_lock (database);
  }

  if (pthread_mutex_unlock (&IRR.lock_all_mutex_lock) != 0)
    trace (ERROR, default_trace, "Error unlocking --lock_all_mutex_lock--: %s\n",
           strerror (errno));

}

/* irr_unlock_all
 * Unlock down all IRR database used by this IRR connection
 */
void irr_unlock_all (irr_connection_t *irr) {
  irr_database_t *database;

  LL_ContIterate (irr->ll_database, database) {
    irr_unlock (database);
  }
}

void irr_update_lock (irr_database_t *database) {
  irr_clean_lock(database);
  irr_lock(database);
}

void irr_update_unlock (irr_database_t *database) {
  irr_unlock(database);
  irr_clean_unlock(database);
}

void irr_clean_lock (irr_database_t *database) {
  if (pthread_mutex_lock (&database->mutex_clean_lock) != 0)
    trace (ERROR, default_trace, "Error locking clean database %s : %s\n", 
	   database->name, strerror (errno));
}

void irr_clean_unlock (irr_database_t *database) {
  if (pthread_mutex_unlock (&database->mutex_clean_lock) != 0)
    trace (ERROR, default_trace, "Error unlocking clean database %s : %s\n", 
	   database->name, strerror (errno));
}

void irr_lock (irr_database_t *database) {
  /*if (rwl_writelock (&database->rwlock) != 0)*/
  if (pthread_mutex_lock (&database->mutex_lock) != 0)
    trace (ERROR, default_trace, "Error locking database %s : %s\n", 
	   database->name, strerror (errno));
}

void irr_unlock (irr_database_t *database) {
  if (pthread_mutex_unlock (&database->mutex_lock) != 0)
  /*if (rwl_writeunlock (&database->rwlock) != 0) */
    trace (ERROR, default_trace, "Error unlocking database %s : %s\n", 
	   database->name, strerror (errno));
}

/* copy_irr_object
 * Copy an object from one <DB>.db file to another
 * this is used in updates and in resyching database
 */
long copy_irr_object (irr_database_t *database, irr_object_t *object) {
  char buffer[BUFSIZE];
  long start_offset = 0;
  int len_to_read = object->len;
  int i, fread_len;

  if ( database->db_fp == NULL ) {
    trace (ERROR, default_trace, "copy_irr_object(): database not open\n");
    exit (1);
  }
  if (fseek (object->fp, object->offset, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "copy_irr_object(): irr_object fseek failed\n");
    exit (1);
  }
  if (fseek (database->db_fp, 0, SEEK_END) < 0) {
    trace (ERROR, default_trace, "copy_irr_object(): database fseek failed\n");
    exit (1);
  }
  start_offset = ftell (database->db_fp);

  while (len_to_read > 0) {
    if ( len_to_read > BUFSIZE )
	i = BUFSIZE;
    else
	i = len_to_read;
    fread_len = fread(buffer, 1, i, object->fp);
    len_to_read -= fread_len;
    fwrite (buffer, 1, (size_t) fread_len, database->db_fp);
    database->bytes += fread_len;
  }
  fread_len = fread (buffer, 1, 1, object->fp); /* read final terminating newline */
  if (fread_len !=1)
    trace (ERROR, default_trace,"copy_irr_object(): Error reading final terminating newline\n");
  if (buffer[0] != '\n')
    trace (ERROR, default_trace,"copy_irr_object(): Terminating newline characacter incorrect, value = %d\n",buffer[0]);
  fwrite ("\n", 1, 1, database->db_fp); /* write terminating newline */
  database->bytes += 1;

  return start_offset;
}

/* Delete_IRR_Object
 * Delete generic object used to hold data during scanning
 */
void Delete_IRR_Object (irr_object_t *object) {
/* ll_mbrs and ll_mbr_by_ref are saved as data for the hash */
  if (object->name) 
    irrd_free(object->name);
  if (object->nic_hdl)
    irrd_free(object->nic_hdl);
  if (object->ll_mnt_by) 
    LL_Destroy (object->ll_mnt_by);
  if (object->ll_mbrs) 
    LL_Destroy (object->ll_mbrs);
  if (object->ll_mbr_by_ref) 
    LL_Destroy (object->ll_mbr_by_ref);
  if (object->ll_mbr_of) 
    LL_Destroy (object->ll_mbr_of);
  if (object->ll_prefix) 
    LL_Destroy (object->ll_prefix);
  irrd_free(object);
}

/* New_IRR_Object
 * Create generic object used to hold data during scanning
 */
irr_object_t *New_IRR_Object (char *buffer, u_long position, u_long mode) {
  irr_object_t *irr_object;

  irr_object = irrd_malloc(sizeof(irr_object_t));
  irr_object->offset = position;
  irr_object->origin_found = 0;
  irr_object->mode = mode;
  irr_object->type = NO_FIELD;
  irr_object->filter_val = XXX_F;

  return (irr_object);
}

void Delete_Ref_keys (reference_key_t *ref_item) {
  irrd_free(ref_item->key);
  irrd_free(ref_item);
}

void foldin_key_list (LINKED_LIST **ll, char *key, int type) {
  int found;
  reference_key_t *ref_item;

  if (key == NULL)
    return;

  if (*ll == NULL) /* this way we know if *ll != NULL, list is non-empty */
    *ll = LL_Create (LL_DestroyFunction, Delete_Ref_keys, NULL);

  found = 0;
  LL_ContIterate ((*ll), ref_item) {
    if (!strcmp (ref_item->key, key)) {
      found = 1;
      break;
    }
  }

  if (!found) {
    ref_item = irrd_malloc(sizeof(reference_key_t));
    ref_item->key = strdup (key);
    ref_item->type = type;
    LL_Add ((*ll), ref_item);
  }
}

void pick_off_indirect_references (irr_answer_t *irr_answer, LINKED_LIST **ll) {
  char *cp, buf[BUFSIZE];
  enum STATES state  = BLANK_LINE, save_state;
  int curr_f = NO_FIELD;
    
  if (irr_answer->type == ROUTE || irr_answer->type == ROUTE6 || irr_answer->type == PERSON || irr_answer->type == ROLE)
    return;

  if (irr_answer->len == 0 ||
      fseek (irr_answer->db->db_fp, irr_answer->offset, SEEK_SET) < 0)
    return;

  do {
    cp = fgets (buf, BUFSIZE, irr_answer->db->db_fp);

    if ((state = get_state (cp, strlen(buf), state, &save_state)) == START_F) {
      curr_f = get_curr_f (buf);

      /* all fields here must be *single valued*, else code won't work */
      if ((irr_answer->type == IPV6_SITE && 
	   (curr_f == PREFIX || curr_f == CONTACT)) ||
	  (curr_f == ADMIN_C || curr_f == TECH_C)) {
	cp = buf + strlen(key_info[curr_f].name);
	whitespace_newline_remove (cp);
	foldin_key_list (ll, cp, curr_f);
      }
    }
  } while (state != BLANK_LINE && state != DB_EOF);

  return;
}

void lookup_object_references (irr_connection_t *irr) {
  irr_answer_t *irr_answer;
  LINKED_LIST *ll = NULL;
  reference_key_t *ref;

  LL_ContIterate (irr->ll_answer, irr_answer)
    pick_off_indirect_references (irr_answer, &ll);
  
  if (ll) {
    LL_ContIterate (ll, ref) {
      /*
printf("JW: indirect key lookup-(%s,%d)\n",ref->key,ref->type);
*/
      irr_database_find_matches (irr, ref->key, SECONDARY, 
				 RAWHOISD_MODE, ref->type, NULL, NULL);
    }
    LL_Destroy (ll);
  }
}

void lookup_prefix_exact (irr_connection_t *irr, char *key, enum IRR_OBJECTS type) {
  irr_database_t *database;
  u_long offset, len = 0;
  char *last, *prefix, *str_orig, *tmpptr;
  uint32_t origin;

  while (*key != '\0' && isspace ((int) *key)) key++;
  if (*key == '\0')
    return;

  if (type == INET6NUM) {
    origin = 0;
    prefix = key;
  } else  {
    if (strchr (key, '-') != NULL)
      prefix = strtok_r (key, "-", &last);
    else 
      prefix = strtok_r (key, " ", &last);

    if (prefix == NULL)
      return;

    if ((str_orig = strtok_r (NULL, " ", &last)) == NULL)
      return;

    while (*str_orig != '\0' && !isdigit ((int) *str_orig)) str_orig++;
    if (*str_orig == '\0')
      return;
    /* check for dots for now, should remove as asplain becomes standard */
    if ((tmpptr = index(str_orig,'.')) != NULL) {
      *tmpptr = 0;
      origin = atoi (str_orig)*65536 + atoi(tmpptr + 1);
      *tmpptr = '.';
    } else {
      if (convert_to_32 (str_orig, &origin) != 1) {
	return;  /* return if conversion fails */
      }
    }
  }

  LL_Iterate (irr->ll_database, database) {
    if (seek_prefix_object (database, type, prefix, origin, &offset, &len) > 0)
      break;
  }

  if (len > 0)
    irr_build_answer (irr, database, type, offset, len);
}

/* convert a string to an unsigned 32 bit int
 * return 1 if no errors found
 * otherwise return -1
 */
int convert_to_32 (char *strval, uint32_t *uval) {
  char *p;
  u_long d;
  
  /* see if strval points to anything */
  if (strval == NULL)
    return (-1);

  /* make sure the value is in range and has non-zero digits */
  errno = 0;    /* To distinguish success/failure after call */
  d = strtoul(strval, &p, 10);

  /* Check for various possible errors */

  if (p == strval) {  /* no digits found */
    trace (ERROR, default_trace, "convert_to_32 : no digits %s\n", strval);
    return(-1);
  }

  if ((errno == ERANGE && d == ULONG_MAX)
      || (errno != 0 && d == 0)) {
    trace (ERROR, default_trace, "convert_to_32 : out-of-range %s\n", strval);
    return(-1);
  }

  /* check for 64 bit architectures */
  if (sizeof (uint32_t) != sizeof (u_long)) {
    if (d > UINT_MAX) {
      trace (ERROR, default_trace, "convert_to_32 : out-of-range %s\n", strval);
      return(-1);	/* exceeds max 32-bit uint */
    }
  }

  *uval = (uint32_t) d;
  return (1);
}

irr_hash_string_t *new_irr_hash_string (char *str) {
  irr_hash_string_t *tmp;
  tmp = irrd_malloc(sizeof(irr_hash_string_t));
  tmp->string = strdup (str);
  return (tmp);
}

void delete_irr_hash_string (irr_hash_string_t *str) {
  irrd_free(str->string);
  irrd_free(str);
}

/* This routine finds a token in the string.  *x will
 * point to the first character in the string and *y will
 * point to the first character after the token.  A token
 * is a printable character string.  A '\n' is not considered
 * part of a legal token.  
 * Invoke this routine by setting 'x' and 'y' to the beginning
 * of the target string like this: 'x = y = target_string;
 * if (find_token (&x, &y) > 0) ...
 * each successive call to find_token () will move the pointer
 * along.
 *
 * Return:
 *   length if a token is found (*x points to token, *y first token after)
 *  -1 if no token is found (*x and *y are to be ignored)
 */
int find_token (char **x, char **y) {

  /* It's possible the target string is NULL 
   * or we are in a rpsl comment 
   */
  if (*x == NULL)
    return -1;

  /* find the first non-blank character */
  *x = *y;
  while (**x != '\0' && (**x == ' ' || **x == '\t' || **x == '\n')) (*x)++;

  if (**x == '\0')
    return -1;

  /* find the first space at the end of the token */
  *y = *x + 1;
  while (**y != '\0' && isgraph ((int) **y)) (*y)++;

  return (*y - *x);  
}

/*
 * -i performs inverse lookups on the given attribute name.
 * Return 1 if attribute name valid/supported, else return -1.
 */
int ripe_inverse_attr (irr_connection_t *irr, char **cp) {
  int attrname_len;
  char *p, *q;

  p = q = *cp;
  if ( (attrname_len = find_token (&p, &q)) < 0)
    return attrname_len;
  *cp = q;

  /* check for mnt-by inverse lookup */
  if ( attrname_len == 6 && !strncasecmp (p, "mnt-by", 6)) {
    irr->inverse_type = MNT_BY;
    return 1;
  } 

  /* check for origin inverse lookup */
  if ( attrname_len == 6 && !strncasecmp (p, "origin", 6)) {
    irr->inverse_type = ORIGIN;
    return 1;
  } 

  /* check for member-of inverse lookup */
  if ( attrname_len == 9 && !strncasecmp (p, "member-of", 9)) {
    irr->inverse_type = MEMBER_OF;
    return 1;
  } 

  return -1;
}

/*
 * -s flag has been found, now return the token after the '-s'.
 * Return 1 if something is found after the -s, else return -1.
 */
int ripe_set_source (irr_connection_t *irr, char **cp) {
  int sources_len;
  char *p, *q;

  p = q = *cp;
  if ( (sources_len = find_token (&p, &q)) < 0)
    return sources_len;
  *cp = q;

  if (sources_len >= RIPE_SOURCES_SZ)
    return -1;

  strncpy (irr->ripe_sources, p, sources_len);
  irr->ripe_sources[sources_len] = '\0';

  return 1;
}

/*
 * convert a "<object type>" in a '-t' flag from string to enum IRR_OJBECT.
 * return 1 if a valid object type is found, else return -1;
 */
int ripe_obj_type (irr_connection_t *irr, char **cp) {
  int i, j, slen, ret_code = -1;
  char *p, *q;

  p = q = *cp;
  if ((slen = find_token (&p, &q)) < 0)
    return slen;
  *cp = q;

  for (i = 0; m_info[i].command; i++) {
    j = strlen (m_info[i].command) - 1;
    if (slen == j &&
        !strncasecmp (p, m_info[i].command, j)) {
      irr->ripe_type = m_info[i].type;
      ret_code = 1;
      break;
    }
  }
  return ret_code;
}

int process_tool_param (irr_connection_t *irr, char **cp) {
  int toolname_len;
  char *p, *q;

  p = q = *cp;
  if ((toolname_len = find_token (&p, &q)) < 0)
    return toolname_len;
  *cp = q;
  return 1;
}

/*
 * Parse the ripe whois command line and set the corresponding flag
 * (and flag value if necessary).
 * This routine moves the cursor pointer used by the ripe command
 * processing routine.
 *
 * RETURN values:
 *
 *   return 1 if no flags encountered or supplied flags were
 * parsed without error.
 *
 *   return -1 if an error is encountered parsing
 * a flag. 
 */
int parse_ripe_flags (irr_connection_t *irr, char **cp) {
  int non_flag_token;
  char buf[BUFSIZE];
  char *p = *cp; /* save pointer to input line for trace output */

  /* buffer for error messages */
  buf[0] = '\0';

  for (non_flag_token = 0; **cp != '\0'; (*cp)++) {
    if (**cp == '-') {
      (*cp)++;
      if (**cp == '\0') {
	sprintf (buf, "%%ERROR:  flag value not specified\n\n\n");
	trace (NORM, default_trace, 
		"parse_ripe_flags(): flag value not specified\n");
	break;
      }
      switch (**cp) {

      case 'k': irr->stay_open = 1;    break;
      case 'F': irr->ripe_flags |= FAST_OUT;    break;
      case 'r': irr->ripe_flags |= RECURS_OFF;  break;
      case 'R':
		if (IRR.roa_database == NULL) {
		  strcpy (buf, "%%  ROA Database not configured, \"-R\" flag not supported.\n\n\n");
		  non_flag_token = 1;
		} else
		  irr->ripe_flags |= ROA_STATUS;
		break;
      case 'U':
		if (IRR.roa_database == NULL) {
		  strcpy (buf, "%%  ROA Database not configured, \"-U\" flag not supported.\n\n\n");
		  non_flag_token = 1;
		} else
		  irr->ripe_flags |= ROA_URI;
		break;
      case 'a': irr->ripe_flags |= SOURCES_ALL; break;
      case 'K': irr->ripe_flags |= KEYFIELDS_ONLY; break;
      case 'l': irr->ripe_flags |= LESS_ONE;    break;
      case 'L': irr->ripe_flags |= LESS_ALL;    break;
/*    case 'm': irr->ripe_flags |= MORE_ONE;    break; */ /* unsupported */
      case 'M': irr->ripe_flags |= MORE_ALL;    break;
      case 'x': irr->ripe_flags |= EXACT_MATCH;    break;
     /* special mirror request command, e.g. -g RADB:1:232-LAST */
      case 'g': (*cp)++;
		irr_service_mirror_request (irr, *cp);
		return -1;	/* return error as we are all done */
      case 'i': (*cp)++;
                if (ripe_inverse_attr (irr, cp) > 0)
                  irr->ripe_flags |= INVERSE_ATTR;  
                else {
                  strcpy (buf, "%% Attribute name after \"-i\""
			  " is invalid or unsupported.\n\n\n");
                  non_flag_token = 1;
                }
                break;
      case 's': (*cp)++;
                if (ripe_set_source (irr, cp) > 0)
                  irr->ripe_flags |= SET_SOURCE;  
                else {
                  strcpy (buf, "%% DB source after \"-s\""
			  " flag is too long or invalid.\n\n\n");
                  non_flag_token = 1;
                }
                break;
      case 't': (*cp)++;
                if (ripe_obj_type (irr, cp) > 0)
                  irr->ripe_flags |= TEMPLATE;
                else
		  strcpy (buf, "%%  Required object type not specified after \"-t\" flag or unrecognized type.\n\n\n");
		non_flag_token = 1;
                break;
      case 'T': (*cp)++;
                if (ripe_obj_type (irr, cp) > 0)
                  irr->ripe_flags |= OBJ_TYPE;
                else {
		  strcpy (buf, "%%  Required object type not specified after \"-T\" flag or unrecognized type.\n\n\n");
		  non_flag_token = 1;
		}
		break;
      case 'V': (*cp)++;
		if (process_tool_param (irr, cp) < 0) {
		  strcpy (buf, "%%  Error processing \"-V\" flag.\n\n\n");
		  non_flag_token = 1;
		}
                break;
      default : non_flag_token = 1;             
                sprintf (buf, "%%  Unrecognized flag \"-%c\".\n\n\n", **cp);
	        trace (NORM, default_trace, 
	        "parse_ripe_flags(): unrecognized flag '%c'\n", **cp);
	        trace (NORM, default_trace, 
	        "parse_ripe_flags(): rest of input line (%s)", p);
		(*cp)--;
		break;
      }
    }
    else if (**cp == ' ' ||
	     **cp == '\t')
      continue;
    else 
      non_flag_token = 1;

    if (non_flag_token)
      break;
  }

  if (buf[0] != '\0') { /* Got an error, tell the user */
    irr_write_nobuffer (irr, buf);
    return -1;
  }

  return 1;
}

void delete_prefix_objs (irr_prefix_object_t *prefix_obj) {
  irr_prefix_object_t  *next;

  /* follow the linked list of prefix objects and delete them */
  while (prefix_obj) {
    next = prefix_obj->next;
    irrd_free(prefix_obj);
    prefix_obj = next;
  }
}

/* radix_flush
 * Delete a radix tree and all referenced prefix objects.
   Called by database_clear.
 */
void radix_flush (radix_tree_t *radix_tree) {

  Destroy_Radix (radix_tree, delete_prefix_objs);
}

void convert_toupper(char *_z) {
  while (*_z != '\0') {
    *_z = toupper((int) *_z);
    _z++;
  }
}

/* convert time_t to something more readable */
void nice_time (long seconds, char *buf) {

  if (seconds <= 0) {
    sprintf (buf, "--:--:--");
    return;
  }

  if (seconds < 200) {
    sprintf (buf, "%ld seconds", seconds);
    return;
  }

  if (seconds / 3600 > 99)
    sprintf (buf, "%02lddy%02ldhr", 
	     seconds / (3600 * 24), (seconds % (3600 * 24)) / 3600);
  else
    sprintf (buf, "%02ld:%02ld:%02ld", 
	     seconds / 3600, (seconds / 60) % 60, seconds % 60);

  return;
}

/* irr_sort_database  
 * A real hack to alphabetically sort databases after reading the config file
 * We need to convert linked_list types as the sorting code for intrusive
 * is broken. Easier to do this than fix the sorting code...
 *
 */

static int compare_db (irr_database_t *db1, irr_database_t *db2) {
   return (strcmp (db1->name, db2->name));
 }

void irr_sort_database () {
  LINKED_LIST *ll_tmp;
  irr_database_t *db = NULL;

  ll_tmp = LL_Create (LL_CompareFunction, compare_db, NULL);
  LL_Iterate (IRR.ll_database, db) {
    LL_Add (ll_tmp, db);
  }
  LL_Sort (ll_tmp);
  /*LL_Clear (IRR.ll_database_alphabetized);*/
  db = NULL;
  LL_Iterate (ll_tmp, db) {
    LL_Add (IRR.ll_database_alphabetized, db);
  }

  LL_Destroy (ll_tmp);
}

/* Send a message to the user and get a yes or no response.
 * Terminate execution if user replies 'y' or 'Y' to the message.
 *
 * Anything other than 'n' or 'N' is considered a 'yes'
 * response.
 *
 * Input:
 *  -a message to send to the user (msg)
 *
 * Return:
 *  -void
 */
void interactive_io (char *msg) {
  char buf[BUFSIZE+1];
  
  /* interactive input from the user */
  trace (NORM, default_trace, "%s [y/n]\n", msg);
  printf ("%s [y/n] ", msg);
  buf[0] = '\0';
  while (buf != fgets (buf, BUFSIZE, stdin)) {
    printf ("Error reading input\n");
    printf ("%s [y/n] ", msg);
  }

  if (buf[0] == 'n' || buf[0] == 'N') {
    trace (NORM, default_trace, "Terminating execution at user's request.\n");
    printf ("Terminating execution at user's request.\n");
    exit (0);
  }

  trace (NORM, default_trace, "Continuing execution.\n");
  printf ("Continuing execution.\n");
}

/* Perform basic dir tests and return a textual
 * description of anything we find wrong such as
 * insufficient permissions.
 *
 * Results will tell if (dir) exists and if we have 
 * read/write permissions.  Function will try to
 * create (dir) if it doesn't exist and the
 * (creat_dir) flag is set.
 *
 * Input:
 *  -name of the dir to check (dir)
 *  -flag to indicate if (dir) should be created if it 
 *   doesn't exist
 *
 * Return:
 *  -a text string describing some problem
 *  -NULL otherwise
 */
char *dir_chks (char *dir, int creat_dir) {
  char file[BUFSIZE+1];
  FILE *fp;

  /* Sanity checks */
  if (dir == NULL)
    return strdup ("No database directory specified!");

  if (strlen (dir) > BUFSIZE)
    return strdup ("Database directory name is too long! MAX chars (BUFSIZE)");
  
  /* see if we can create a temp file in the directory */
  snprintf (file, BUFSIZE, "%s/cache-test.%d", dir, (int) getpid ());
  if ((fp = fopen (file, "w+")) == NULL) {

    /* dir does not exist, try to make it */
    if (creat_dir &&
	errno == ENOENT) {
      if (!mkdir (dir, 00755))
	return NULL;
    }

    /* we have permission problems or ... */
    snprintf (file, BUFSIZE, 
	      "Could not create the directory '%s': %s", dir, strerror (errno));
    return strdup (file);
  }
  else {
    fclose (fp);
    remove (file);
  }

  return NULL;
}

/* overwrite a hashed password with a fake value
   to prevent cracking attacks */
                                                                                
void scrub_cryptpw(char *buf) {
  char *ptr = buf;
                                                                                
  ptr = index(buf, ':');  /* check for start of attribute */
  if (ptr == NULL)
    return;
  ptr++;        /* skip past ":" or continuation character */
  while (*ptr == ' ' || *ptr == '\t')
    ptr++;      /* skip white space */
  if (!strncasecmp(ptr, "CRYPT-PW", 8) ) {
    ptr += 8;   /* skip past CRYPT-PW string */
    while (*ptr == ' ' || *ptr == '\t')
      ptr++;    /* skip white space */
    if (strlen(ptr) < 13)       /* crypt-pw string takes 13 bytes */
      return;
    memcpy(ptr, "HIDDENCRYPTPW", 13);  /* overwrite with our magic string */
  }
  return;
}

/* overwrite a hashed password with a fake value
   to prevent cracking attacks */


void scrub_md5pw(char *buf) {
  char *ptr = buf;  


  ptr = index(buf, ':');  /* check for start of attribute */
  if (ptr == NULL)
    return;
  ptr++;        /* skip past ":" or continuation character */
  while (*ptr == ' ' || *ptr == '\t')
    ptr++;      /* skip white space */
  if (!strncasecmp(ptr, "MD5-PW", 6) ) {
    ptr += 6;   /* skip past CRYPT-PW string */
    while (*ptr == ' ' || *ptr == '\t')
      ptr++;    /* skip white space */
    if (strlen(ptr) < 34)       /* md5-pw string takes 34 bytes */
      return;
    memcpy(ptr, "$1$SaltSalt$DummifiedMD5HashValue.", 34);  /* overwrite with our magic string */
  }
  return;
}


/* print an AS number, in asplain format */
char *print_as(char *buf, uint32_t asnumber) {

  sprintf (buf, "%u", asnumber);
  return buf;
}

