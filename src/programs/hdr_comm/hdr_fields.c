/*
 * $Id: hdr_fields.c,v 1.7 2002/10/17 20:12:01 ljb Exp $
 */

#include "hdr_comm.h"

/* leave blank space after HDR-START:, overwritten
 * by irr_notify () to indicate if transaction
 * succeeded or failed.
 */
const char HDR_START[]      = "HDR-START: ";
const char HDR_END[]        = "HDR-END:";

/* irr_check */

const char SOURCE[]         = "SOURCE:";
const char SYNTAX_ERRORS[]  = "SYNTAX-ERRORS:";
const char SYNTAX_WARNS[ ]  = "SYNTAX-WARNS:";
const char OBJ_TYPE[]       = "OBJ-TYPE:";
const char OBJ_KEY[]        = "OBJ-KEY:";
const char MNT_BY[]         = "MNT-BY:";
const char OVERRIDE[]       = "OVERRIDE:";
const char CHECK_NOTIFY[]   = "CHECK-NOTIFY:";
const char CHECK_MNT_NFY[]  = "CHECK-MNT-NFY:";
const char PASSWORD[]       = "PASSWORD:";
const char KEYCERTFN[]      = "KEYCERTFN:";

/* irr_auth */

const char DEL_NO_EXIST[]   = "DEL-NO-EXIST:";
const char OTHERFAIL[]      = "OTHERFAIL:";
const char AUTHFAIL[]       = "AUTHFAIL:";
const char MAINT_NO_EXIST[] = "MAINT-NO-EXIST:";
const char UPD_TO[]         = "UPD-TO:";
const char OLD_OBJ_FILE[]   = "OLD-OBJ-FILE:";
const char NEW_MNT_ERROR[]  = "NEW_MNT_ERROR:";
const char DEL_MNT_ERROR[]  = "DEL_MNT_ERROR:";
const char BAD_OVERRIDE[]   = "BAD_OVERRIDE:";

/* irr_check and irr_auth */

const char OP[]             = "OP:";
const char NOTIFY[]         = "NOTIFY:";
const char MNT_NFY[]        = "MNT-NFY:";

/* pgp/email check */

const char PGP_KEY[]        = "COOKIE: PGPKEY-";
const char FROM[]           = "COOKIE: MAIL-FROM ";
const char DATE[]           = "COOKIE: DATE ";
const char SUBJECT[]        = "COOKIE: SUBJ ";
const char MSG_ID[]         = "COOKIE: MESS ";
const char UNKNOWN_USER[]   = "UNKNOWN-USER:";
/* 5/10/00 key-cert support */
const char HEXID_MISMATCH[] = "COOKIE: KEY-CERT HEXID MISMATCH";
const char BAD_TRANS_SIG[]  = "COOKIE: BAD TRANSACTION SIGNATURE";
const char KEY_CERT_CAND[]  = "COOKIE: NEW SIGNATURE CANDIDATE";

/* this is the value of the UNKNOWN_USER[] field */
const char UNKNOWN_USER_NAME[] = "WHO_KNOWS?";

/* used in irr_check and irr_notify to indicate
 * errors and warning messages to the user 
 */
const char ERROR_TAG[] = "#ERROR: ";
const char WARN_TAG[]  = "#WARNING: ";
/*
const char ERROR_TAG[] = "*ERROR*: ";
const char WARN_TAG[]  = "*WARNING*: ";
*/
