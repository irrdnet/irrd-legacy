#ifndef _ATOMIC_OPS_H
#define _ATOMIC_OPS_H

#include <config.h>
#include <mrt.h>
#include "../programs/IRRd/irrd.h"
#include <irr_defs.h>

#define BAK_SFX "irrdbak"

typedef struct _atom_finfo_t {
  char *tmp_dir;
  char tmp_list[BUFSIZE+1];
  char file_list[BUFSIZE+1];
} atom_finfo_t;

/* function prototypes */

int  atomic_move    (char *, char *, uii_connection_t *, atom_finfo_t *);
int  atomic_del     (char *, char *, uii_connection_t *, atom_finfo_t *);
void atomic_cleanup (atom_finfo_t *);

#endif /*_ATOMIC_OPS_H  */
