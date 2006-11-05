/* 
 * $Id: trans_hdrs.c,v 1.6 2002/10/17 20:25:57 ljb Exp $
 */

#include <irr_notify.h>


/* array of file names and pointers of the notification
 * and forward files.
 */
nfile_t msg_hdl[MAX_HDLS];
int num_hdls;

char tempfname[] = "/var/tmp/irr_notify.XXXXXX";

const char SENDER_HEADER[] =
"\nYour transaction has been processed by the\nIRRd routing registry system.\n";

const char DIAG_HEADER[] =
"\nDiagnostic output:\n\n------------------------------------------------------------\n\n";

const char RESPONSE_FOOTER[] =
"\n------------------------------------------------------------\n\n";

const char FORWARD_HEADER[] =
"Dear Maintainer,\n\n  This is to notify you that one or more objects in which you are\nlisted as a maintainer were the subject of an update attempt, but\n*failed* the proper authorization.\n";

const char NOTIFY_HEADER[] =
"Dear Colleague,\n\n  This is to notify you that one or more objects in which you are\ndesignated for notification have been modified in the %s routing\nregistry database.\n";

const char MAIL_HEADERS[] =
"The submission contained the following mail headers:\n\n- From: %s\n- Subject: %s\n- Date: %s\n- Msg-Id: %s\n\n";

const char WEB_UPDATE[] =
"\nThe Web submission originated from IP address:\n-   %s\n";

const char REPLACE_FAIL_NOTE[] =
"\nThe existing object has not been replaced.  Please verify that you\nare the authorized maintainer for the existing object as indicated by\nthe mnt-by attribute.  It is common for service provides to \"proxy\"\nregister their customer routes.  If the mnt-by attributes differ and\nyou consider yourself to be the rightful maintainer for this object,\nplease contact %s in order to replace the existing object.\n";

/*
 * <OPERATION> FAILED: [<obj type>] <obj key>
 * 
 * eg, ADD FAILED: [aut-num] AS9999
 */
const char SENDER_OP_FAILED[] =
"%s FAILED: [%s] %s\n";

/*
 * Same as SENDER_OP_FAILED[] execpt "OK" instead
 * of "FAILED".
 */
const char SENDER_OP_SUCCESS[] =
"%s OK: [%s] %s\n";

const char SENDER_OP_NOOP[] = 
"NO CHANGE: [%s] %s\n";

const char NULL_SUBMISSION_MSG[] =
" No objects were found in your submission.\n";

const char UNKOWN_USER_MSG[] =
" Could not determine who to reply to.\n";

const char SENDER_NET_ERROR[] =
"%s FAILED: [%s] %s\n%s";

const char INTERNAL_ERROR[] =
"%s SKIPPED: [%s] %s\n%s";

const char SENDER_SKIP_RESULT[] =
"%s: [%s] %s (NO ERRORS.  ABORT TRANSACTION.)\n";

/* Defined in hdr_fields because it is used 
 * in irr_check and irr_notify.
 * Used as additional fields on the transaction as error
 * info lines (ie, tell the user what went wrong).
 *
const char ERROR_TAG[] = "#ERROR: ";
*/

/* eg, ..... <source> where source is RADB, RIPE, ... */
const char DEL_NO_EXIST_MSG[] = "This object does not exist in DB \"%s\".\n";

const char MAINT_NO_EXIST_MSG[] = "Maintainer references do not exist:\n";
const char AUTHFAIL_MSG[] = "Authorization failure.\n";
const char NEW_MNT_ERROR_MSG[] = "New maintainers must be added by a DB administrator.\n";
const char NEW_MNT_ERROR_MSG_2[] = "New maintainers must be added by a DB administrator.\nForwarding new request to";
const char DEL_MNT_ERROR_MSG[] = "Maintainers may only be deleted by a DB administrator.\n";
const char DEL_MNT_ERROR_MSG_2[] = "Maintainers may only be deleted by a DB administrator.\nForwarding deletion request to";
const char BAD_OVERRIDE_MSG[] = "Incorrect override password \"%s\"\n";
const char UNKNOWN_USER_MSG[] = "Could not determine submitter\n";
const char MSG_SEPERATOR[] = "\n---\n";
const char EXIST_OBJ_MSG[] = "EXISTING OBJECT:\n\n";
const char PREV_OBJ_MSG[] = "PREVIOUS OBJECT:\n\n";
const char NOTIFY_REPL_MSG[] = "\nREPLACED BY:\n\n";
const char NOTIFY_DEL_MSG[] = "OBJECT DELETED:\n\n";
const char NOTIFY_ADD_MSG[] = "NEW OBJECT CREATION:\n\n";
const char FORWARD_REPL_MSG[] = "REQUESTED REPLACEMENT OBJECT:\n";
const char FORWARD_DEL_MSG[] = "REQUESTED DELETE OBJECT:\n";
const char FORWARD_ADD_MSG[] = "REQUESTED OBJECT CREATE:\n";
