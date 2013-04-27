#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "mrt.h"
#include "trace.h"
#include "irrd.h"

#define GRM_BUFSIZE (10*1024)

/*
 * mirrorstatus_buildquery:
 *
 * Builds the !j query that we're going to send to a remote IP.
 * Returns 0 on failure, 1 on success.
 *
 * Arguments:
 * LINKED_LIST *ll_checkdb - Empty linked list.  We add on each
 *   irr_database_t * that matches the mirror_prefix and mirror_port.
 *   This lets us minimize the number of databases we have to iterate through
 *   for checking answers.
 * char *query - Pointer to a string in which to build the query.
 * int query_len - Length of the query array
 * prefix_t *mirror_prefix - The IP address we are querying.
 * int mirror_port - The TCP port that we're querying.
 *
 */
static int mirrorstatus_buildquery (LINKED_LIST *ll_checkdb, char *query, int query_len, prefix_t *mirror_prefix, int mirror_port) {
  char *cp;
  int space_left;
  irr_database_t *database;
  int ret = 0;

  /* Iterate through all the databases looking for this prefix and 
     mirror_port and build up the query. */

  strcpy(query, "!j");
  space_left = query_len - 3;
  LL_IntrIterate (IRR.ll_database, database) {
    if ((database->mirror_prefix != NULL) &&
        (prefix_equal(mirror_prefix, database->mirror_prefix)) && 
        (mirror_port == database->mirror_port)) {

      if (strlen(database->name) > (space_left - 1)) {
	/* TODO - abort */
      }
      strncat(query, database->name, space_left);
      strcat(query, ",");
      space_left -= MAX((0), (strlen(database->name) + 1));

      LL_Add (ll_checkdb, database);
    }
  }

  /* Do we have anyone in our list? */
  if (!strcmp(query, "!j")) {
     trace (NORM, default_trace,
            "mirrorstatus: *WARNING* unable to build query - no one mirroring via %s:%d\n",
	    prefix_toa(mirror_prefix), mirror_port);
     *query = '\0';
     goto FAIL;
  }

  cp = query + strlen(query) - 1;
  *(cp++) = '\n'; /* Remove trailing comma */
  *cp = '\0';

  convert_toupper(query+2);

  ret = 1;

FAIL:

  return (ret);
}

/*
 * mirrorstatus_sendquery_getanswer:
 *
 * Sends the !j query to a remote IP.  Pulls back the answer.
 * Returns 0 on failure, 1 on success.
 *
 * Arguments:
 * char *query - Pointer to a string in which to build the query.
 * char *answer - Where we are going to store the answer we get back.
 * int answer_len - Length of the answer array
 * prefix_t *mirror_prefix - The IP address we are querying.
 * int mirror_port - The TCP port that we're querying.
 *
 */
static int mirrorstatus_sendquery_getanswer (char *query, char *answer, int answer_size, prefix_t *mirror_prefix, int mirror_port) {
  int ret = 0, n_ret, select_ret, n_read, space_left;
  int sockfd;
  struct sockaddr_in servaddr;
  char *cp;
  fd_set fd_read;
  struct timeval tv;

#ifdef HAVE_LIBPTHREAD
  tv.tv_usec = 0;
  tv.tv_sec = 30; /* 30 second timeout */
#else
  tv.tv_usec = 0;
  tv.tv_sec = 5; /* 5 second timeout */
#endif

  trace (TRACE, default_trace, "get_remote_mirrorstatus: Connecting to %s:%d\n",
	 prefix_toa(mirror_prefix), mirror_port);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&servaddr, 0, sizeof(servaddr));

  /* Connect */
  n_ret = nonblock_connect (default_trace, mirror_prefix, mirror_port, sockfd);

  if (n_ret != 1) {
    trace (NORM, default_trace, "mirrorstatus: Failed to connect to %s:%d\n",
	   prefix_toa(mirror_prefix), mirror_port);
    if (sockfd > -1) close (sockfd);
    goto FAIL;
  }

  /* This is slightly sloppy.  We count on a single write being able to
     push the answer since its doubtful that we'll have more than the
     lowest MTU (700+bytes) of queries. */
  n_ret = write (sockfd, query, strlen(query));
  if (n_ret != strlen(query)) {
    trace (NORM, default_trace,
	   "mirrorstatus: Failed to write query to %s:%d\nget_remote_mirrorstatus: %s",
	   prefix_toa(mirror_prefix), mirror_port, query);
    if (sockfd > -1) close (sockfd);
    goto FAIL;
  }

  space_left = answer_size - 1;
  answer[answer_size] = '\0';
  n_read = 0;
  cp = answer;

  FD_ZERO (&fd_read);
  FD_SET (sockfd, &fd_read);
  /* We can get away without resetting the fd_read in the select since
     we're only checking one descriptor */

  while ((select_ret = select(sockfd+1, &fd_read, NULL, NULL, &tv)) &&
	 (space_left > 0)) {
    n_read = read(sockfd, cp, space_left);
    if (n_read == 0) {
      close (sockfd);
      sockfd = -1;
      break;
    }
    else if (n_read < 0) {
      trace (NORM, default_trace,
	     "mirrorstatus: Error reading from socket for %s:%d\n",
	     prefix_toa(mirror_prefix), mirror_port);
      goto FAIL;
    }
    cp += n_read;
    space_left -= n_read;
  }

  if (sockfd < 0) {
    /* Alles gute */
  }
  else if (select_ret == 0) {
    /* Select timed out */
    trace (NORM, default_trace,
	   "mirrorstatus: Timed out reading file descriptor for %s:%d\n",
	   prefix_toa(mirror_prefix), mirror_port);
    goto FAIL;
  }
  else if (space_left == 0) {
    /* Ran out of buffer */
    trace (NORM, default_trace,
	   "mirrorstatus: Ran out of buffer space while reading from %s:%d\n",
	   prefix_toa(mirror_prefix), mirror_port);
    goto FAIL;
  }
  else {
    /* Unspecified error */
    trace (NORM, default_trace,
	   "mirrorstatus: Unknown error while reading from %s:%d\n",
	   prefix_toa(mirror_prefix), mirror_port);
    goto FAIL;
  }

  ret = 1;

