/*
 * $Id: irrd_util.c,v 1.10 2002/02/04 20:53:56 ljb Exp $
 * originally Id: util.c,v 1.51 1998/08/07 19:48:58 gerald Exp 
 */

#include <stdio.h>
#include <string.h>
#include <mrt.h>
#include <trace.h>
#include <time.h>
#include <signal.h>
#include <config_file.h>
#include <limits.h>
#include "irrd.h"
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

static int find_token (char **, char **);
extern key_label_t key_info[][2];
extern trace_t *default_trace;
extern m_command_t m_info [];

irr_database_t *new_database (char *name) {
  irr_database_t *database;
  hash_item_t hash_item;

  database = New (irr_database_t);
  memset(database, 0, sizeof(irr_database_t));

  database->radix = New_Radix (128);
  database->hash =
    HASH_Create (1000,
                 HASH_KeyOffset, HASH_Offset (&hash_item, &hash_item.key),
                 HASH_DestroyFunction, irr_hash_destroy,
                 0);
  database->hash_spec =
    HASH_Create (1000,
                 HASH_KeyOffset, HASH_Offset (&hash_item, &hash_item.key),
                 HASH_DestroyFunction, irr_hash_destroy, 0);

  database->name = strdup (name);
  database->mirror_fd  = -1;
  database->journal_fd = -1;
  database->max_journal_bytes = IRR_MAX_JOURNAL_SIZE;
  database->db_fp         = NULL;
  database->db_syntax  = EMPTY_DB;
  database->obj_filter = 0; /* Any object bit-fields that are 1 will be filtered out
                               * of the DB (including mirroring, updates and reloads).
                               */
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
 * Lock down all IRR database used by this IRR connection
 */
void irr_lock_all (irr_connection_t *irr) {
  irr_database_t *database;

  /* Avoid deadlock, only 1 routine can get all locks at one time */
  trace (TRACE, default_trace, "About to lock  --lock_all_mutex_lock--\n");
  if (pthread_mutex_lock (&IRR.lock_all_mutex_lock) != 0)
    trace (ERROR, default_trace, "Error locking --lock_all_mutex_lock--: %s\n", 
	   strerror (errno));
  else
    trace (TRACE, default_trace, "Locked --lock_all_mutex_lock--\n");

  LL_ContIterate (irr->ll_database, database) {
    irr_lock (database);
  }

  if (pthread_mutex_unlock (&IRR.lock_all_mutex_lock) != 0)
    trace (ERROR, default_trace, "Error unlocking --lock_all_mutex_lock--: %s\n",
           strerror (errno));
  else
    trace (TRACE, default_trace, "Unlocked --lock_all_mutex_lock--\n");

  trace (TRACE, default_trace, "Done with lock_all\n");
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

void Delete_RT_Object (irr_route_object_t *attr) {
  Delete (attr);
}

int irr_comp (char *s1, char *s2) {
  return (strcmp (s1, s2));
}

int get_prefix_from_disk (FILE *fp, u_long offset, char *buffer) {
  char *p, temp[BUFSIZE], *last; 

  if (fseek (fp, offset, SEEK_SET) < 0) 
    trace (NORM, default_trace, "** Error ** fseek failed in get_prefix");
    
  if (fgets (temp, sizeof (temp) - 1, fp) != NULL) {
    if (strtok_r (temp, " ", &last) != NULL && 
	(p = strtok_r (NULL, " ", &last)) != NULL) {
      if (*(p + strlen (p) - 1) ==  '\n')
        *(p + strlen (p) - 1) = '\0';
      strcat (buffer + strlen (buffer), p);
      return 1;
    }
    else 
      trace (NORM, default_trace, 
	     "ERROR -- get_prefix_from_disk(): bad route field (%s)\n", temp);
  }
  else
    trace (NORM, default_trace, 
	   "ERROR -- get_prefix_from_disk(): offset is bad %lu\n", offset);

  return -1;
}

/* copy_irr_object
 * Copy an object from one <DB>.db file to another
 * this is used in updates and in resyching database
 */
long copy_irr_object (FILE *src_fp, long offset, irr_database_t *database,
                     u_long obj_length) {
  char buffer[BUFSIZE];
  long start_offset = 0;
  int bytes_read   = 0;
  int i;

  if ( database->db_fp == NULL )
    return -1L;
  
  if ((fseek (src_fp, offset, SEEK_SET) < 0) ||
      (fseek (database->db_fp, 0, SEEK_END) < 0)) 
    trace (NORM, default_trace, "** Error ** fseek failed in copy_irr_object");
  start_offset = ftell (database->db_fp);

  bytes_read = 0;
  while (fgets (buffer, sizeof (buffer)-1, src_fp) != NULL) {
    if ((i = strlen (buffer)) < 2) break;

    if (bytes_read < obj_length) {
      fwrite (buffer, 1, (size_t) i, database->db_fp);
      database->bytes += i;
    }
     bytes_read += i;
  }
  sprintf (buffer, "\n");
  fwrite (buffer, 1, strlen (buffer), database->db_fp);
  database->bytes += strlen (buffer);

  return start_offset;
}

/* Delete_IRR_Object
 * Delete generic object used to hold data during scanning
 */
void Delete_IRR_Object (irr_object_t *object) {
/* ll_as and ll_mbr_by_ref are saved as data for the hash */
  if (object->name) 
    Delete (object->name);
  if (object->nic_hdl)
    Delete (object->nic_hdl);
  if (object->ll_mnt_by) 
    LL_Destroy (object->ll_mnt_by);
  if (object->ll_as) 
    LL_Destroy (object->ll_as);
  if (object->ll_keys) 
    LL_Destroy (object->ll_keys);
  if (object->ll_community)
    LL_Destroy (object->ll_community);
  if (object->ll_mbr_by_ref) 
    LL_Destroy (object->ll_mbr_by_ref);
  if (object->ll_mbr_of) 
    LL_Destroy (object->ll_mbr_of);
  if (object->ll_prefix) 
    LL_Destroy (object->ll_prefix);
  Delete (object);
}

/* New_IRR_Object
 * Create generic object used to hold data during scanning
 */
irr_object_t *New_IRR_Object (char *buffer, u_long position, u_long mode) {
  irr_object_t *irr_object;

  irr_object = New (irr_object_t);
  irr_object->offset = position;
  irr_object->mode = mode;
  irr_object->type = NO_FIELD;
  irr_object->ll_keys = LL_Create (LL_DestroyFunction, free, 0);
  irr_object->ll_prefix = LL_Create (LL_DestroyFunction, free, 0);
  irr_object->filter_val = XXX_F;

  return (irr_object);
}

/* init_key
 * Pick-off the primary key field (ie, the usual first object field)
 * You cannot assume for example that 'route:' will be the first field
 * as this is *not* an rpsl or ripe181 requirement.  
 */
void init_key (char *buf, int curr_f, irr_object_t *object, enum DB_SYNTAX syntax) {
  char *cp;

  if (object->name == NULL) { /* could get here twice if bad syntax object */
    cp = buf + key_info[curr_f][syntax].len;
    whitespace_remove (cp);
    object->name = strdup (cp);
    object->type = curr_f;
    object->filter_val = key_info[curr_f][syntax].filter_val;
  }
}

void Delete_Ref_keys (reference_key_t *ref_item) {
  Delete (ref_item->key);
  Delete (ref_item);
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
    ref_item = New (reference_key_t);
    ref_item->key = strdup (key);
    ref_item->type = type;
    LL_Add ((*ll), ref_item);
  }
}

void pick_off_indirect_references (irr_answer_t *irr_answer, LINKED_LIST **ll) {
  char *cp, buf[BUFSIZE];
  enum STATES state  = BLANK_LINE, save_state;
  int curr_f = NO_FIELD;
    
  if (irr_answer->type == ROUTE || irr_answer->type == PERSON)
    return;

  if (irr_answer->len == 0 ||
      fseek (irr_answer->fp, irr_answer->offset, SEEK_SET) < 0)
    return;

  do {
    cp = fgets (buf, BUFSIZE - 1, irr_answer->fp);

    if ((state = get_state (buf, cp, state, &save_state)) == START_F) {
      curr_f = get_curr_f (irr_answer->db_syntax, buf, state, curr_f);

      /* all fields here must be *single valued*, else code won't work */
      if ((irr_answer->type == IPV6_SITE && 
	   (curr_f == PREFIX || 
	    curr_f == CONTACT)) ||
	  (curr_f == ADMIN_C || curr_f == TECH_C)) {
	cp = buf + key_info[curr_f][irr_answer->db_syntax].len;
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

void lookup_route_exact (irr_connection_t *irr, char *key) {
  irr_database_t *database;
  u_long offset, len = 0;
  char *last, *prefix, *str_orig;
  u_short origin;

  while (*key != '\0' && isspace ((int) *key)) key++;
  if (*key == '\0')
    return;

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

  origin = (u_short) atoi (str_orig);
  LL_Iterate (irr->ll_database, database) {
    if (seek_route_object (database, prefix, origin, &offset, 
			   &len, irr->withdrawn) > 0)
      break;
  }

  if (len > 0)
    irr_build_answer (irr, database->db_fp, ROUTE, offset, len, NULL, database->db_syntax);
}

/* convert a string to an unsigned long
 * return 1 if no errors found
 * otherwise return -1
 */
int convert_to_lu (char *strval, u_long *uval) {
  char *p;
  u_long d;
  
  /* see if strval points to anything */
  if (strval == NULL)
    return (-1);

  /* make sure the value is in range and has non-zero digits */
  d = strtoul (strval, &p, 10);  
  if (d == ULONG_MAX || p == strval)
    return (-1);

  *uval = d;
  return (1);
}

irr_hash_string_t *new_irr_hash_string (char *str) {
  irr_hash_string_t *tmp;
  tmp = New (irr_hash_string_t);
  tmp->string = strdup (str);
  return (tmp);
}

void delete_irr_hash_string (irr_hash_string_t *str) {
  Delete (str->string);
  Delete (str);
}

/*
 * -s flag has been found, now return the token after the '-s'.
 * Return 1 if something is found after the -s, else return -1.
 */
int ripe_set_source (irr_connection_t *irr, char **cp) {
  char *p;
 
  while (**cp != '\0' && isspace ((int) **cp)) (*cp)++;

  if (**cp == '\0')
    return -1;
  else
    p = *cp;

  while (**cp != '\0' && isgraph ((int) **cp)) (*cp)++;

  strncpy (irr->ripe_tmp, p, *cp - p);
  irr->ripe_tmp[*cp - p] = '\0';

  return 1;
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
 *   1 if a token is found (*x points to token, *y first token after)
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

  return 1;  
}

/*
 * convert a "<object type>" in a '-t' flag from string to enum IRR_OJBECT.
 * return 1 if a valid object type is found, else return -1;
 */
int ripe_obj_type (irr_connection_t *irr, char **cp) {
  int i, j, slen, ret_code = -1;
  char *p, *q;

  /*
  while (**cp != '\0' && isspace ((int) **cp)) (*cp)++;
  
  if (**cp == '\0')
    return -1;
  */

  p = q = *cp;
  if (find_token (&p, &q) < 0)
    return -1;
  *cp = q;
  
  
  slen = q - p;
  for (i = 0; i < IRR_MAX_MCMDS; i++) {
    j = strlen (m_info[i].command) - 1;
    if (slen == j &&
        !strncasecmp (p, m_info[i].command, j)) {
      irr->ripe_type = m_info[i].type;
      ret_code = 1;
      break;
    }
  }

  /* place the cursor pointer to the start of the next token 
     find_token (cp, &q); */

  return ret_code;
}

void ignore_flag_param (irr_connection_t *irr, char **cp) {

  while (**cp != '\0' && isgraph ((int) **cp)) (*cp)++;
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
      switch (**cp) {
      case 'F': irr->ripe_flags |= FAST_OUT;    break;
      case 'r': irr->ripe_flags |= RECURS_OFF;  break;
      case 'l': irr->ripe_flags |= LESS_ONE;    break;
      case 'L': irr->ripe_flags |= LESS_ALL;    break;
      case 'm': irr->ripe_flags |= MORE_ONE;    break;
      case 'M': irr->ripe_flags |= MORE_ALL;    break;
      case 'a': irr->ripe_flags |= SOURCES_ALL; break;
      case 's': (*cp)++;
                if (ripe_set_source (irr, cp) > 0)
                  irr->ripe_flags |= SET_SOURCE;  
                else {
                  strcpy (buf, "%%  Required DB source not specified after \"-s\""
			  " flag.\n");
                  non_flag_token = 1;
                }
                break;
      case 't': (*cp)++;
                if (ripe_obj_type (irr, cp) > 0)
                  irr->ripe_flags |= TEMPLATE;
                else
		  strcpy (buf, "%%  Required object type not specified after \"-t\" flag or unrecognized type.\n");
		non_flag_token = 1;
                break;
      case 'T': (*cp)++;
                if (ripe_obj_type (irr, cp) > 0)
                  irr->ripe_flags |= OBJ_TYPE;
                else {
		  strcpy (buf, "%%  Required object type not specified after \"-T\" flag or unrecognized type.\n");
		  non_flag_token = 1;
		}
		break;
      case 'V': ignore_flag_param (irr, cp);
                break;
      default : non_flag_token = 1;             
                sprintf (buf, "%%  Unrecognized flag \"-%c\".\n", **cp);
	        trace (NORM, default_trace, 
	        "ERROR:parse_ripe_flags(): unrecognized flag '%c'\n", **cp);
	        trace (NORM, default_trace, 
	        "ERROR:parse_ripe_flags(): rest of input line (%s)", p);
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
    irr_write (irr, buf, strlen (buf));
    irr_write_buffer_flush (irr);
    return -1;
  }

  return 1;
}

void IRRD_Delete_Node (radix_node_t *node) {
  LINKED_LIST *ll_attr;

  ll_attr = (LINKED_LIST *) node->data;
  if (ll_attr) LL_Destroy (ll_attr);

  if (node->prefix)
    Delete_Prefix (node->prefix);
  Delete (node);
}

radix_str_t *new_radix_str (radix_node_t *node) {
  radix_str_t *tmp;
  tmp = New (radix_str_t);
  tmp->ptr = node;
  return (tmp);
}

void delete_radix_str (radix_str_t *str) {
  IRRD_Delete_Node (str->ptr);
  Delete (str);
}

/* radix_flush
 * Delete a radix tree. Called by database_clear
 */
void radix_flush (radix_tree_t *radix_tree) {
  radix_node_t *node = NULL;
  radix_str_t tmp;
  LINKED_LIST *ll_nodes;

  if (radix_tree == NULL)
    return;

  ll_nodes = LL_Create (LL_Intrusive, True,
			LL_NextOffset, LL_Offset (&tmp, &tmp.next),
			LL_PrevOffset, LL_Offset (&tmp, &tmp.prev),
			LL_DestroyFunction, delete_radix_str,
			0);

  RADIX_WALK_ALL (radix_tree->head, node) {
    LL_Add (ll_nodes, new_radix_str (node));
  }
  RADIX_WALK_END;

  LL_Destroy (ll_nodes);
  Delete (radix_tree);
}

/* prefix_tobitstring
 * convert a prefix to 110000111
 * This is used so we don't need a radix tree and can store prefixes
 * in a dbm file
 * Probably better ways to write this code...
 */
char *prefix_tobitstring (prefix_t *prefix) {
  char *tmp, *cp, *p;
  u_char bit;
  u_long i, b;
  int n, masklen;


  p = tmp = malloc (34);
  cp = prefix_tochar (prefix);
  masklen = 0;

  for (n=0; n<= 3; n++) {
    b = cp[n];
    bit = 128;
    i = 0;

    while (i++ <= 7) {
      if (masklen == prefix->bitlen) 
	goto done;

      if ( (b & bit)) {
        sprintf (p, "1"); p += strlen (p);
      }
      else
        sprintf (p, "0"); p += strlen (p);
    
      bit = bit >> 1;
      masklen++;
    }
  }

done:
  while (masklen++ < 32) {
    sprintf (p, "-"); p += strlen (p);
  }

  return (tmp);
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
  fgets (buf, BUFSIZE, stdin);

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
	      "Could not create the directory: %s", strerror (errno));
    return strdup (file);
  }
  else {
    fclose (fp);
    remove (file);
  }

  return NULL;
}
