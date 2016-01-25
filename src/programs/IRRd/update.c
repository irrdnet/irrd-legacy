/*
 * $Id: update.c,v 1.6 2002/10/17 20:02:32 ljb Exp $
 * originally Id: update.c,v 1.46 1998/07/29 21:15:18 gerald Exp 
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"
#include "irrd_prototypes.h"

static int build_secondary_keys (irr_database_t *db, irr_object_t *object);
static int inetnum2prefixes(irr_database_t *db, irr_object_t *object);

void mark_deleted_irr_object (irr_database_t *database, u_long offset) {
  char *xx = "*xx";

  if (fseek (database->db_fp, offset, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "fseek failed in mark_deleted (%s)", strerror(errno));
    return;
  }
  if (fwrite (xx, 1, 3, database->db_fp) == 0) 
    trace (ERROR, default_trace, "fwrite failed in mark_deleted (%s)", strerror(errno));

  fflush (database->db_fp);
}

void add_spec_keys (irr_database_t *db, irr_object_t *object) {
  char *maint, *set_name, new_key[BUFSIZE];

  if (object->ll_mbr_of == NULL)
    return;

  /* this key is for (maint|as-set) hash lookups from set objects */

  if (object->ll_mnt_by != NULL) {
    LL_ContIterate (object->ll_mnt_by, maint) {
      LL_ContIterate (object->ll_mbr_of, set_name) {
        make_spec_key (new_key, maint, set_name);
        if (object->mode == IRR_DELETE)
          memory_hash_spec_remove (db, new_key, SET_MBRSX, object);
        else
          memory_hash_spec_store (db, new_key, SET_MBRSX, object);
      }
    }
  }

  /* add this key for fast ANY lookups */

  LL_ContIterate (object->ll_mbr_of, set_name) {
    make_spec_key (new_key, NULL, set_name);
    if (object->mode == IRR_DELETE)
      memory_hash_spec_remove (db, new_key, SET_MBRSX, object);
    else
      memory_hash_spec_store (db, new_key, SET_MBRSX, object);
  }
}

/* load_irr_object
 * We need to find an object before we can delete it 
 * Basically, we need to fill in the object structure (with all of the old keys)
 */
irr_object_t *load_irr_object (irr_database_t *database, irr_object_t *irr_object) {
  irr_object_t *old_irr_object = NULL;
  int ret;
  u_long offset, len;

  /* route, route6, and inet6num objects are in the radix tree */
  if (irr_object->type == ROUTE || irr_object->type == ROUTE6 || irr_object->type == INET6NUM) 
    ret = seek_prefix_object (database, irr_object->type, irr_object->name, irr_object->origin, &offset, &len);
  else /* fill in object->offset and object->len */
    ret = find_object_offset_len (database, irr_object->name, 
                                  irr_object->type, &offset, &len);

  /* object doesn't really exist -- nothing to delete */
  if (ret < 1)
    return (NULL);

  if (fseek(database->db_fp, offset, SEEK_SET) < 0) {
    trace (ERROR, default_trace, "fseek failed in load_irr_obj (%s)", strerror(errno));
    return (NULL);
  }

  old_irr_object = (irr_object_t *) scan_irr_file_main (database->db_fp, database, 0, SCAN_OBJECT);
  if (old_irr_object)
    old_irr_object->offset = offset;

  return (old_irr_object);
}

/* irr_special_indexing_store
 * store "special" keys, or keys that we cannot just dump into the
 * the generic hash table. These include route and inetnum objects (which
 * need special radix magic).
 *
 * return 1 is we still need to store this object in the hash
 */