FAIL:
  if (sockfd >= 0) close (sockfd);

  return (ret);
}

/*
 * mirrorstatus_process_answer
 *
 * When we've gotten an "A" answer back from the remote server,
 * this function parses the output.
 * Returns 0 on failure, 1 on success.
 *
 * Arguments:
 * LINKED_LIST *ll_checkdb - Empty linked list.  We add on each
 *   irr_database_t * that matches the mirror_prefix and mirror_port.
 *   This lets us minimize the number of databases we have to iterate through
 *   for checking answers.
 * char *answer - Where we are going to store the answer we get back.
 *
 */
static int mirrorstatus_process_answer (LINKED_LIST *ll_checkdb, char *answer) {
  char *answer_line, *db_name, *lasts, *lasts2;
  char *p_flag, *p_answer, *p_last_export;
  irr_database_t *db;
  u_long range_start, range_end, last_export;
  int ret = 0;

  /* Realize of course the "answer" is getting horribly mangled by
     all of these strtok's that are being called */

  /* Tokenize on newlines */
  answer_line = strtok_r(answer, "\n", &lasts);
  do {
    db_name = strtok_r (answer_line, ":", &lasts2);
    /* Don't trust the other side to send us non-gibberish */
    if (db_name == NULL) {
      trace (NORM, default_trace,
             "mirrorstatus: *ERROR* Got invalid !j db_name.\n");
      goto FAIL;
    }

    LL_Iterate(ll_checkdb, db) {
      if (!strcasecmp(db->name, db_name)) {
	p_flag = strtok_r (NULL, ":", &lasts2);
	p_answer = strtok_r (NULL, ":", &lasts2);
	p_last_export = strtok_r (NULL, ":", &lasts2);

	if ((p_flag == NULL) || (strlen(p_flag) != 1)) {
	  trace (NORM, default_trace,
		 "mirrorstatus: *ERROR* Got invalid !j answer [2].\n");
	  goto FAIL;
	}

	switch (*p_flag) {
	  case 'Y' :
	  case 'N' :
	    if ((p_answer != NULL) && (sscanf(p_answer, "%lu-%lu", &range_start, &range_end) != 2)) {
	      trace (NORM, default_trace,
		     "mirrorstatus: *ERROR* Invalid range tokens\n");
	      goto FAIL;
	    }
	    last_export = 0;
	    if ((p_last_export != NULL) && (sscanf(p_last_export, "%lu", &last_export) != 1)) {
	      trace (NORM, default_trace,
		     "mirrorstatus: *ERROR* Invalid last export tokens\n");
	      goto FAIL;
	    }
	    db->remote_mirrorstatus = (*p_flag == 'Y') ? MIRRORSTATUS_YES : MIRRORSTATUS_NO;
	    db->remote_oldestjournal = (*p_flag == 'Y') ? range_start : 0;
	    db->remote_currentserial = range_end;
	    db->remote_lastexport    = last_export;
	    break;
	  case 'X' :
	    db->remote_mirrorstatus = MIRRORSTATUS_UNAVAILABLE;
	    db->remote_oldestjournal = 0;
	    db->remote_currentserial = 0;
	    db->remote_lastexport = 0;
	    break;
	  default  :
	    trace (NORM, default_trace, 
	           "mirrorstatus: *ERROR* Invalid answer flag\n");
	    goto FAIL;
	}
	break; /* We only want to go through the loop once per db match */
      }
    }
    answer_line = strtok_r(NULL, "\n", &lasts);
  } while (answer_line != NULL);

  ret = 1;

FAIL:
  return (ret);
}

