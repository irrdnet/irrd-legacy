#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <irrauth.h>

void add_trans_obj (trace_t *tr, lookup_info_t *start, obj_lookup_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

void free_trans_obj (obj_lookup_t *obj) {
  free (obj->key);
  free (obj->type);
  free (obj->source);
  free (obj);
}

void free_trans_list (trace_t *tr, lookup_info_t *start) {
  obj_lookup_t *obj, *temp;

  obj = start->first;
  while (obj != NULL) {
    /*fprintf (dfile, "trans object: %s %s %s state (%d) \n", 
      obj->key, obj->type, obj->source, obj->state);*/
    temp = obj->next;
    free_trans_obj (obj);
    obj = temp;
  }
} 

obj_lookup_t *create_trans_obj (trace_t *tr, trans_info_t *ti, int state,
				     long offset, FILE *fd) {
  obj_lookup_t *obj;

  obj = (obj_lookup_t *) malloc (sizeof (obj_lookup_t));
  obj->key    = strdup (ti->obj_key);
  obj->type   = strdup (ti->obj_type);
  obj->source = strdup (ti->source);
  obj->state  = state;
  obj->fpos   = offset;
  obj->fd     = fd;

  return (obj);
}

obj_lookup_t *find_trans_object (trace_t *tr, lookup_info_t *start, char *source, 
				 char *type, char *key) {
  obj_lookup_t *obj;

  /*fprintf (dfile, "is (%s) (%s) (%s) in the trans list?\n", key, type, source);*/
  obj = start->first;
  while (obj != NULL) {
    /*fprintf (dfile, "examine object (%s) (%s) (%s)\n", obj->key, obj->type, obj->source);*/
    if (!strcasecmp (obj->source, source) &&
	!strcasecmp (obj->type, type)     &&
	!strcasecmp (obj->key, key))
      break;
    obj = obj->next;
  }

  
  /*if (obj == NULL)
    fprintf (dfile, "(%s) not in list...\n---\n", key);*/
  /* else
     fprintf (dfile, "(%s) object found!\n---\n", key);*/

  return obj;
}

void trans_list_update (trace_t *tr, lookup_info_t *start, trans_info_t *ti, 
			FILE *fd, long offset) {
  obj_lookup_t *obj;
  int state;
  
  /*fprintf (dfile, "\n---\nEnter trans_list_update ()...\n");*/
  /* all possible transaction failure modes */
  if (ti->syntax_errors          ||
      ti->del_no_exist           ||
      ti->authfail               ||
      ti->otherfail != NULL      ||
      ti->maint_no_exist != NULL)
    return;

  /*fprintf (dfile, "add/update object (%s)\n", ti->obj_key);*/
  state = 1;
  if (!strcmp (ti->op, DEL_OP))
    state = 0;
 
  /* obj is in list, update its state */
  if ((obj = find_trans_object (tr, start, ti->source, 
				ti->obj_type, ti->obj_key)) != NULL) {
    obj->state = state;
    obj->fpos = offset;
  }
  else {
    obj = create_trans_obj (tr, ti, state, offset, fd);
    add_trans_obj (tr, start, obj);
  }
  /*fprintf (dfile, "exit trans_list_update ()\n---\n");*/
}