int irr_special_indexing_store (irr_database_t *database, irr_object_t *irr_object) {
  char *key, key_buf[BUFSIZE], str_origin[16];
  prefix_t *prefix;
  enum SPEC_KEYS speckey = GASX;
  int store_hash = 1;
  int family = AF_INET;	/* default family for ROUTE object */

  /* first add/delete object from hash associated with maintainers */
  if (irr_object->ll_mnt_by != NULL) {
    LL_ContIterate (irr_object->ll_mnt_by, key) {
      make_mntobj_key (key_buf, key);
      if (irr_object->mode == IRR_DELETE)
        memory_hash_spec_remove (database, key_buf, MNTOBJS, irr_object);
      else
        memory_hash_spec_store (database, key_buf, MNTOBJS, irr_object);
    }
  }

  switch (irr_object->type) {
  case ROUTE6:
    family = AF_INET6;  /* change family to IPV6 and fall thru.. */
    speckey = GASX6;
    make_6as_key (key_buf, print_as(str_origin, irr_object->origin));
    prefix = ascii2prefix(family, irr_object->name);
  case ROUTE:
    prefix = ascii2prefix(family, irr_object->name);
    if (prefix == NULL) {
      trace (ERROR, default_trace, "irr_special_indexing_store: prefix %s is invalid\n", irr_object->name);
      return 0;
    }
    add_spec_keys (database, irr_object);
    if (family == AF_INET)
	make_gas_key (key_buf, print_as(str_origin, irr_object->origin));
    if (irr_object->mode == IRR_DELETE) {
      memory_hash_spec_remove (database, key_buf, speckey, irr_object);
      delete_irr_prefix (database, prefix, irr_object);
    } else {
      memory_hash_spec_store (database, key_buf, speckey, irr_object);
      add_irr_prefix (database, prefix, irr_object);
    }
    store_hash = 0;
    break;
  case AUT_NUM:
    add_spec_keys (database, irr_object);
    break;
  case INET6NUM:
    prefix = ascii2prefix(AF_INET6, irr_object->name);
    if (prefix == NULL) return 0;
    if (irr_object->mode == IRR_DELETE)
      delete_irr_prefix (database, prefix, irr_object);
    else
      add_irr_prefix (database, prefix, irr_object); 
    store_hash = 0;
    break;
  case INETNUM:
    inetnum2prefixes(database, irr_object);
    break;
  case IPV6_SITE:
    if (irr_object->ll_prefix) {
      LL_Iterate (irr_object->ll_prefix, key) {
        prefix = ascii2prefix(AF_INET6, key);
        if (prefix == NULL) return 0;
        if (irr_object->mode == IRR_DELETE)
	  delete_irr_prefix (database, prefix, irr_object);
        else
	  add_irr_prefix (database, prefix, irr_object); 
      }
    }
    break;
  case AS_SET: case ROUTE_SET:
    make_setobj_key (key_buf, irr_object->name);
    if (irr_object->mode == IRR_DELETE)
      memory_hash_spec_remove (database, key_buf, SET_OBJX, NULL);
    else
      memory_hash_spec_store (database, key_buf, SET_OBJX, irr_object);
    break;
  default:
    break;
  }
  return (store_hash);
}

void back_out_secondaries (irr_database_t *db, irr_object_t *object, char *buf) {
  char *p, *q;

  for (q = buf; (p = strchr (q, ' ')) != NULL; q = p) {
    *p++ = '\0';
    irr_database_remove (db, q, object->offset);
  }
}

static int
v4addr2num(char *src, uint32_t *val, char **endptr) {
  static const char digits[] = "0123456789";
  int saw_digit = 0, octets = 0, lastval = 0, ch;
  uint32_t retval = 0;

  while ((ch = *src) != '\0') {
    const char *pch;

    if ((pch = strchr(digits, ch)) != NULL) {
      int new = lastval * 10 + (pch - digits);

      if (new > 255)
	return(0);
      lastval = new;
      if (!saw_digit) {
	if (++octets > 4)
	  return(0);
	saw_digit = 1;
      }
    } else if (ch == '.' && saw_digit) {
      if (octets == 4)
	return(0);
      retval = retval * 256 + lastval;
      lastval = 0;
      saw_digit = 0;
    } else
      break;
    src++;
  }
  *endptr = src;
  if (octets < 4)
    return(0);
  *val = retval * 256 + lastval;
  return(1);
}