/*
 * get_remote_mirrorstatus
 *
 * Returns 0 on failure, 1 on success.
 *
 * Gets the remote mirror status.  Since a given irrd client may mirror
 * multiple databases from a given server, its nice to not have to 
 * send individual !j's to get answers.  (!! aside)
 * Thus this function expects a prefix and a port and will examine the
 * database linked list and send queries for all databases that we
 * are mirroring off a given IP address.
 *
 * prefix_t *mirror_prefix - The IP address that we want status for
 * int mirror_port - The TCP port that we are getting the status from.
 *
 */
int get_remote_mirrorstatus (prefix_t *mirror_prefix, int mirror_port) {
  char query[BUFSIZE];
  char answer[GRM_BUFSIZE], *p_dup_answer = NULL, *cp;
  int return_val = 0;
  int answer_length;
  irr_database_t *database;
  LINKED_LIST *ll_checkdb;

  ll_checkdb = LL_Create(0);

  if (!mirrorstatus_buildquery (ll_checkdb, query, BUFSIZE, 
                                mirror_prefix, mirror_port)) {
    LL_Iterate(ll_checkdb, database) {
      database->remote_mirrorstatus = MIRRORSTATUS_FAILED;
      database->remote_oldestjournal = 0;
      database->remote_currentserial = 0;
    }
    goto FAIL;
  }

  if (!mirrorstatus_sendquery_getanswer(query, answer, GRM_BUFSIZE, 
                                        mirror_prefix, mirror_port)) {
    LL_Iterate(ll_checkdb, database) {
      database->remote_mirrorstatus = MIRRORSTATUS_FAILED;
      database->remote_oldestjournal = 0;
      database->remote_currentserial = 0;
      }
    goto FAIL;
  }

  switch (answer[0]) {
    case '\n' : /* RIPE-181 */
    case 'F'  : /* Not Supported */
      LL_Iterate(ll_checkdb, database) {
	database->remote_mirrorstatus = MIRRORSTATUS_UNSUPPORTED;
	database->remote_oldestjournal = 0;
	database->remote_currentserial = 0;
      }
      break;
    case 'D'  : /* Empty query */
      LL_Iterate(ll_checkdb, database) {
	database->remote_mirrorstatus = MIRRORSTATUS_UNAVAILABLE;
	database->remote_oldestjournal = 0;
	database->remote_currentserial = 0;
      }
      break;
    case 'A'  : /* Valid query - here's the answer */
      p_dup_answer = strdup(answer);

      cp = p_dup_answer + 1;
      if (!sscanf (cp, "%d\n", &answer_length)) goto FAIL;
      while (*cp != '\n') cp++;
      cp++;
      cp[answer_length - 1] = '\0'; /* Trims off last NL */

      if (!mirrorstatus_process_answer (ll_checkdb, cp)) {
	trace (NORM, default_trace, "mirrorstatus: Invalid answer follows\n%s\n", answer);
        goto FAIL;
      }
      break;
    default   :
      trace (NORM, default_trace, "mirrorstatus: *ERROR* got gibberish for an answer to !j\n%s\n", answer);
      goto FAIL;
      break; /* NOT REACHED */
  }

  return_val = 1;

FAIL:
  LL_Destroy (ll_checkdb);
  if (p_dup_answer) { free(p_dup_answer); }

  return (return_val);
}

/*
 * uii_mirrorstatus_db
 * 
 * Returns 0 on failure, 1 on success.
 *
 * Arguments:
 * uii_connection_t *uii - Pointer to the outside world.
 * char *db_name - the DB we've been asked about
 *
 * This is a sloppy function, meant to be run by humans.  It will
 * call get_remote_mirrorstatus for a specific database and display
 * the output to uii.  Biggest problem is that it'll !j each time you
 * want to see the data.
 *
 */
