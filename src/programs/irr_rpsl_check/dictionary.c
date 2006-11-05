#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <limits.h>

#include <irr_rpsl_check.h>
#include <irr_defs.h>

/* predefined type check routines */

static int  boolean_check   (parse_info_t *, char *, predef_t *, int, char *);
static int  free_text_check (parse_info_t *, char *, predef_t *, int, char *);
static int  email_check     (parse_info_t *, char *, predef_t *, int, char *);
static int  asnum_check     (parse_info_t *, char *, predef_t *, int, char *);
static int  enum_check      (parse_info_t *, char *, predef_t *, int, char *);
static int  int_check       (parse_info_t *, char *, predef_t *, int, char *);
static int  ipv4_check      (parse_info_t *, char *, predef_t *, int, char *);
static int  ipv6_check      (parse_info_t *, char *, predef_t *, int, char *);

static int  get_int     (parse_info_t *, char *, int, char *, unsigned long *);
static int  check_int_range (parse_info_t *, char *, predef_t *, int, unsigned long);

static int  typedef_handler    (parse_info_t *, char *, type_t *, int, 
				  enum ARG_CONTEXT, char **);
static int    union_handler      (parse_info_t *, char *, type_t *, int, 
				  enum ARG_CONTEXT, char **) ;
static int    predef_handler     (parse_info_t *, char *, type_t *, int, 
				  enum ARG_CONTEXT, char **);
static void   add_type_to_list   (type_t_ll *, type_t *) ;
static type_t *find_type_in_list (type_t_ll *, char *);
static type_t *predef_dup        (char *);
static int    empty_string       (char *);
static char   *my_strdup         (int, char *);
static int    arg_type_check     (parse_info_t *, char *, type_t *, int, 
				  enum ARG_CONTEXT, char **) ;
static int    rpsl_word_check    (parse_info_t *, char *, predef_t *, 
				  int, char *);

extern regex_t re[];

/* New.................................*/

int as_set_name_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		       int show_emsg, char *parm) {
  /* parm is string to be checked */
  char * t = parm;
  char ebuf[2048];

  fprintf(dfile, "as_set_name_check() arg=(%s)\n", parm);

  if(strlen(parm) > 3){
    if( *t == 'a' && *(t+1) == 's' && *(t+2) == '-') 
      return(rpsl_word_check(pi, rp_name, p, show_emsg, (t+3)));
    else{
      if(show_emsg){
        snprintf(ebuf, 2048, "Invalid as-set name: %s", parm);
        error_msg_queue (pi, ebuf, ERROR_MSG); 
      }
      return 1;
    }
  }
  if(show_emsg){
    snprintf(ebuf, 2048, "Invalid as-set name: %s", parm);
    error_msg_queue (pi, ebuf, ERROR_MSG); 
  }
  return 1;

}

int route_set_name_check (parse_info_t *pi, char *rp_name, predef_t *p, 
			  int show_emsg, char *parm) {
  /* parm is string to be checked */
  char * t = parm;
  char ebuf[2048];
  
  fprintf(dfile, "route_set_name_check() arg=(%s)\n", parm);

  if(strlen(parm) > 3){
    if( *t == 'r' && *(t+1) == 't' && *(t+2) == '-') 
      return(rpsl_word_check(pi, rp_name, p, show_emsg, (t+3)));
    else{
      if(show_emsg){
        snprintf(ebuf, 2048, "Invalid route-set name: %s", parm);
        error_msg_queue (pi, ebuf, ERROR_MSG); 
      }
      return 1;
    }
  }
  if(show_emsg){
    snprintf(ebuf, 2048, "Invalid route-set name: %s", parm);
    error_msg_queue (pi, ebuf, ERROR_MSG); 
  }
  return 1;

}