/* Convert an inetnum block to a list of prefix/mask entries */
int inetnum2prefixes(irr_database_t *database, irr_object_t *object) {
  char *ptr;
  prefix_t *prefix;
  uint32_t start, end, temp, im, last_im; /* v4 addresses in host long format */
  int tbit;
  struct in_addr address;

  /* convert first address to integer val */
  if (!v4addr2num(object->name, &start, &ptr)) {
    trace (ERROR, default_trace, "inetnum address: %s is invalid\n", object->name);
    return 0;
  }

  /* skip spaces and '-' until we get to the end addr */
  while ((*ptr < '0' || *ptr > '9') && *ptr != '\0') {
    ptr++;
  }
  /* convert end address to integer val */
  if (!v4addr2num(ptr, &end, &ptr)) {
    trace (ERROR, default_trace, "inetnum address: %s is invalid\n", object->name);
    return 0;
  }

  /* start is after ending, swap start and end */
  if (start > end) {
     temp = end;
     end = start;
     start = temp;
  }
  while (end >= start) {
    im = 0xffffffff;
    tbit = 32;
   
    while (1) {
	last_im = im;
	im <<= 1;
	if ( (start & im ) != start )
	  break;
	if ( (start | ~im) > end)
	  break;
	tbit--;
	/* check for roll-over */
	if (tbit == 0) {
	  last_im = start;
	  break;
	}
    }
    address.s_addr = htonl(start);
    if (!(prefix = New_Prefix(AF_INET, &address.s_addr, tbit)))
      return 0;
    if (object->mode == IRR_DELETE)
      delete_irr_prefix (database, prefix, object);
    else
      add_irr_prefix (database, prefix, object); 
    start -= last_im;	/* start at next boundary */
    if (start == 0)	/* if we rolled over, break */
      break;
  }
  return 1;
}

/* JW It looks like the irr_database_store () routine allows 
 * duplicate PRIMARY keys.  Need to fix so it returns 
 * an error
 */
/* Given a PERSON/ROLE object, make its secondary keys and its complete
 * 'person: nic-hdl:' key (which is a primary key).  So this routine
 * inappropriately named.
 *
 * Return:
 *   void
 */
int build_secondary_keys (irr_database_t *db, irr_object_t *object) {
  char buf[256], buf1[256], *cp, *last, *p;
  int n = 0, ret_code = 1;

  switch (object->type) {
  case PERSON:
  case ROLE:
    p = buf;
    strncpy (buf1, object->name, 255);
    buf1[255] = '\0';
    cp = buf1;
    strtok_r (cp, " ", &last);
    while (cp != NULL) {
      whitespace_remove (cp);

      if (object->mode == IRR_DELETE)
	ret_code = irr_database_remove(db, cp, object->offset);
      else {
        if ((ret_code = irr_database_store (db, cp, SECONDARY, object->type, 
			       object->offset, object->len)) < 0) {
	/* an error occured, remove any secondary key's we have added */
	  if (p != buf) {
	    *p++ = ' ';
	    *p = '\0';
	    back_out_secondaries (db, object, buf);
	  }
	  break;
	}
      }
      n = strlen (cp);
      if (p != buf)
	*p++ = ' ';
      memcpy (p, cp, n);
      p += n;	
      cp = strtok_r (NULL, " ", &last);
    }
    if (object->nic_hdl) {
      n = strlen (object->nic_hdl);
      if ( (p - buf + n) >= 255 ) { /* check for overflow */
	ret_code = -1;
	break;
      }
      *p++ = ' ';
      memcpy (p, object->nic_hdl, n);
      *(p + n) = '\0';
      if (object->mode == IRR_DELETE) {
	ret_code = irr_database_remove(db, buf, object->offset);
        ret_code = irr_database_remove(db, object->nic_hdl, object->offset);
      } else {
        if ((ret_code = irr_database_store (db, buf, PRIMARY, object->type, 
			       object->offset, object->len)) < 0) {
	  *p = '\0';
	  back_out_secondaries (db, object, buf);
	} else {
          if (irr_database_store (db, object->nic_hdl, SECONDARY, object->type, 
			object->offset, object->len) < 0)  {
	    ret_code = -1;
	    irr_database_remove (db, buf, object->offset);
	  }
	}
      }
    }
    break;
  default:
    break;
  }
  return ret_code;
}