void uii_mirrorstatus_db (uii_connection_t *uii, char *db_name) {
  irr_database_t *database;
  uint32_t oldest_journal = 0;
  int ret;
  enum { UNDETERMINED, READONLY, AUTHORITATIVE, MIRROR } status = UNDETERMINED;

  if ((database = find_database (db_name)) != NULL) {
    uii_add_bulk_output (uii, "%s (", db_name);
    if (database->mirror_prefix == NULL) {
      if ((database->flags & IRR_AUTHORITATIVE) == IRR_AUTHORITATIVE) {
	status = AUTHORITATIVE;
	uii_add_bulk_output (uii, "Authoritative)\r\n", db_name);
      }
      else {
	status = READONLY;
	uii_add_bulk_output (uii, "Read only)\r\n", db_name);
      }
    }
    else {
      status = MIRROR;
      uii_add_bulk_output (uii, "Mirror)\r\n", db_name);
    }

    uii_add_bulk_output (uii, "\r\nLocal Information:\r\n");

    if (find_oldest_serial(database->name, JOURNAL_OLD, &oldest_journal) != 1)
      if (find_oldest_serial(database->name, JOURNAL_NEW, &oldest_journal) != 1)
	 oldest_journal = 0;

    if (oldest_journal) {
      uii_add_bulk_output (uii, "  Oldest journal serial number: %lu\r\n",
			   oldest_journal);
    }
    uii_add_bulk_output (uii, "  Current serial number: %lu\r\n",
                         database->serial_number);

    if (status == MIRROR) {
      uii_add_bulk_output (uii, "\r\nRemote Information:\r\n");
      uii_add_bulk_output (uii, "  Mirror host: %s:%d\r\n",
                           prefix_toa(database->mirror_prefix),
			   database->mirror_port);

      ret = get_remote_mirrorstatus(database->mirror_prefix, database->mirror_port);
      if (ret != 1) {
	uii_add_bulk_output (uii, "  *WARNING* Error getting status for remote database\r\n");
	uii_add_bulk_output (uii, "  *WARNING* See logfile for further info.\r\n", db_name);
      }

      else {
	switch (database->remote_mirrorstatus) {
	  case MIRRORSTATUS_UNDETERMINED:
	    uii_add_bulk_output (uii, "  Status undetermined.  See logfiles for further info.\r\n");
	    break;
	  case MIRRORSTATUS_FAILED:
	    uii_add_bulk_output (uii, "  Failed to connect to remote database.  See logfiles for further info.\r\n");
	    break;
	  case MIRRORSTATUS_UNSUPPORTED:
	    uii_add_bulk_output (uii, "  Remote status query unsupported.\r\n");
	    break;
	  case MIRRORSTATUS_UNAVAILABLE:
	    uii_add_bulk_output (uii, "  Remote status information unavailable.\r\n");
	    break;
	  case MIRRORSTATUS_YES:
	    uii_add_bulk_output (uii, "  Mirrorable.\r\n");
	    uii_add_bulk_output (uii, "  Oldest journal serial number: %d.\r\n",
	                         database->remote_oldestjournal);
	    uii_add_bulk_output (uii, "  Current serial number: %d.\r\n",
	                         database->remote_currentserial);
	    if (database->remote_lastexport > 0)
	      uii_add_bulk_output (uii, "  Last exported at serial number: %d.\r\n",
				   database->remote_lastexport);

	    if (database->serial_number < database->remote_oldestjournal) {
	      uii_add_bulk_output (uii, "  *WARNING* Remote oldest journal > our CURRENTSERIAL.\r\n");
	      uii_add_bulk_output (uii, "  *WARNING* Mirroring will fail.  Please reseed your database.\r\n");
	    }
	    if (database->serial_number > database->remote_currentserial) {
	      uii_add_bulk_output (uii, "  *WARNING* Remote CURRENTSERIAL < our CURRENTSERIAL.\r\n");
	      uii_add_bulk_output (uii, "  *WARNING* Mirroring will fail.  Please reseed your database.\r\n");
	      uii_add_bulk_output (uii, "  *WARNING* (Remote database corruption suspected.  Contact remote db-admin.)\r\n");
	    }
	    break;
	  case MIRRORSTATUS_NO:
	    uii_add_bulk_output (uii, "  Not Mirrorable.\r\n");
	    uii_add_bulk_output (uii, "  Current serial number: %d.\r\n",
	                         database->remote_currentserial);
	    if (database->remote_lastexport > 0)
	      uii_add_bulk_output (uii, "  Last exported at serial number: %d.\r\n",
				   database->remote_lastexport);
	    if (database->serial_number > database->remote_currentserial) {
	      uii_add_bulk_output (uii, "  *WARNING* Remote CURRENTSERIAL < our CURRENTSERIAL.\r\n");
	      uii_add_bulk_output (uii, "  *WARNING* (Remote database corruption suspected.  Contact remote db-admin.)\r\n");
	    }
	    break;
	  default:
	    uii_add_bulk_output (uii, "  *ERROR* Invalid value in databases remote_mirrorstatus field\r\n");
	    break;
	}
      }
    }
  }
  else {
    uii_add_bulk_output (uii, "Database %s does not exist.\r\n", db_name);
  }
}