int real_check (parse_info_t *pi, char *rp_name, predef_t *p, int show_emsg, 
		char *parm) {
  char ebuf[2048];
  double dval;

  fprintf(dfile, "real_check () arg=(%s) lower=(%f) upper=(%f)\n",
          parm, p->u.d.lower, p->u.d.upper);

  if (regexec(&re[RE_REAL], parm, (size_t) 0, NULL, 0)) {
    if (show_emsg) {
      snprintf (ebuf, 2048, "Non-numeric argument value found for RP attribute \"%s\"", 
	       rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  } 

  /* predef_t doesn't have a float upper and lower...fix */
  if (!p->use_bounds)
    return 0;
  
  dval = atof (parm);
  if (dval < p->u.d.lower) {
    if (show_emsg) {
      snprintf (ebuf,  2048,
               "Real value (%f) is less than lower bound (%f) for RP attribute "
	       "\"%s\"", 
               dval, p->u.d.lower, rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }
  
  if (dval > p->u.d.upper)  {
    if (show_emsg) {
      snprintf (ebuf, 2048,
               "Real value (%f) exceeds upper bound (%f) for RP attribute \"%s\"", 
               dval, p->u.d.upper, rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }
  return 0;
}


int rpsl_word_check (parse_info_t *pi, char *rp_name, predef_t *p, int show_emsg, 
		     char *parm) {
  char * t = parm;
  char ebuf[2048];

 fprintf(dfile, "rpsl_word_check() arg=(%s)\n", parm);

  if( !isalpha(*t) ){  /* check first letter */
    if(show_emsg){
      snprintf(ebuf, 2048, "Invalid rpsl_word: %s must start with a letter", parm);
      error_msg_queue (pi, ebuf, ERROR_MSG); 
    }
    return 1;
  }

  if ( !isalpha(  *(t+strlen(t)-1) ) &&  !isdigit( *(t+strlen(t)-1) ) ){/* check last letter, maybe save us */
    if(show_emsg){
      snprintf(ebuf, 2048, "Invalid rpsl_word: %s must end in a letter or digit", parm);
      error_msg_queue (pi, ebuf, ERROR_MSG); 
    }
    return 1;                                                              /* from going through whole string */
  }

  while(*++t){
    if(!isalpha(*t) && !isdigit(*t) && *t != '-' && *t != '_' ){
      if(show_emsg){
        sprintf(ebuf, "Invalid rpsl_word: cannot contain character %c", *t);
        error_msg_queue (pi, ebuf, ERROR_MSG); 
      }
      return 1;
    }
  }
  
  return 0;
}

int email_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		 int show_emsg, char *email_addr) {
  char *q;
  char ebuf[2048];

  if ((q = strchr (email_addr, '@')) == NULL) {
    if (show_emsg) {
      snprintf (ebuf, 2048, "Missing '@' in email address for attribute \"%s\"", rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }

  if (strchr (q + 1, '@') != NULL) {
    if (show_emsg) {
      snprintf (ebuf, 2048,
	       "Multiple '@'s found in email address for attribute \"%s\"", rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }

  if (regexec (&re[RE_EMAIL3], email_addr, (size_t) 0, NULL, 0)  ||
      (regexec (&re[RE_EMAIL1], email_addr, (size_t) 0, NULL, 0) &&
       regexec (&re[RE_EMAIL2], email_addr, (size_t) 0, NULL, 0))) {
    if (show_emsg) {
      snprintf (ebuf, 2048, "Malformed RFC822 email address for attribute \"%s\"", rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }

  return 0;
}

/* boolean means either true or false */
int boolean_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		   int show_emsg, char *parm) {
  char ebuf[2048];
  
  fprintf(dfile, "boolean_check() arg=(%s)\n", parm);
  
  if (!strcasecmp(parm, "true") || 
      !strcasecmp(parm, "false"))
    return 0;
  else {
    if (show_emsg) {
      snprintf(ebuf, 2048,
	      "Invalid boolean value %s found in attribute \"%s\"", parm, rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
     }
    return 1;
  }
}

/* check to make sure of ASCII characters */
int free_text_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		     int show_emsg, char *parm) {
  char ebuf[2048];
  int bad_arg;

  fprintf(dfile, "free_text_check() arg=(%s)\n", parm);

  for (; *parm && isascii(*parm); parm++);

  bad_arg = (*parm != '\0');

  if (bad_arg && show_emsg) {
    snprintf(ebuf, 2048, "Non-ASCII character found in attribute \"%s\"", rp_name);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }
  
  return bad_arg;
}

int empty_string (char *s) {
  
  for (; *s && !isgraph (*s); s++);
  return (*s == '\0');
}

/* Return
 * 
 * 1 if type check passes
 * 0 if there is a arg type mismatch
 */
int arg_type_check (parse_info_t *pi, char *rp_name, type_t *t, int show_emsg, 
		    enum ARG_CONTEXT context, char **arg) {
  char ebuf[2048];
  
  /* check for non-list input to list arg and vice-versa */
  if (t->list) {
    /*
    if (**arg != '{') {
      if (show_emsg) {
	snprintf (ebuf, 2048, "RP attribute \"%s\" list attribute expected", rp_name);
	error_msg_queue (pi, ebuf, ERROR_MSG);
      }
      return 0;
    }
    */
    return 1;
  }
  else if (**arg == '{' && context == NON_LIST_TYPE) {
    if (show_emsg) {
      snprintf (ebuf, 2048, "RP attribute \"%s\" non-list argument expected", rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 0;
  }
  return 1;
}

/* Dup string (s).
 *
 * Rewritten 10/30/00 to not use a static buffer
 * to build the string.
 *
 * Input:
 *   -yes or no to enclose dup string in braces (add_braces)
 *   -string to dup (s)
 *
 * Return:
 *   -dupped string s
 *    if s is NULL then return "" or "{}" depending on value
 *    of (add_braces)
 */
char *my_strdup (int add_braces, char *s) {
  char *p, *q;
  int len;
  
  if (s == NULL) {
    if (add_braces)
      return strdup ("{}");
    else
      return strdup ("");
  }

  /* malloc size = strlen (s) + braces + '\0' */
  len = strlen (s);
  q = p = (char *) malloc (len + ((add_braces) ? 2 : 0) + 1);

  if (add_braces) 
    memcpy (p++, "{", 1);
  
  /* copy the string (s) */
  memcpy (p, s, len);
  p += len;
  
  if (add_braces)
    memcpy (p++, "}", 1);

  /* string terminator */
  *p = '\0';
  
fprintf (dfile, "my_strdup () returns (%s)\n", p);
  return q;
}


type_t *predef_dup (char *s) {
  int i;

  if (s == NULL)
    return NULL;

  for (i = 0; i < MAX_PREDEF_TYPES; i++)
    if (!strcasecmp (s, predef_type[i]) && (i != ENUM))
      return create_predef ((enum PREDEF_TYPE) i, 0, NULL, NULL, NULL);
	
  return NULL;
}

/* find a type typedef type that matches 's' */
type_t *find_type_in_list (type_t_ll *start, char *s) {
  type_t *t;

  if (s == NULL)
    return NULL;

  for (t = start->first; t != NULL; t = t->next)
    if (t->type     == TYPEDEF &&
	t->u.t.name != NULL    &&
	!strcasecmp (s, t->u.t.name))
      break;

  return t;
}

type_t *create_typedef (char *name, type_t *t) {
  type_t *obj;

  obj = (type_t *) malloc (sizeof (type_t));
  memset ((char *) obj, 0, sizeof (type_t));

  obj->type     = TYPEDEF;
  if (name != NULL)
    obj->u.t.name = name;
  obj->u.t.t    = t;

  return obj;
}

type_t *create_union (param_t *p_list) {
  type_t *obj;

  obj = (type_t *) malloc (sizeof (type_t));
  memset ((char *) obj, 0, sizeof (type_t));

  obj->type = UNION;
  obj->u.u.ll = p_list;

  return obj;
}

type_t *create_predef (enum PREDEF_TYPE ptype, int use_bounds, irange_t *i, 
		       drange_t *d, char *s) {
  type_t *obj;

  obj = (type_t *) malloc (sizeof (type_t));
  memset ((char *) obj, 0, sizeof (type_t));

  obj->type            = PREDEFINED;
  obj->u.p.use_bounds  = use_bounds;
  if (i != NULL) {
    obj->u.p.u.i.lower = i->lower;
    obj->u.p.u.i.upper = i->upper;
  }
  else if (d != NULL) {
    obj->u.p.u.d.lower = d->lower;
    obj->u.p.u.d.upper = d->upper;
  }
  obj->u.p.ptype       = ptype;
  obj->u.p.enum_string = s;

  return obj;
}

type_t *create_type (parse_info_t *pi, enum RPSL_DATA_TYPE type, ...) {
  va_list ap;
  irange_t i;
  drange_t d;
  param_t *p;
  type_t *t2, *t = NULL;
  enum PREDEF_TYPE ptype;
  char *s;

  va_start (ap, type);

  if (type == UNION) {
    p = va_arg (ap, struct _param_t *);
    t = create_union (p);
  }
  else if (type == PREDEFINED) {
    ptype = va_arg (ap, enum PREDEF_TYPE); 
    switch (ptype) {
    case INTEGER:
      i.lower = strtoul (va_arg (ap, char *), NULL, 10);
      if (i.lower == ULONG_MAX && errno == ERANGE) {
	error_msg_queue (pi, "Lower bound value exceeds maximum unsigned int", 
			 ERROR_MSG);
	break;
      }
      i.upper = strtoul (va_arg (ap, char *), NULL, 10);
      if (i.upper == ULONG_MAX && errno == ERANGE) {
	error_msg_queue (pi, "Upper bound value exceeds maximum unsigned int", 
			 ERROR_MSG);
	break;
      }
      if (i.lower > i.upper) {
	error_msg_queue (pi, "Lower bound value exceeds upper bound", 
			 ERROR_MSG);
	break;
      }
      t = create_predef (ptype, 1, &i, NULL, NULL);
      break;
    case REAL:
      d.lower = atof (va_arg (ap, char *));
      d.upper = atof (va_arg (ap, char *));
      t = create_predef (ptype, 1, NULL, &d, NULL);
      if (d.lower > d.upper)
	error_msg_queue (pi, "Lower bound value exceeds maximum bound", 
			 ERROR_MSG);
      break;      
    case ENUM:
      s = va_arg (ap, char *);
      t = create_predef (ptype, 0, NULL, NULL, s);
      break;
    default:
      fprintf (dfile, "create_type () unknown predef type (%d)\n", ptype);
      break;
    }
  }
  else if (type == TYPEDEF) { /* the t2 rec need not be a typedef, thus
			       * it may not have a name
			       */
    s = va_arg (ap, char *);
    t2 = va_arg (ap, struct _type_t *);
    if (t2->type == TYPEDEF && t2->u.t.name == NULL)
      t2->u.t.name = strdup (s);
    else
      t = create_typedef (strdup (s), t2);
  }
  else if (type == LIST) {
    t = va_arg (ap, struct _type_t *);
    t->list = 1;
    s = va_arg (ap, char *);
    if (s != NULL) {
      t->min_elem = atol (s);
      s = va_arg (ap, char *);
      t->max_elem = atol (s);
      if (t->min_elem > t->max_elem)
	error_msg_queue (pi, "Minimum elements value exceeds maximum elements", 
			 ERROR_MSG);
    }
  }
  else if (type == UNKNOWN) {
    s = va_arg (ap, char *);
    if ((t2 = find_type_in_list (&type_ll, s)) != NULL)
      t = create_typedef (NULL, t2);
    else if ((t = predef_dup (s)) == NULL)
      error_msg_queue (pi, "Unknown type", ERROR_MSG);
  }
  else
    fprintf (dfile, "create_type () unknown type (%d)\n", type);

  va_end (ap);

  /* add t to the list */
  if (type != LIST)
    add_type_to_list (&type_ll, t);

  return t;
}

param_t *create_parm (type_t *t) {
  param_t *obj;

  if (t == NULL)
    return NULL;

  obj = (param_t *) malloc (sizeof (param_t));
  memset ((char *) obj, 0, sizeof (param_t));
  obj->t = t;
 
  return (obj);
}

/* add 'elem' to the end of the parm list 'p_list */
param_t *add_parm_obj (param_t *p_list, param_t *elem) {
  param_t *p;

  if (p_list == NULL || elem == NULL)
    return NULL;

  /* find the last element in the list */
  for (p = p_list; p->next != NULL; p = p->next);
  p->next = elem;

  return p_list;
}

void initialize_umethods (method_t *m) {
  method_t *mdup;

  mdup = create_method (m->name, NULL, m->context, 0);
  mdup->type = m->type;
  mdup->ll   = m->ll;
  mdup->next = NULL;
  m->type    = UNION_OF_METHODS;
  m->umeth   = mdup;
  m->ll      = NULL;
}

method_t *add_umeth_obj (method_t *list, method_t *obj) {
  method_t *m;

  for (m = list->umeth; m->next != NULL; m = m->next);
  m->next = obj;

  return list->umeth;
}

/* Add a method object to the end of a method list */
method_t *add_method (method_t *list, method_t *obj) {
  method_t *m;

  if (obj == NULL || list == NULL)
    return list;

  if ((m = find_method (list, obj->name)) != NULL) {
    if (m->type != UNION_OF_METHODS)
      initialize_umethods (m);
    m->umeth = add_umeth_obj (m, obj);
  }
  else {
    for (m = list; m->next != NULL; m = m->next);
    m->next = obj;
  }

  return list;
}

/* determine if the argument type is a list or non-list
 * argument.
 *
 * valid_arg () function will use this info to check for
 * arg list errors, ie, a list argument in a non-list context
 * and vice-versa.
 */
enum ARG_CONTEXT find_arg_context (param_t *parm_list, int is_union) {
  param_t *p, ps;

  if (parm_list == NULL)
    return NON_LIST_TYPE;
  if (parm_list->t == NULL) {
    fprintf (dfile, "find_arg_context() parm_list->t is null\n");
    return NON_LIST_TYPE;
  }
  fprintf (dfile, "find_arg_context () type (%d)\n", parm_list->t->type);

  if (parm_list->t->type == PREDEFINED)
    return NON_LIST_TYPE;
  if (parm_list->t->type == TYPEDEF) {
    if (parm_list->t->u.t.name != NULL)
      fprintf (dfile, "find_arg_context () type name (%s)\n", parm_list->t->u.t.name);
    else
      fprintf (dfile, "find_arg_context () type name is NULL\n");
  }

  if ((parm_list->next != NULL && !is_union) || parm_list->t->list)
    return LIST_TYPE;
  else {
    if (parm_list->t->type == TYPEDEF) {
      ps.next = NULL;
      ps.t    = parm_list->t->u.t.t;
      return find_arg_context (&ps, 0);
    }
    else if (parm_list->t->type == UNION) {
      for (p = parm_list->t->u.u.ll; p != NULL; p = p->next)
	if (find_arg_context (p, 1) == LIST_TYPE)
	  return LIST_TYPE;
    }
  }
  return NON_LIST_TYPE;
}

method_t *create_method (char *name, param_t *parm_list, 
			 enum ARG_CONTEXT context, int is_3dots) {
  method_t *obj;
  param_t *p;

  obj = (method_t *) malloc (sizeof (method_t));
  memset ((char *) obj, 0, sizeof (method_t));
  obj->name    = strdup (name);
  obj->context = context;
  obj->ll      = parm_list;
  if (is_3dots) { /* find the last element in the last and make it a list */
    for (p = parm_list; p->next != NULL; p = p->next);
    p->t->list     = 1;
    p->t->min_elem = 1;
    p->t->max_elem = 1024;
  }

  return (obj);
}

rp_attr_t *create_rp_attr (char *name, method_t *method_list) {
  rp_attr_t *obj;

  obj = (rp_attr_t *) malloc (sizeof (rp_attr_t));
  memset ((char *) obj, 0, sizeof (rp_attr_t));
  obj->name  = strdup (name);
  obj->first = method_list;

  return (obj);
}

void add_rp_attr (rp_attr_t_ll *start, rp_attr_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

proto_t *create_proto_attr (char *name, method_t *m) {
  proto_t *obj;

  obj = (proto_t *) malloc (sizeof (proto_t));
  memset ((proto_t *) obj, 0, sizeof (proto_t));
  obj->name  = strdup (name);
  obj->first = m;
  return (obj);
}

proto_t *find_protocol (proto_t_ll *start, char *proto) {
  proto_t *p;

  if (proto == NULL)
    return NULL;
  for (p = start->first; p != NULL; p = p->next)
    if (!strcasecmp (proto, p->name))
      break;

  return p;
}

void add_new_proto (proto_t_ll *start, proto_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

afi_t *create_afi_attr (char *name) {
  afi_t *obj;

  obj = (afi_t *) malloc (sizeof (afi_t));
  memset ((afi_t *) obj, 0, sizeof (afi_t));
  obj->name  = strdup (name);
  return (obj);
}

afi_t *find_afi (afi_t_ll *start, char *afi) {
  afi_t *p;

  if (afi == NULL)
    return NULL;
  for (p = start->first; p != NULL; p = p->next) {
    if (!strcasecmp (afi, p->name)) 
      break;
  }

  if (p == NULL)
      fprintf (dfile, "Unable to find afi: %s\n", afi);

  return p;
}

void add_new_afi (afi_t_ll *start, afi_t *obj) {

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
}

void add_type_to_list (type_t_ll *start, type_t *obj) {

  if (obj == NULL)
    return;

  obj->next = NULL;
  if (start->first == NULL)
    start->first = obj;
  else
    start->last->next = obj;

  start->last = obj;
  
}

/* debug's */

void print_typedef (type_t *x) {
  typedef_t *t = &(x->u.t);
  param_t p;

  if (t->name == NULL)
    fprintf (dfile, "%9stypedef name is NULL\n", " ");
  else
    fprintf (dfile, "%9stypedef name <(%s)>\n", " ", t->name);

  fprintf (dfile, "%9s-------->\n", " ");
  p.next = NULL;
  p.t    = t->t;
  print_parm_list (&p);
  fprintf (dfile, "%9s-------->\n", " ");
}

void print_union (type_t *t) {

  fprintf (dfile, "%9s--------\n", " ");
  print_parm_list (t->u.u.ll);
  fprintf (dfile, "%9s--------\n", " ");
}

void print_predef (type_t *t) {
  predef_t *p = &(t->u.p);

  if (p->ptype == ENUM)
    fprintf (dfile, "%9s%s: (%s)\n", " ", predef_type[p->ptype], p->enum_string);
  else if (p->ptype == INTEGER)
    fprintf (dfile, "%9s%s: upper (%lu) lower (%lu)\n", " ", predef_type[p->ptype], p->u.i.upper, p->u.i.lower);
  else if (p->ptype == REAL)
    fprintf (dfile, "%9s%s: upper (%f) lower (%f)\n", " ", predef_type[p->ptype], p->u.d.upper, p->u.d.lower);
  else
    fprintf (dfile, "%9s%s:\n", " ", predef_type[p->ptype]);
}

void print_parm_list (param_t *ll) {
  param_t *p;
  type_t *t;

  if (ll == NULL) {
    fprintf (dfile, "%6sEmpty parm list\n", " ");
    return;
  }

  for (p = ll; p != NULL; p = p->next) {
    t = p->t;
    if (t->type == UNKNOWN) {
      fprintf (dfile, "%6s???unknown parm type???\n", " ");
      continue;
    }
    if (t->type == LIST || t->list)
      fprintf (dfile, "%6slist of %s min_elem (%ld) max_elem (%ld)\n", 
	       " ", data_type[t->type], t->min_elem, t->max_elem);
    else
      fprintf (dfile, "%6s%s\n", " ", data_type[t->type]);

    if (t->type == PREDEFINED)
      print_predef (t);
    else if (t->type == UNION)
      print_union (t);
    else
      print_typedef (t);
  }
}

void print_method_list (method_t *ll) {
  method_t *m;
  int i;

  for (m = ll, i = 1; m != NULL; m = m->next, i++) {
    if (m->type == UNION_OF_METHODS) {
      fprintf (dfile, "   ------------BEGIN UNION OF METHODS-----\n");
      fprintf (dfile, "   method %d: (%s)\n", i, m->name);
      print_method_list (m->umeth);
      fprintf (dfile, "   ------------EOF UNION OF METHODS-------\n");
    }
    else {
      fprintf (dfile, "   method %d: (%s)\n", i, m->name);
      print_parm_list (m->ll);
    }
  }
}

void print_rp_list (rp_attr_t_ll *start) {
  rp_attr_t *p;

  fprintf (dfile, "\n\n-------------------\n");
  for (p = start->first; p != NULL; p = p->next) {
    fprintf (dfile, "\nrp name=(%s)\n", p->name);
    print_method_list (p->first);
  }
  fprintf (dfile, "-------------------\n\n");
}

void print_proto_list (proto_t_ll *start) {
  proto_t *p;

  fprintf (dfile, "\n\n-------------------\n");
  for (p = start->first; p != NULL; p = p->next) {
    fprintf (dfile, "\nproto name=(%s)\n", p->name);
    print_method_list (p->first);
  }
  fprintf (dfile, "-------------------\n\n");
}

void print_afi_list (afi_t_ll *start) {
  afi_t *p;

  fprintf (dfile, "\n-------------------\n");
  for (p = start->first; p != NULL; p = p->next) {
    fprintf (dfile, "afi name=(%s)\n", p->name);
  }
  fprintf (dfile, "-------------------\n\n");
}

void print_typedef_list (type_t_ll *ll) {
  type_t *t;
  param_t p;

  fprintf (dfile, "\n\ntype list----------------\n");
  for (t = ll->first; t != NULL; t = t->next) {
    p.next = NULL;
    p.t    = t;
    print_parm_list (&p);
  }
  fprintf (dfile, "----------------\n\n");
}

/* New.................................*/

char *find_matching_brace (char **s) {
  char *p;
  int c;

  c = 0;
  p = *s;
  for (; *p; p++) {
    if (*p == '{')
      c++;
    else if (*p == '}' && --c < 1)
      break;
  }

  return p;
}


char *find_list_arg (char *s, char **ret) {
  char *q, *elem;

  if ((q = strchr (s, ',')) != NULL) {
    *q = '\0';
    elem = strdup (s);
    /* pass over any spaces */
    for (q++; *q && isspace (*q); q++);
    *ret  = strdup (q);
  }
  else {
    /* pass over any spaces */
    for (; *s && isspace (*s); s++);
    for (q = s + strlen (s) - 1; *q && isspace (*q); *q-- = '\0');
    elem = strdup (s);
    *ret  = strdup ("");
  }

  return elem;
}

char *find_arg (char **s, int list) {
  char *q, *x, *elem, *ret;
  int c;

  x = *s;

  if (**s != '{')
    elem = find_list_arg (*s, &ret);
  else {
    /* pass over any spaces */
    for (; **s && isspace (**s); (*s)++);

    /* remove right-end surrounding brace */
    c = strlen (*s) - 1;
    if (*(*s + c) == '}') {
      *(*s + c) = ' ';
      (*s)++;
    }
    
    /* remove the left-end surrounding brace */
    if (**s == '{') {
      q = find_matching_brace (s);
      *++q = '\0';
      elem = strdup (*s);
      ret  = strdup (++q);
      }
    else
      elem = find_list_arg (*s, &ret);
  }

  free (x);
  *s = ret;

  return elem;
}

/* processing modules */

int ipv4_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		int show_emsg, char *parm) {

  return !_is_ipv4_prefix (pi, parm, 1);
}

int ipv6_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		int show_emsg, char *parm) {

  return !_is_ipv6_prefix (pi, parm, 1);
}

int int_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		 int show_emsg, char *parm) {
  char *colon;
  int bad_arg;
  char ebuf[1024];
  unsigned long return_val, save_val;

  fprintf (dfile, "int_check () arg=(%s) lower=(%lu) upper=(%lu)\n", 
	   parm, p->u.i.lower, p->u.i.upper);
  if ((colon = strchr (parm, ':')) == NULL) {
    bad_arg = get_int (pi, rp_name, show_emsg, parm, &return_val);
  } else {
    *colon = '\0';
    bad_arg = get_int (pi, rp_name, show_emsg, parm, &return_val);
    if (!bad_arg) {
      if (return_val > 65535) { 
	bad_arg = 1;
        if (show_emsg) {
          snprintf (ebuf, 1024, "community string most significant 16-bit word (%lu) exceeds 65535", 
	       return_val);
          error_msg_queue (pi, ebuf, ERROR_MSG);
	} 
      }
    }
    *colon = ':';
    if (!bad_arg) {
      save_val = return_val * 65536;
      bad_arg = get_int (pi, rp_name, show_emsg, colon + 1, &return_val);
      if (!bad_arg) {
        if (return_val > 65535) { 
	  bad_arg = 1;
          if (show_emsg) {
	    snprintf (ebuf, 1024, "community string least significant 16-bit word (%lu) exceeds 65535", return_val);
	    error_msg_queue (pi, ebuf, ERROR_MSG);
	  } 
	} else {
	  return_val += save_val;
	}
      }
    }
  }
  if (!bad_arg)
    bad_arg = check_int_range (pi, rp_name, p, show_emsg, return_val);

  return bad_arg;
}

int get_int (parse_info_t *pi, char *rp_name, 
		 int show_emsg, char *parm, unsigned long *return_val) {
  char ebuf[1024];
  char *q;
  unsigned long ival;

  for (q = parm; *q != '\0' && isdigit (*q); q++);

  if (*q != '\0') {
    if (show_emsg) {
      snprintf (ebuf, 1024, "Non-numeric argument value found for RP attribute \"%s\"", 
	       rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }
  ival = strtoul (parm, NULL, 10);
  if (ival == ULONG_MAX && errno == ERANGE) {
    if (show_emsg) {
      snprintf (ebuf, 1024,
	       "Integer value (%s) exceeds max unsigned 32-bit integer",
	       parm);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }
  *return_val = ival;
  return 0;
}

int check_int_range (parse_info_t *pi, char *rp_name, predef_t *p, 
		 int show_emsg, unsigned long ival) {
  char ebuf[1024];

  if (!p->use_bounds)
    return 0;
  if (ival < p->u.i.lower) {
    if (show_emsg) {
      snprintf (ebuf, 1024,
	       "Integer value (%lu) is less than lower bound (%lu) for RP "
	       "attribute \"%s\"", 
	       ival, p->u.i.lower, rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }

  if (ival > p->u.i.upper)  {
    if (show_emsg) {
      snprintf (ebuf,  1024,
	       "Integer value (%lu) exceeds upper bound (%lu) for RP attribute "
	       "\"%s\"", 
	       ival, p->u.i.upper, rp_name);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }
    return 1;
  }
  
  return 0;
}

char *append_enum (char *curr_list, char *new_elem) {
  char *new_list;
  int slen;

  if (curr_list == NULL)
    curr_list = strdup ("");

  slen = strlen (curr_list) + strlen (new_elem) + 2;
  new_list = (char *) malloc (slen);
  strcpy (new_list, curr_list);
  strcat (new_list, new_elem);
  *(new_list + slen - 2) = ENUM_DELIMIT;
  *(new_list + slen - 1) = '\0';

  free (curr_list);
  return new_list;
}

int asnum_check (parse_info_t *pi, char *rp_name, predef_t *p, 
		 int show_emsg, char *parm) {
  int bad_arg;
  char ebuf[2048];

  /* allow the use of the term "PeerAS" instead of an AS number */
  if (!strcasecmp("peeras", parm))
     bad_arg = 0;
  else
     bad_arg = regexec(&re[RE_ASNUM], parm, (size_t) 0, NULL, 0);

  if (bad_arg && show_emsg) {
    snprintf (ebuf, 2048, "Incorrect value (%s) for RP attribute \"%s\", permitted values include either an AS number or PeerAS", parm, rp_name);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }

  return bad_arg;
}

/* Given an enum element '*parm', determine if parm is a legal
 * enum value.  The function assumes that the stored list of legal
 * enum values is stored with the ENUM_DELIMIT char immediately after
 * each enum element. eg, if ENUM_DELIMIT = ':' and we have k enum 
 * elements, the list would be stored as
 *
 * enum1:enum2:... :enumk:
 *
 * Return
 *   bad_arg = 0 parm is present in the legal enum list
 *   bad_arg = 1 otherwise
 */
int enum_check (parse_info_t *pi, char *rp_name, predef_t *p, int show_emsg, 
		char *parm) {
  int bad_arg = 1;
  char ebuf[2048];
  char *q, *r, *s;

  q = r = s = strdup (p->enum_string);

  /* p and q bracket the enum test string, each enum string is
   * delimited by ENUM_DELIMIT.
   */
  for (; bad_arg && (q = strchr (s, ENUM_DELIMIT)) != NULL; s = ++q) {
    *q = '\0';
    if (!strcasecmp (s, parm))
      bad_arg = 0;
  }

  if (bad_arg && show_emsg) {
    snprintf (ebuf, 2048, "Invalid enumeration value (%s) for RP attribute \"%s\"", 
	     parm, rp_name);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }

  free (r);
  return bad_arg;
}

int predef_handler (parse_info_t *pi, char *rp_name, type_t *t, 
		    int show_emsg, enum ARG_CONTEXT context, char **arg) {
  int bad_arg = 0, i;
  char ebuf[2048];
  int (*arg_handler)(parse_info_t *, char *, predef_t *, int, char *);
  char *p, *s;

  fprintf (dfile, "predef_handler () arg=(%s)\n", *arg);

  /* Check for list arg type with '**arg' and vice-versa */
  if (!arg_type_check (pi, rp_name, t, show_emsg, context, arg))
    return 1;

  /* Dupe the arg list.  Return list intact if argument check fails */
  if (*arg != NULL)
    s = strdup (*arg);
  else
    return 0;

  if (t->u.p.ptype == INTEGER)
    arg_handler = &int_check;
  else if (t->u.p.ptype == ENUM)
    arg_handler = &enum_check;
  else if (t->u.p.ptype == IPV4_ADDRESS)
    arg_handler = &ipv4_check;
  else if (t->u.p.ptype == IPV6_ADDRESS)
    arg_handler = &ipv6_check;
  else if (t->u.p.ptype == AS_NUMBER)
    arg_handler = &asnum_check;
  else if (t->u.p.ptype == BOOLEAN)
    arg_handler = &boolean_check;
  else if (t->u.p.ptype == FREE_TEXT)
    arg_handler = &free_text_check;
  else if (t->u.p.ptype == EMAIL)
    arg_handler = &email_check;
  else if (t->u.p.ptype == REAL)
    arg_handler = &real_check;
  else {
    fprintf (dfile, "predef_handler () unknown type to check (%d)\n", t->u.p.ptype);
    return 1;
  }

  /* for each element in the list, pass it to the argument handler */
  for (i = 0; !bad_arg && (p = find_arg (&s, t->list)) && *p != '\0'; free (p)) {
    if (!(bad_arg = (t->list && t->max_elem > 0 && ++i >= t->max_elem)))
      bad_arg = (*arg_handler) (pi, rp_name, &(t->u.p), show_emsg, p);
    else if (show_emsg) {
      snprintf (ebuf, 2048, "RP attr (%s) number of args (%d) exceeds maximum "
	       "threshold (%ld)", rp_name, i, t->max_elem);
      error_msg_queue (pi, ebuf, ERROR_MSG);
    }

    if (!t->list) {
      free (p);
      break;
    }
  }

  /* See if the minimum elements threshold is satisfied */
  if (!bad_arg                                                    && 
      (bad_arg = (t->list && t->max_elem > 0 && t->min_elem > i)) &&
      show_emsg) {
    snprintf (ebuf, 2048, "RP attr (%s) actual arguments (%d) is less than minimum "
	     "arguments threshold (%ld)", rp_name, i, t->min_elem);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }

  /* if the arg is good, reset the original arg list
   * with this arg removed
   */
  if (!bad_arg) {
    free (*arg);
    *arg = strdup (s);
  }
  free (s);    
  
  return bad_arg;
}

int union_handler (parse_info_t *pi, char *rp_name, type_t *t, 
		   int show_emsg, enum ARG_CONTEXT context, char **arg) {
  int i, too_many_args = 0;
  int bad_arg;
  char ebuf[2048];
  int (*arg_handler)(parse_info_t *, char *, type_t *, int, enum ARG_CONTEXT, 
		     char **);
  char *p, *s = NULL;
  param_t *parm;

  /* Check for list arg type with '**arg' and vice-versa */
  if (!arg_type_check (pi, rp_name, t, show_emsg, context, arg))
    return 1;

  /* Dupe the arg list.  Return list intact if argument check fails */
  if (*arg != NULL)
    s = strdup (*arg);
  else
    return 0;


  /* for each parm in the argument list, see if a union type check passes */
  for (bad_arg = i = 0; (p = find_arg (&s, t->list)) && *p != '\0'; free (p)) {
    bad_arg = 1;

    for (parm = t->u.u.ll; parm != NULL; parm = parm->next) {

      /* determine the union type */
      if (parm->t->type == PREDEFINED)
	arg_handler = &predef_handler;
      else if (parm->t->type == TYPEDEF)
	arg_handler = &typedef_handler;
      else if (parm->t->type == UNION)
	arg_handler = &union_handler;
      else {
	fprintf (dfile, "union_handler () unknown type (%d)\n", parm->t->type);
	return 1;
      }
      
      if (!(bad_arg = (t->list && t->max_elem > 0 && i >= t->max_elem))) {
	if (!(bad_arg = (*arg_handler) (pi, rp_name, parm->t, 0, context, &p))) {
	  i++;
	  break;
	}
      }
      else {
	too_many_args = 1;
	if (show_emsg) {
	  snprintf (ebuf, 2048, "RP attr (%s) number of args (%d) exceeds maximum "
		   "threshold (%ld)", rp_name, i, t->max_elem);
	  error_msg_queue (pi, ebuf, ERROR_MSG);
	}
	break;
      }
    }
    
    if (bad_arg && !too_many_args && show_emsg) {

      snprintf (ebuf, 2048, "RP attr (%s) syntax error \"%s\"", rp_name, p);
      error_msg_queue (pi, ebuf, ERROR_MSG);
      break;
    }
    
    if (!t->list)
      break;
  }

  /* See if the minimum elements threshold is satisfied */
  if (!bad_arg                                                    && 
      (bad_arg = (t->list && t->max_elem > 0 && t->min_elem > i)) &&
      show_emsg) {
    snprintf (ebuf, 2048, "RP attr (%s) actual arguments (%d) is less than minimum "
	     "arguments threshold (%ld)", rp_name, i, t->min_elem);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }

  /* if the arg is good, reset the original arg list
   * with this arg removed
   */
  if (!bad_arg) {
    free (*arg);
    *arg = strdup (s);
  }
  free (s);    
  
  return bad_arg;
}

int typedef_handler (parse_info_t *pi, char *rp_name, type_t *t, 
		     int show_emsg, enum ARG_CONTEXT context, char **arg) {
  char *p, *s;
  int bad_arg = 0, i = 0;
  char ebuf[2048];
  int (*arg_handler)(parse_info_t *, char *, type_t *, int, enum ARG_CONTEXT, 
		     char **);
  
  fprintf (dfile, "typedef_handler () arg=(%s) list=(%d)\n", *arg, t->list);
  
  /* Check for list arg type with '**arg' and vice-versa */
  if (!arg_type_check (pi, rp_name, t, show_emsg, context, arg))
    return 1;
  
  /* Dupe the arg list.  Return list intact if argument check fails */
  if (*arg != NULL)
    s = strdup (*arg);
  else
    return 0;
  
  if (t->u.t.t->type == PREDEFINED)
    arg_handler = &predef_handler;
  else if (t->u.t.t->type == TYPEDEF)
    arg_handler = &typedef_handler;
  else if (t->u.t.t->type == UNION)
    arg_handler = &union_handler;
  else {
    fprintf (dfile, "typedef_handler () unknown type (%d)\n", t->u.t.t->type);
    return 1;
  }

  if (t->list) {
    /* for each element in the list, pass it to the argument handler */
    for (i = 0; !bad_arg && (p = find_arg (&s, t->list)) && *p != '\0'; 
	 i++, free (p)) {
      if (!(bad_arg = (t->max_elem > 0 && i >= t->max_elem)))
	bad_arg = (*arg_handler) (pi, rp_name, t->u.t.t, show_emsg, context, &p);
      else if (show_emsg) {
	snprintf (ebuf, 2048, "RP attr (%s) number of args (%d) exceeds maximum "
		 "threshold (%ld)", rp_name, i, t->max_elem);
	error_msg_queue (pi, ebuf, ERROR_MSG);
      }
    }
  }
  else /* pass a non-list argument to the argument handler */
    bad_arg = (*arg_handler) (pi, rp_name, t->u.t.t, show_emsg, context, &s);
  
  /* See if the minimum elements threshold is satisfied */
  if (!bad_arg                                                    && 
      (bad_arg = (t->list && t->max_elem > 0 && t->min_elem > i)) &&
      show_emsg) {
    snprintf (ebuf, 2048, "RP attr (%s) actual arguments (%d) is less than minimum "
	     "arguments threshold (%ld)", rp_name, i, t->min_elem);
    error_msg_queue (pi, ebuf, ERROR_MSG);
  }

  /* if the arg is good, reset the original arg list
   * with this arg removed
   */
  if (!bad_arg) {
    free (*arg);
    *arg = strdup (s);
  }
  free (s);    
  
  return bad_arg;
}

int valid_args (parse_info_t *pi, method_t *m, int add_braces, 
		char *rp_attr, int show_emsg, char **args) {
  char *s;
  int bad_arg;
  param_t  *parm;
  method_t *meth;
  char ebuf[2048];

  /* dup input string to support object canonicalization */
  s = my_strdup (add_braces, *args);

  if (m->type == UNION_OF_METHODS) {
    for (bad_arg = 1, meth = m->umeth; meth != NULL && bad_arg; meth = meth->next)
      bad_arg = valid_args (pi, meth, 0, rp_attr, 0, &s);

    if (bad_arg) {
      snprintf (ebuf, 2048,  "\"%s\" method \"%s\" argument does not match any of the "
	       "specified types", rp_attr, m->name);
      error_msg_queue (pi, ebuf, ERROR_MSG);      
    }
  }
  else {
    if (m->context == LIST_TYPE && *s != '{') {
      if (show_emsg) {
	snprintf (ebuf, 2048, "RP attribute \"%s\" list attribute expected", rp_attr);
	error_msg_queue (pi, ebuf, ERROR_MSG);
      }
      return 0;
    }

    for (bad_arg = 0, parm = m->ll; parm != NULL && !bad_arg; parm = parm->next) {
      
      switch (parm->t->type) {
      case PREDEFINED:
	bad_arg = predef_handler  (pi, rp_attr, parm->t, show_emsg, m->context, &s);
	break;
      case TYPEDEF:
	bad_arg = typedef_handler (pi, rp_attr, parm->t, show_emsg, m->context, &s);
	break;
      case UNION:
	bad_arg = union_handler   (pi, rp_attr, parm->t, show_emsg, m->context, &s);
	break;
      default:
	break;
      }
      
      if (empty_string (s))
	break;
    }

    if (!bad_arg) {
      if (parm != NULL && parm->next != NULL) {
	bad_arg = 1;
	if (show_emsg) {
	  snprintf (ebuf, 2048, "RP-attribute \"%s\", not enough arguments for method "
		   "\"%s\"", 
		   rp_attr, m->name);
	  error_msg_queue (pi, ebuf, ERROR_MSG);
	}
      }
      else if (!empty_string (s)) {
	bad_arg = 1;
	if (show_emsg) {
	  snprintf (ebuf, 2048, "RP-attribute \"%s\", too many arguments for method "
		   "\"%s\"", 
		   rp_attr, m->name);
	  error_msg_queue (pi, ebuf, ERROR_MSG);
	}
      }
    }
  }

  free (s);

  return !bad_arg;
}

