/* 
 * $Id: trans_hdrs.c,v 1.4 2001/02/01 02:08:38 labovit Exp $
 */

#include <irr_notify.h>


/* array of file names and pointers of the notification
 * and forward files.
 */
nfile_t msg_hdl[MAX_HDLS];
int num_hdls;

char tempfname[] = "/var/tmp/irr_notify.XXXXXX";

const char SENDER_HEADER[] =
"Your transaction:\n\n > From: %s\n > Subject: %s\n > Date: %s\n > Msg-Id: %s\n\nhas been processed by the IRRd database system.\nDiagnostic output:\n\n------------------------------------------------------------\n\n";

const char RESPONSE_FOOTER[] =
"\n------------------------------------------------------------\n\nRADB services are provided with support from Merit Network, Inc.,\nVerio, Inc., Teleglobe International and connect.com.au pty ltd.\n\nIf you have any questions, please send them to db-admin@radb.net.\nFree database software, documentation and tools are\navailable at\nhttp://www.irrd.net.\n\n--IRRd Team\n";


const char FORWARD_HEADER[] =
"Dear Maintainer,\n\nThis is to notify you that some objects in which you are mentioned as\na maintainer were requested to be changed, but *failed* the proper\nauthorisation for any of the mentioned maintainers.\nPlease contact the sender of these changes about changes that\nneed to be made to the following objects.\n\nThe mail message causing these failures had the following mail headers:\n\n- From:    %s\n- Subject: %s\n- Date:    %s\n- Msg-Id:  %s\n";

const char NOTIFY_HEADER[] =
"Dear Colleague,\n\nThis is to notify you that some objects in which you are mentioned as\na notifier, guardian or maintainer of one of the guarded attributes\nin this object have been modified in the Merit RADB/IRR database.\nThe objects below are the NEW entries for these\nobjects in the database. In case of DELETIONS, the deleted object is\ndisplayed. NOOPs will not be reported.\n\nThe update causing these changes had the following mail headers:\n\n- From:    %s\n- Subject: %s\n- Date:    %s\n- Msg-Id:  %s\n";

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
"%s: [%s] %s\n";

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
const char NEW_MNT_ERROR_MSG_2[] = "New maintainers must be added by an DB administrator.  Sending to";
const char BAD_OVERRIDE_MSG[] = "Incorrect override password \"%s\"\n";
const char UNKNOWN_USER_MSG[] = "Could not determine submitter\n";
const char MSG_SEPERATOR[] = "\n---\n";
const char PREV_OBJ_MSG[] = "PREVIOUS OBJECT:\n\n";
const char NOTIFY_REPL_MSG[] = "\nREPLACED BY:\n\n";
const char NOTIFY_DEL_MSG[] = "OBJECT DELETED:\n\n";
const char NOTIFY_ADD_MSG[] = "NEW OBJECT CREATION:\n\n";
const char FORWARD_REPL_MSG[] = "REQUESTED REPLACEMENT OBJECT:\n";
const char FORWARD_DEL_MSG[] = "REQUESTED DELETE OBJECT:\n";
const char FORWARD_ADD_MSG[] = "REQUESTED OBJECT CREATE:\n";
