#ifndef _IRR_PGP_H
#define _IRR_PGP_H

#include <config.h>

enum KEY_TYPE {
  UNKNOWN_KT = 0,
  RSA,
  DSS
};

enum PGP_OP_TYPE {
  PGP_DEL,
  PGP_ADD,
  PGP_FINGERPR,
  PGP_VERIFY_REGULAR,
  PGP_VERIFY_DETACHED,
  PGP_SIGN_REGULAR,
  PGP_SIGN_DETACHED
};

typedef struct _char_list_t {
  char *key;
  struct _char_list_t *next;
} char_list_t;

typedef struct _pgp_data_t {
  enum KEY_TYPE type;          /* RSA, DSS                          */
  int sign_key;                /* can this key be used for signing? */
  int hex_cnt;                 /* number of hex ID's found          */
  int owner_cnt;               /* number of uid/owner's found       */
  int fingerpr_cnt;            /* number of fingerprints found      */
  char_list_t *hex_first;      /* pointers to the hex ID list       */
  char_list_t *hex_last;
  char_list_t *owner_first;    /* pointers to the uid/owner's list  */
  char_list_t *owner_last;
  char_list_t *fp_first;       /* pointers to the fingerprint list  */
  char_list_t *fp_last;
} pgp_data_t;

/* function prototypes/lib linkables */
int    pgp_add             (trace_t *, char *, char *, pgp_data_t *);
int    pgp_del             (trace_t *, char *, char *);
int    pgp_fingerpr        (trace_t *, char *, char *, pgp_data_t *);
int    pgp_verify_regular  (trace_t *, char *, char *, char *, pgp_data_t *);
int    pgp_verify_detached (trace_t *, char *, char *, char *, pgp_data_t *);
int    pgp_hexid_check     (char *, char *);
int    pgp_sign_detached   (trace_t *, char *, char *, char *, char *, char *);
int    pgp_sign_regular    (trace_t *, char *, char *, char *, char *, char *);
void   pgp_free            (pgp_data_t *);

/* debug */
void   display_pgp_data    (pgp_data_t *);

#endif /* _IRR_PGP_H */