/* scan_process_object
 * We have scanned an entire object from the db, update, journal, whatever file
 * Now process the darn thing
 */
int add_irr_object (irr_database_t *database, irr_object_t *irr_object) {
  long svoffset = 0, offset;
  int ret_code = 1, store_hash = 0;

  /* if update (e.g. mirror or user update), save to end of main database */
  /* append the new object to the end of the db file */
  if (irr_object->mode == IRR_UPDATE) {
    offset = copy_irr_object (database, irr_object);
    if (offset < 0) {
      trace (ERROR, default_trace, 
	     "DB (%s) file error, could not add object (%s) to EOF\n",
	     database->name, irr_object->name);
      return -1;
    }
    svoffset = irr_object->offset;
    irr_object->offset = (u_long) offset;
  }

  /* deal with primary key; JW later need to change to detect error */
  store_hash = irr_special_indexing_store (database, irr_object); 
  
  if (store_hash) {
    if ((ret_code = irr_database_store (database, irr_object->name, PRIMARY, 
					irr_object->type, irr_object->offset, 
					irr_object->len)) > 0) {
      /* Routine will build the PERSON/ROLE secondary and 'person: hic-hdl:' primary key */
      if ( (irr_object->type == PERSON || irr_object->type == ROLE) && 
	  (ret_code = build_secondary_keys (database, irr_object)) < 0)
	irr_database_remove (database, irr_object->name, irr_object->offset);
    }
  }

  if (irr_object->mode == IRR_UPDATE) {
    if (ret_code > 0)
      trace (NORM, default_trace, "Object %s added\n", irr_object->name);
    else
      trace (ERROR, default_trace, 
	     "Disk or indexing error: key (%s)\n", irr_object->name);
  }

  /* statistics */
  if (ret_code > 0) {
    database->num_objects[irr_object->type]++;
    database->bytes += irr_object->len;
  }

  if (irr_object->mode == IRR_UPDATE) 
    irr_object->offset = svoffset;

  return ret_code;
}

/* delete_irr_object 
 * First, load the current version of the object. We need to check a) that it
 * exists, and b) we need to get the current offset/len and the secondary keys
 * Once we have all of this, delete the indicies, mark the database file on disk
 */
int delete_irr_object (irr_database_t *database, irr_object_t *irr_object,
                       u_long *db_offset) {
  int ret_code = 1, store_hash = 0;

  irr_object_t *stored_irr_object;

  if ((stored_irr_object = load_irr_object (database, irr_object)) == NULL) {
    if (irr_object->mode == IRR_DELETE)
      trace (NORM, default_trace, "Object %s not found -- delete failed\n",
	     irr_object->name);
    return -1;
  }

  /* remove the special indexes for this object */
  stored_irr_object->mode = IRR_DELETE;
  store_hash = irr_special_indexing_store (database, stored_irr_object); 

  if (store_hash) {
    if ((ret_code = irr_database_remove (database, irr_object->name, 
					 stored_irr_object->offset)) > 0)
      if ( (stored_irr_object->type == PERSON ||
		stored_irr_object->type == ROLE) &&
	  (ret_code = build_secondary_keys (database, stored_irr_object)) < 0)
	irr_database_store (database, irr_object->name, PRIMARY, 
			    stored_irr_object->type, stored_irr_object->offset, 
			    stored_irr_object->len);
  }
  
  /* statistics */
  if (ret_code > 0) {
    database->num_objects[irr_object->type]--;
    database->bytes -= irr_object->len;
    *db_offset = stored_irr_object->offset;
    trace (NORM, default_trace, "Object %s deleted!\n", irr_object->name);
  }

  /* release memory */
  Delete_IRR_Object (stored_irr_object);

  return ret_code;
}

