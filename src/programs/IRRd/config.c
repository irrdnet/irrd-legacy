/*
 * $Id: config.c,v 1.25 2002/10/17 20:02:29 ljb Exp $
 * originally Id: config.c,v 1.50 1998/07/20 01:22:03 labovit Exp 
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <glib.h>
#include <irrdmem.h>

#include "config_file.h"
#include "irrd.h"

find_filter_t o_filter[] = {
  {"unused",	  XXX_F	         },	
  {"autnum",      AUT_NUM_F      },
  {"as-set",      AS_SET_F       },
  {"mntner",      MNTNER_F       },
  {"route",       ROUTE_F        },
  {"route6",	  ROUTE6_F	 },
  {"route-set",   ROUTE_SET_F    },
  {"inet-rtr",    INET_RTR_F     },
  {"rtr-set",     RTR_SET_F      },
  {"person",      PERSON_F       },
  {"role",        ROLE_F         },
  {"filter-set",  FILTER_SET_F   },
  {"peering-set", PEERING_SET_F  },
  {"key-cert",    KEY_CERT_F     },
  {"dictionary",  DICTIONARY_F   },
  {"repository",  REPOSITORY_F   },
  {"inetnum",     INETNUM_F      },
  {"inet6num",    INET6NUM_F     },
  {"as-block",	  AS_BLOCK_F	 },
  {"domain",      DOMAIN_F       },
  {"limerick",    LIMERICK_F     },
  {"ipv6-site",   IPV6_SITE_F    },
};

u_int as_lookup_fn (u_short *as1, u_short *as2) {  
  return ((!((*as1) - (*as2))));
}

u_int as_hash_fn (u_short *as, u_int size)
{
    u_int value;

    value = *as % size;
    return (value);
}

#ifdef HJHJHH
void config_create_default () {
  char *tmp;
  CONFIG.ll_modules = LL_Create (0);

  tmp = strdup ("#####################################################################");
  config_add_module (0, "comment", get_comment_config, tmp);

  tmp = malloc (512);
  sprintf (tmp, "# IRRd -- version %s ", IRRD_VERSION);
  config_add_module (0, "comment", get_comment_config, tmp);
		
  tmp = strdup  ("#####################################################################");
  config_add_module (0, "comment", get_comment_config, tmp);
  config_add_module (0, "comment", get_comment_config, strdup ("!"));
  config_add_module (0, "comment", get_comment_config, strdup ("!"));  

  /*config_add_module (0, "irr_port", get_config_irr_port, NULL); 
  config_add_module (0, "irr_directory", get_config_irr_directory, NULL); 
  */
}
#endif 

void get_config_irr_expansion_timeout () {
  config_add_output ("irr_expansion_timeout %d\r\n", IRR.expansion_timeout);
}

/* irr_expansion_timeout %d 
 * number of seconds a "!i" set expansion is allowed to take before timing out
 * and aborting.  A value of zero indicates no timeout.
 */
int config_irr_expansion_timeout (uii_connection_t *uii, int timeout) {

  if (timeout < 0) {
    config_notice (NORM, uii, "CONFIG Error -- expansion must non-negative\n", timeout);
    return (-1);
  }
  IRR.expansion_timeout = timeout;
  config_add_module (0, "irr_expansion_timeout", get_config_irr_expansion_timeout, NULL); 
  return (1);
}

void get_config_irr_max_connections () {
  config_add_output ("irr_max_connections %d\r\n", IRR.max_connections);
}

/* irr_max_con %d 
 * maximum number of silumtaneous open TCP connections to our whois port
 */
int config_irr_max_con (uii_connection_t *uii, int max) {

  if ((max <= 0) || (max >= 10000)) {
    config_notice (NORM, uii, "CONFIG Error -- usage: irr_max_connections <1-1000>\n", max);
    return (-1);
  }
  IRR.max_connections = max;
  config_add_module (0, "irr_max_connections", get_config_irr_max_connections, NULL); 
  return (1);
}

/* return the irr_port (whois) on which we are listening */
void get_config_irr_port () {
  if (IRR.irr_port_access == 0)
    config_add_output ("irr_port %d\r\n", IRR.irr_port);
  else
    config_add_output ("irr_port %d access %d\r\n", IRR.irr_port, IRR.irr_port_access);
}

/* irr_port %d
 * The port we listen on for RAWhoisd style machine queries
 */
void config_irr_port (uii_connection_t *uii, int port, int access_list) {
  if ((port <= 0) || (port > 60000)) {
    config_notice (NORM, uii, "CONFIG Error -- usage: irr_port <port_num> [access <acccess list>]\n");
    return;
  }
  IRR.irr_port = port;

  if ((access_list < 1) || (access_list > 101)) {
    config_notice (NORM, uii, "CONFIG Error -- usage: irr_port <port_num> [access <acccess list>]\n");
    return;
  }

  IRR.irr_port_access = access_list;
  
  config_add_module (0, "irr_port", get_config_irr_port, NULL); 
}

/* irr_port %d
 * The port we listen on for RAWhoisd style machine queries
 */
void config_irr_port_2 (uii_connection_t *uii, int port) {
  if ((port <= 0) || (port > 60000)) {
    config_notice (NORM, uii, "CONFIG Error -- usage: irr_port <port_num> [access <acccess list>]\n");
    return;
  }
  IRR.irr_port = port;

  /* reset to no access list filtering */
  IRR.irr_port_access = 0;

  config_add_module (0, "irr_port", get_config_irr_port, NULL); 
}

void get_config_irr_mirror_interval () {
  config_add_output ("irr_mirror_interval %d\r\n", IRR.mirror_interval);
}

void config_irr_mirror_interval (uii_connection_t *uii, int interval) {
  IRR.mirror_interval = interval;
  config_add_module (0, "irr_mirror_interval", get_config_irr_mirror_interval, NULL); 
}

void get_config_irr_directory () {
  config_add_output ("irr_directory %s\r\n", IRR.database_dir);
}

int config_irr_directory (uii_connection_t *uii, char *directory) {
  char status_filename[BUFSIZE];

  if (IRR.database_dir != NULL) 
    irrd_free(IRR.database_dir);

  IRR.database_dir = directory;
  config_add_module (0, "irr_directory", get_config_irr_directory, NULL); 

  sprintf(status_filename, "%s/IRRD_STATUS", directory);
  if (IRR.statusfile) {
    CloseStatusFile(IRR.statusfile);
    irrd_free(IRR.statusfile);
  }
  IRR.statusfile = InitStatusFile(status_filename);

  return (1);
}

void get_config_irr_database (irr_database_t *database) {
  int atts = 0;

  if (database->flags & IRR_AUTHORITATIVE) {
    config_add_output ("irr_database %s authoritative\r\n", 
		       database->name);
    atts = 1;
  }

  if (database->mirror_prefix != NULL) {
    config_add_output ("irr_database %s mirror_host %s %d\r\n", 
		       database->name,
		       prefix_toa (database->mirror_prefix),
		       database->mirror_port);
    atts =1;
  }

  /* default protocol is 1, if not version 1, save to config */
  if (database->mirror_protocol != 1) {
    config_add_output ("irr_database %s mirror_protocol %d\r\n",
                       database->name,
                       database->mirror_protocol);
    atts =1;
  }

  if (database->access_list != 0) {
    config_add_output ("irr_database %s access %d\r\n", 
		       database->name,
		       database->access_list);
    atts =1;
  }

  if (database->write_access_list != 0) {
    config_add_output ("irr_database %s write-access %d\r\n", 
		       database->name,
		       database->write_access_list);
    atts =1;
  }

  if (database->cryptpw_access_list != 0) {
    config_add_output ("irr_database %s cryptpw-access %d\r\n",
                       database->name,
                       database->cryptpw_access_list);
    atts =1;
  }

  if (database->mirror_access_list != 0) {
    config_add_output ("irr_database %s mirror-access %d\r\n", 
		       database->name,
		       database->mirror_access_list);
    atts =1;
  }

  if (database->compress_script != 0) {
    config_add_output ("irr_database %s compress_script %s\r\n",
                       database->name,
                       database->compress_script);
    atts =1;
  }

  if (database->export_timer != NULL) {
    config_add_output ("irr_database %s export %d\r\n", 
		       database->name, 
		       database->export_timer->time_interval_base);
    atts =1;
  }

  if ((database->clean_timer != NULL) && (!database->no_dbclean)) {
    config_add_output ("irr_database %s clean %d\r\n", 
		       database->name, 
		       database->clean_timer->time_interval_base);
    atts =1;
  }

  if (database->no_dbclean) {
    config_add_output ("irr_database %s no-clean\r\n", database->name);
    atts =1;
  }

  if (database->flags & IRR_NODEFAULT) {
    config_add_output ("irr_database %s no-default\r\n", 
		       database->name);
    atts = 1;
  }

  if (database->flags & IRR_ROUTING_TABLE_DUMP) {
    config_add_output ("irr_database %s routing-table-dump\r\n",
			database->name);
    atts = 1;
  }

  if (database->flags & IRR_ROA_DATA) {
    config_add_output ("irr_database %s roa-data\r\n",
			database->name);
    atts = 1;
  }
  
  if (atts == 0) 
    config_add_output ("irr_database %s\r\n", database->name);
}


int config_irr_database_nodefault (uii_connection_t *uii, char *name) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  irrd_free(name);
  database->flags |= IRR_NODEFAULT;

  return (1);
}

void get_config_roadisclaimer () {
  char *st;

  LL_Iterate (IRR.ll_roa_disclaimer, st) {
    config_add_output ("roa-disclaimer %s\r\n", st);
  }
}

/* roa-disclaimer %s */
int config_roa_disclaimer (uii_connection_t *uii, char *st) {

  if (IRR.ll_roa_disclaimer == NULL)
    IRR.ll_roa_disclaimer = LL_Create (0);
  if (st == NULL) 
    st = " ";
  LL_Add (IRR.ll_roa_disclaimer, strdup (st));
  config_add_module (0, "roadisclaimer", get_config_roadisclaimer, NULL);
  return (1);
}

int config_irr_database_roa_data (uii_connection_t *uii, char *name) {
  irr_database_t *database = NULL;

  if (IRR.roa_database != NULL) {
    config_notice (ERROR, uii, "ROA database %s already exists!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  database = new_database (name);
  irrd_free(name);
  IRR.roa_database = database;
  database->flags |= IRR_ROA_DATA;

  return (1);
}

/* irr_database %s export %d [%s] */
int config_irr_database_export (uii_connection_t *uii, char *name, int interval, int num, char *filename) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    if (num == 1)
      irrd_free(filename);
    return (-1);
  }

  /* once a day */
  database->export_timer = (mtimer_t *) 
  New_Timer (irr_export_timer, interval, "IRR export", database);
  timer_set_jitter (database->export_timer, (int) (interval / 3.0));
  Timer_Turn_ON ((mtimer_t *) database->export_timer);
  
  if (num == 1) {
    database->export_filename = filename;
  }

  irrd_free(name); 
  return (1);
}

/* Parse the irrd.conf command 'irr_path %s' and save the specified 
 * path component (which can be used to find irrdcacher, wget, 
 * ripe2rpsl ...).  The specified path component is added to the
 * users PATH environment variable when external binaries are invoked.
 *
 * irrd.conf command:
 * irr_path %s
 *
 * eg, 'irr_path :/irr/bin:/usr/local/joe/mybin
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -a path component (path)
 *
 * Return:
 *  -void
 *
 * set's 'IRR.path' to (path)
 */
int config_irr_path (uii_connection_t *uii, char *path) {
  char buf[BUFSIZE+2];

  if (path == NULL) {
    config_notice (ERROR, uii, "NULL irr path component!\r\n");
    return -1;
  }
  else if (strlen (path) > BUFSIZE) {
    config_notice (ERROR, uii, "path component too large! MAX(%d)\r\n", BUFSIZE);
    irrd_free(path);
    return -1;
  }

  irrd_free(IRR.path);

  /* make sure path component begin's with a ':' */
  if (*path != ':')
    sprintf (buf, ":%s", path);
  else
    strcpy (buf, path);

  IRR.path = strdup (buf);
  irrd_free(path);

  /* let the user know we have updated the path */
  config_notice (NORM, uii, "path component set to (%s)\r\n", IRR.path);

  return 1;
}


/* Reset the ftp URL for DB (name)
 *
 * URL to be used by irrdcacher for remote DB retrievel.  
 *
 * (name) must not be an authoritative DB.
 *
 * irrd.conf command:
 * irr_database %s remote_ftp_url %s
 *
 * eg, 'irr_database ripe remote_ftp_url ftp://ftp.ripe.net/ripe/dbase
 * 
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB name to associate the ftp URL with (name)
 *  -ftp URL to use for remote DB refreshes (dir)
 *
 * Return:
 *  -1 if there were no errors in the command
 *  --1 otherwise
 *
 *  set's the 'remote_ftp_url' irr_database_t member with (dir)
 */
int config_irr_remote_ftp_url (uii_connection_t *uii, char *name, char *dir) {
  int ret_code = 1;
  irr_database_t *db;
  regex_t url_re;
  char *ftp_url = "^ftp://[^ \t/]+/[[:graph:]]+$";

  /* Do we know about this DB? */
  if ((db = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    ret_code = -1;
    goto INPUT_ERROR;
  }

  /* make sure this DB is not authoritative */
  if (db->flags & IRR_AUTHORITATIVE) {
    config_notice (ERROR, uii, "*ERROR* database %s is authoritative !\r\n",
		   name);
    ret_code = -1;
    goto INPUT_ERROR;
  }

  /* make sure the remote url is in valid URL format */

  /* compile our reg ex */
  regcomp (&url_re,  ftp_url, REG_EXTENDED|REG_NOSUB);

  /* check the URL format */
  if (regexec (&url_re, dir, 0, NULL, 0)) {
    config_notice (ERROR, uii, "Invalid URL format (%s)! (ftp://<remote server>/<remote directory>)\r\n", dir);
    ret_code = -1;
    }
  else {
    /* make purify happy */
    irrd_free(db->remote_ftp_url);
    db->remote_ftp_url = strdup (dir);
    config_notice (NORM, uii, "Remote ftp URL (%s) set for (%s)\r\n", dir, name);
  }
  regfree (&url_re);

INPUT_ERROR:
  irrd_free(dir);
  irrd_free(name);

  return ret_code;
}


/* Set the repository signature hex ID for DB (name) to (hexid).
 *
 * The hexid can be in "[[:xdigit:]]{8}" format or "0x[[:xdigit:]]{8}".
 * Internally the hexid will be stored as "0x[[:xdigit:]]{8}".
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB to set the repository hex ID (name)
 *  -hex ID of the repository signature (hexid)
 * 
 * Return:
 *  -1 if the operation was successful
 *  --1 otherwise
 */
int config_irr_repos_hexid (uii_connection_t *uii, char *name, char *hexid) {
  char hex_out[16];
  int ret_code = -1;
  irr_database_t *db;
  regex_t re;
  char *hex_num  = "^(0x)?[[:xdigit:]]{8}$";
  
  /* Do we know about this DB? */
  if ((db = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    goto INPUT_ERROR;
  }

  /* compile the regex */
  regcomp (&re, hex_num, REG_EXTENDED|REG_NOSUB);

  /* see if the (hexid) is well-formed */
  if (regexec (&re, hexid, 0, NULL, 0) == 0) {
    if (*hexid == '0')
      strcpy (hex_out, hexid);           /* perfect! */
    else
      sprintf (hex_out, "0x%s", hexid);  /* need to add a leading '0x' */

    /* replace the old repos hexid with the new */
    irrd_free(db->repos_hexid);
    db->repos_hexid = strdup (hex_out);
    ret_code = 1;
    config_notice (NORM, uii, 
		   "Repository hex ID (%s) set for (%s)\r\n", hexid, name);

  }
  else
    config_notice (NORM, uii, 
		   "Malformed hex ID (%s) expecting \"[[:xdigit:]]{8}\"\r\n", hexid);

  regfree (&re);

INPUT_ERROR:
  irrd_free(hexid);
  irrd_free(name);

  return ret_code;
}


/* Set the PGP password for DB (name)'s repository object.
 *
 * (passwd) can have spaces and may be enclosed in "'s.  The
 * enclosing "'s (if any) are not considered part of the password.
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB to set pgp password (name)
 *  -password for the repository object (passwd)
 * 
 * Return:
 *  -1 if the operation was successful
 *  --1 otherwise
 */
int config_irr_pgppass (uii_connection_t *uii, char *name, char *passwd) {
  char pgppass[BUFSIZE+1];
  int ret_code = -1, n, offset;
  irr_database_t *db;
  regmatch_t rm[5];
  regex_t re;
  char *pass  = "^((\"(.*)\")|([^\" \t\n]*))[ \t]*\n?$";

  /* Do we know about this DB? */
  if ((db = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    goto INPUT_ERROR;
  }

  /* compile the regex */
  regcomp (&re, pass, REG_EXTENDED);

  /* see if the (password) is well-formed */
  if (regexec (&re, passwd, 5, rm, 0) == 0) {
    if (rm[3].rm_so != -1)
      offset = 3;
    else
      offset = 4;
    if ((n = (rm[offset].rm_eo - rm[offset].rm_so)) <= BUFSIZE) {
      strncpy (pgppass, (char *) (passwd + rm[offset].rm_so), n);
      pgppass[n] = '\0';
      irrd_free(db->pgppass);
      db->pgppass = strdup (pgppass);
      ret_code = 1;
      config_notice (NORM, uii, "PGP password (%s) set for (%s)\r\n", pgppass, name);
    }
    else
      config_notice (ERROR, uii, "PGP password is too long!  MAX(%d)\r\n", BUFSIZE);
  }
  else
    config_notice (ERROR, uii, "Malformed PGP password\r\n");

  regfree (&re);

INPUT_ERROR:
  irrd_free(passwd);
  irrd_free(name);

  return ret_code;
}

/* Add (dbname) to the list of rpsdist managed DB's.
 *
 * sample irrd.conf entry:
 * rpsdist_database bell

 * rpsdist does not trust DB's defined this way.  DB's 
 * will normally be added this way dynamically from the 
 * !C command.
 * 
 * A corresponding entry is made to irrd so that it will know
 * about the DB and will accept !ms...!me commands from rpsdist.
 * 
 * Input:
 *  -name of the DB to add to the rpsdist list of DB's
 *
 * Return:
 *  -1 if no errors
 *  --1 otherwise
 */
int config_rpsdist_database (uii_connection_t *uii, char *dbname) {
  int ret_code = -1;
  irr_database_t *db;

  /* look up the DB */
  db = find_database (dbname);

  /* mis-configuration checking */
  if (db != NULL &&
      db->mirror_prefix != NULL) {
    config_notice (NORM, uii, "CONFIG Error -- conflicting "
		   "\"irr_database/rpsdist_database\" configurations "
		   "for DB %s\r\n", dbname);
    goto CLEAN_UP;
  }

  if ((ret_code = config_irr_database_authoritative (uii, strdup (dbname))) < 0)
    goto CLEAN_UP;

  /* look up the DB */
  if (db == NULL)
    db = find_database (dbname);
  db->rpsdist_flag = 1;

CLEAN_UP:

  /* clean-up */
  irrd_free(dbname);

  return ret_code;
}

/* Accept rpsdist connections from (host).
 *
 * sample irrd.conf entry:
 * rpsdist accept 198.108.60.1
 *
 * Allows mirror-only hosts to connect to us and get
 * transaction updates from us. (eg, a router.  any host
 * that does not have an DB but wishes to have a local
 * copy of the IRR)
 * 
 * Input:
 *  -host to accept connections from which we will send 
 *   transactions but not accept updates.
 *
 * Return:
 *  -1 if no errors
 *  --1 otherwise
 */
int config_rpsdist_accept (uii_connection_t *uii, char *host) {
  int ret_code = 1;

  /* host */
  if (string_toprefix (host, default_trace) == NULL) {
    config_notice (NORM, uii, "CONFIG Error -- could not resolve %s\r\n", host);
    ret_code = -1;
  }
  
  irrd_free(host);
  
  return ret_code;
}

/* Accept rpsdist connections from (host) and also
 * accept poll requests and unicasted transactions.
 *
 * sample irrd.conf entry:
 * rpsdist ripe accept 198.108.60.1
 *
 * Allows a remote host to send us rpsdist updates
 * from more than 1 host or from a remote host that
 * we don't trust.
 * 
 * Input:
 *  -host to accept connections from which we will send 
 *   transactions and accept updates.
 *  -name of the DB to add to the rpsdist list of DB's
 *
 * Return:
 *  -1 if no errors
 *  --1 otherwise
 */
int config_rpsdist_accept_database (uii_connection_t *uii, char *dbname,
				    char *host) {
  int ret_code = -1;
  irr_database_t *db;
  
  /* look up the DB */
  db = find_database (dbname);

  /* mis-configuration checking */
  if (db != NULL &&
      db->mirror_prefix != NULL) {
    config_notice (NORM, uii, "CONFIG Error -- conflicting "
		   "\"irr_database/rpsdist_database\" configurations "
		   "for DB %s\r\n", dbname);
    goto CLEAN_UP;
  }

  /* host and port */
  if (string_toprefix (host, default_trace) == NULL) {
    config_notice (NORM, uii, "CONFIG Error -- could not resolve %s\r\n", host);
    goto CLEAN_UP;
  }
  
  if ((ret_code = config_irr_database_authoritative (uii, strdup (dbname))) < 0)
    goto CLEAN_UP;

  /* look up the DB */
  if (db == NULL)
    db = find_database (dbname);

  /* if we get here all is well */
  db->rpsdist_flag = 1;
  db->rpsdist_accept_host = strdup (host);
  
CLEAN_UP:

  /* clean-up */
  irrd_free(dbname);
  irrd_free(host);
  
  return ret_code;
}

/* Parse the conf command line for "rpsdist_database %s trusted %s %d".
 *
 * eg, rpsdist_database ripe trusted whois.ripe.net 43
 *
 * The DB becomes trusted for ripe which allows rpsdist to accept
 * signed updates to be accepted without auth checking.  The host
 * and port tell rpsdist where to connect for floods/polls.
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB to associate the information to (dbname)
 *  -host and port to connect to for floods/polls (host, port)
 *
 * Return:
 *  -1 if there were no errors
 *  --1 otherwise
 */
int config_rpsdist_database_trusted (uii_connection_t *uii, char *dbname, 
				     char *host, int port) {
  int ret_code = -1;
  irr_database_t *db;
  
  /* look up the DB */
  db = find_database (dbname);

  /* mis-configuration checking */
  if (db != NULL          &&
      (db->rpsdist_trusted ||
       db->rpsdist_auth    ||
       db->mirror_prefix != NULL)) {
    config_notice (NORM, uii, "CONFIG Error -- conflicting "
		   "\"irr_database/rpsdist_database\" configurations "
		   "for DB %s\r\n", dbname);
    goto CLEAN_UP;
  }    

  /* host */
  if (string_toprefix (host, default_trace) == NULL) {
    config_notice (NORM, uii, "CONFIG Error -- could not resolve %s\r\n", host);
    goto CLEAN_UP;
  }
  
  /* if we get here there were no errors */
  config_notice (NORM, uii, 
		 "rpsdist DB (%s) set values: (host, port)="
		 "(%s, %d)\r\n", dbname, host, port);

  /* let irrd know about this DB */
  if ((ret_code = config_irr_database_authoritative (uii, strdup (dbname))) < 0)
    goto CLEAN_UP;

  /* look up the DB */
  if (db == NULL)
    db = find_database (dbname);
  
  /* set the values */
  db->rpsdist_flag    = 1;
  db->rpsdist_trusted = 1;
  db->rpsdist_host    = strdup (host);
  db->rpsdist_port    = port;
  
CLEAN_UP:

  /* clean-up */
  irrd_free(dbname);
  irrd_free(host);

  return ret_code;
}

/* Parse the conf command line for "rpsdist_database %s authoritative %s".
 *
 * eg, rpsdist_database radb authoritative "Iftdpa!"
 *
 * radb becomes authoritative which allows rpsdist to accept
 * updates.  The password is for the signing key which
 * is needed by rpsdist when flooding.
 *
 * Input:
 *  -data struct for communicating with UII users (uii)
 *  -DB to associate the information to (dbname)
 *  -pgp password for the signing key (passwd)
 *
 * Return:
 *  -1 if there were no errors
 *  --1 otherwise
 */
int config_rpsdist_database_authoritative (uii_connection_t *uii, char *dbname, 
					   char *passwd) {
  int ret_code = -1, n, offset;
  irr_database_t *db;
  char pgppass[BUFSIZE+1];
  regmatch_t rm[5];
  regex_t re_pass;
  char *pass = "^((\"(.*)\")|([^\" \t\n]*))[ \t]*\n?$";
  
  /* look up the DB */
  db = find_database (dbname);

  /* mis-configuration checking */
  if (db != NULL          &&
      (db->rpsdist_trusted ||
       db->mirror_prefix != NULL)) {
    config_notice (NORM, uii, "CONFIG Error -- conflicting "
		   "\"irr_database/rpsdist_database\" configurations "
		   "for DB %s\r\n", dbname);
    goto CLEAN_UP;
  }

  /* PGP password */
  regcomp (&re_pass, pass, REG_EXTENDED);

  /* see if the (password) is well-formed */
  if (regexec (&re_pass, passwd, 5, rm, 0) == 0) {
    if (rm[3].rm_so != -1)
      offset = 3;
    else
      offset = 4;

    if ((n = (rm[offset].rm_eo - rm[offset].rm_so)) <= BUFSIZE) {
      strncpy (pgppass, (char *) (passwd + rm[offset].rm_so), n);
      pgppass[n] = '\0';
      ret_code = 1;
    }
    else
      config_notice (ERROR, uii, "PGP password is too long!  MAX(%d)\r\n", 
		     BUFSIZE);
  }
  else
    config_notice (ERROR, uii, "Malformed PGP password\r\n");

  regfree (&re_pass);
  if (ret_code < 0)
    goto CLEAN_UP;

  /* if we get here there were no errors */
  config_notice (NORM, uii, 
		 "rpsdist DB (%s) set values: (PGPPASS)="
		 "(%s)\r\n", dbname, pgppass);

  /* let irrd know about this DB */
  if ((ret_code = config_irr_database_authoritative (NULL, strdup (dbname))) < 0)
    goto CLEAN_UP;

  /* look up the DB */
  if (db == NULL)
    db = find_database (dbname);
  
  /* set the values */
  db->rpsdist_flag = 1;
  db->rpsdist_auth = 1;
  db->pgppass      = strdup (pgppass);

CLEAN_UP:
  /* clean-up */
  irrd_free(dbname);
  irrd_free(passwd);

  return ret_code;
}

/* config no irr_database %s authoritative */
int no_config_irr_database_authoritative (uii_connection_t *uii, char *name) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s no authoritative\n", name);

  database->flags = 0;
  irrd_free(name);
  return (1);
}

/* config irr database %s authoritative */
int config_irr_database_authoritative (uii_connection_t *uii, char *name) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL)
    config_irr_database (uii, strdup (name));

  if ((database = find_database (name)) == NULL) {
    irrd_free(name);
    return (-1);
  }

  /* can't be authoritative and mirror at the same time! */
  if (database->mirror_prefix != NULL) {
    config_notice (ERROR, uii, "*ERROR* database %s is configured for mirroring!\r\n",
		   name);
    irrd_free(name);
    return (-1);
  }

  /* since we're authoritative, turn on cleaning by default */
  if ((database->clean_timer == NULL) && (!database->no_dbclean)) {
    database->clean_timer = (mtimer_t *) 
    New_Timer (irr_clean_timer, 60*60*24*2, "IRR dbclean", database);
    timer_set_jitter (database->clean_timer, 60*60*1);
    Timer_Turn_ON ((mtimer_t *) database->clean_timer);
  }

  trace (NORM, default_trace, "CONFIG %s authoritative\n", name);
  database->flags |= IRR_AUTHORITATIVE;
  config_add_module (0, "irr_database", get_config_irr_database, database); 
  irrd_free(name);
  return (1);
}

/* config irr_database %s mirror %s<hostname> %d<port> */
int config_irr_database_mirror (uii_connection_t *uii, char *name,
				 char *host, int port) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL)
    config_irr_database (uii, strdup (name));

  if ((database = find_database (name)) == NULL) {
    irrd_free(name);
    irrd_free(host);
    return (-1);
  }

  if (database->flags &= IRR_AUTHORITATIVE) {
    config_notice (ERROR, uii, "*ERROR* database %s is authoritative!\r\n", name);
    irrd_free(name);
    irrd_free(host);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s mirror\n", name);

  if ((database->mirror_prefix = string_toprefix (host, default_trace)) == NULL) {
    config_notice (NORM, uii, "CONFIG Error -- could not resolve %s\r\n", host);
    irrd_free(name);
    irrd_free(host);
    return (-1);
  }
  database->mirror_port = port;
  database->mirror_protocol = 1;	/* default mirror protocol is 1 */

  database->mirror_timer = (mtimer_t *)
    New_Timer (irr_mirror_timer, IRR.mirror_interval, "IRR Mirror", database);
  timer_set_jitter (database->mirror_timer, 40);
  Timer_Turn_ON ((mtimer_t *) database->mirror_timer);

  /* since we're mirroring, turn on cleaning by default */
  if ((database->clean_timer == NULL) && (!database->no_dbclean)) {
    database->clean_timer = (mtimer_t *) 
    New_Timer (irr_clean_timer, 60*60*24*2, "IRR dbclean", database);
    timer_set_jitter (database->clean_timer, 60*60*1);
    Timer_Turn_ON ((mtimer_t *) database->clean_timer);
  }

  irrd_free(name);
  irrd_free(host);
  return (1);
}

/* config irr_database %s access %d
 *  Control access to database
 */
int config_irr_database_access (uii_connection_t *uii, char *name, int num) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) 
    config_irr_database (uii, strdup (name));

  if ((database = find_database (name)) == NULL) {
    irrd_free(name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s access-list %d\n", name, num);
  config_add_module (0, "access", get_config_irr_database, database); 

  database->access_list = num;
  irrd_free(name);
  return (1);
}

/* config irr_database %s mirror_protocol %d */
int config_irr_database_mirror_protocol (uii_connection_t *uii, char *name, int num) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free (name);
    return (-1);
  }

  if (num != 1 && num != 3) {
    config_notice (ERROR, uii, "Unsupported mirror protocol type %d.\r\n", num);
    irrd_free (name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s mirror protocol %d\n", name, num);
  config_add_module (0, "mirror protocol", get_config_irr_database, database);

  database->mirror_protocol = num;
  irrd_free (name);
  return (1);
}

/* config irr_database %s mirror-access %d */
int config_irr_database_mirror_access (uii_connection_t *uii, char *name, int num) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free (name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s access-list mirror %d\n", name, num);
  config_add_module (0, "mirror access", get_config_irr_database, database);

  database->mirror_access_list = num;
  irrd_free (name);
  return (1);
}

/* config irr_database %s write-access %d */
int config_irr_database_access_write (uii_connection_t *uii, char *name, int num) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s access-list write %d\n", name, num);
  config_add_module (0, "write access", get_config_irr_database, database); 

  database->write_access_list = num;
  irrd_free(name);
  return (1);
}

/* config irr_database %s cryptpw-access %d */
int config_irr_database_access_cryptpw (uii_connection_t *uii, char *name, int num) {
  irr_database_t *database = NULL;
                                                                                
  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }
                                                                                
  trace (NORM, default_trace, "CONFIG %s access-list cryptpw %d\n", name, num);
  config_add_module (0, "cryptpw access", get_config_irr_database, database);
                                                                                
  database->cryptpw_access_list = num;
  irrd_free(name);
  return (1);
}

/* config irr_database %s compress_script %s */
int config_irr_database_compress_script (uii_connection_t *uii, char *name, char *script) {
  irr_database_t *database = NULL;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  trace (NORM, default_trace, "CONFIG %s compress_script %s\n", name, script);
  config_add_module (0, "compress script", get_config_irr_database, database);

  database->compress_script = script;
  irrd_free(name);
  return (1);
}

/* no config irr_database %s */
int no_config_irr_database (uii_connection_t *uii, char *name) {
  irr_database_t *db;

  if ((db = find_database (name)) == NULL) {
    irrd_free(name);
    config_notice (NORM, uii, "CONFIG no database %s -- database not found!\r\n", db->name);
    return (-1);
  }

  config_notice (NORM, uii, "CONFIG database %s deleted\r\n", db->name);
  LL_Remove (IRR.ll_database, db);
  irr_update_lock (db);
  radix_flush(db->radix_v4);
  radix_flush(db->radix_v6);
  g_hash_table_destroy(db->hash);
  g_hash_table_destroy(db->hash_spec);
  irrd_free(db->name);
  irr_update_unlock (db);
  pthread_mutex_destroy(&db->mutex_lock);
  pthread_mutex_destroy(&db->mutex_clean_lock);
  irrd_free(db);
  irrd_free(name);
  return (1);
}

/* config irr_database xxxx */
int config_irr_database (uii_connection_t *uii, char *name) {
  irr_database_t *database;

  database = find_database (name);

  if (database == NULL) {
    database = new_database (name);
    LL_Add (IRR.ll_database, database);
  }
  config_add_module (0, "irr_database", get_config_irr_database, database); 
  irrd_free(name);
  return (1);
}

void get_config_server_debug () {
  config_add_output ("debug server file-name %s\r\n", 
		     default_trace->logfile->logfile_name);

  if ((strcmp ("stdout", default_trace->logfile->logfile_name) != 0) &&
      (default_trace->logfile->max_filesize > 0))
    config_add_output ("debug server file-max-size %d\r\n", 
		       default_trace->logfile->max_filesize);

  if (default_trace->syslog_flag)
    config_add_output ("debug server syslog\r\n");
}

/* no debug server */
int no_config_debug_server (uii_connection_t *uii) {
  /* need to do something here !!! */
  return (1);
}

/* debug server verbose */
int config_debug_server_verbose (uii_connection_t *uii) {
  set_trace (default_trace, TRACE_FLAGS, TR_ALL, NULL);
  config_add_module (0, "debug", get_config_server_debug, NULL);
  return (1);
}

/* no debug server verbose */
int config_no_debug_server_verbose (uii_connection_t *uii) {
  set_trace (default_trace, TRACE_FLAGS, NORM, NULL);
  config_add_module (0, "debug", get_config_server_debug, NULL);
  return (1);
}

/* debug server file-name %s */
int config_debug_server_file (uii_connection_t *uii, char *filename) {
  set_trace (default_trace, TRACE_LOGFILE, filename, NULL);
  config_add_module (0, "debug", get_config_server_debug, NULL);
  irrd_free(filename);
  return (1);
}

/* debug server syslog */
int config_debug_server_syslog (uii_connection_t *uii) {
  set_trace (default_trace, TRACE_USE_SYSLOG, 1, 0);
  config_add_module (0, "syslog trace",
		     get_config_server_debug, IRR.submit_trace);
  return (1);
}

/* debug server size %d */
int config_debug_server_size (uii_connection_t *uii, int bytes) {
  set_trace (default_trace, TRACE_MAX_FILESIZE, bytes, NULL);
  config_add_module (0, "debug", get_config_server_debug, IRR.submit_trace);
  return (1);
}

void get_config_submission_debug () {
  config_add_output ("debug submission file-name %s\r\n", 
		     IRR.submit_trace->logfile->logfile_name);
  /* also add verbose and syslog here */

}

/* no debug submission */
int no_config_debug_submission (uii_connection_t *uii) {

  /* do something !!! */
  return (1);
}

/* debug submission file-name %s */
int config_debug_submission_file (uii_connection_t *uii, char *filename) {
  set_trace (IRR.submit_trace, TRACE_LOGFILE, filename, NULL);
  config_add_module (0, "debug", get_config_submission_debug, 
		     IRR.submit_trace);
  irrd_free(filename);
  return (1);
}

/* debug submission verbose */
int config_debug_submission_verbose (uii_connection_t *uii) {
  set_trace (IRR.submit_trace, TRACE_FLAGS, TR_ALL, NULL);
  config_add_module (0, "debug", get_config_submission_debug, NULL);
  return (1);
}

/* debug submission file-max-size %d */
int config_debug_submission_maxsize (uii_connection_t *uii, int size) {
  set_trace (IRR.submit_trace, TRACE_MAX_FILESIZE, size, NULL);
  config_add_module (0, "debug", get_config_submission_debug, NULL);
  return (1);
}

/* debug submission syslog */
int config_debug_submission_syslog (uii_connection_t *uii) {
  set_trace (IRR.submit_trace, TRACE_USE_SYSLOG, 1, NULL);
  config_add_module (0, "debug", get_config_submission_debug, NULL);
  return (1);
}

/* db_admin %s  */
void get_config_dbadmin () {
  config_add_output ("db_admin %s\r\n", IRR.db_admin);
}

int config_dbadmin (uii_connection_t *uii, char *email) {
  IRR.db_admin = email;
  config_add_module (0, "dbadmin", get_config_dbadmin, NULL);
  return (1);
}

/* reply_from %s */
void get_config_replyfrom () {
  config_add_output ("reply_from %s\r\n", IRR.reply_from);
}

int config_replyfrom (uii_connection_t *uii, char *email) {
  IRR.reply_from = email;
  config_add_module (0, "replyfrom", get_config_replyfrom, NULL);
  return (1);
}

void get_config_responsefooter () {
  char *st;

  LL_Iterate (IRR.ll_response_footer, st) {
    config_add_output ("response_footer %s\r\n", st);
  }
}

/* response_footer %s */
int config_response_footer (uii_connection_t *uii, char *st) {

  if (IRR.ll_response_footer == NULL)
    IRR.ll_response_footer = LL_Create (0);
  if (st == NULL) 
    st = " ";
  LL_Add (IRR.ll_response_footer, strdup (st));
  config_add_module (0, "responsefooter", get_config_responsefooter, NULL);
  return (1);
}

void get_config_responsereplace () {
  char *st;

  LL_Iterate (IRR.ll_response_replace, st) {
    config_add_output ("response_replace %s\r\n", st);
  }
}

/* response_replace %s */
int config_response_replace (uii_connection_t *uii, char *st) {

  if (IRR.ll_response_replace == NULL)
    IRR.ll_response_replace = LL_Create (0);
  if (st == NULL) 
    st = " ";
  LL_Add (IRR.ll_response_replace, strdup (st));
  config_add_module (0, "responsereplace", get_config_responsereplace, NULL);
  return (1);
}

void get_config_responsenotifyheader () {
  char *st;

  LL_Iterate (IRR.ll_response_notify_header, st) {
    config_add_output ("response_notify_header %s\r\n", st);
  }
}

/* response_notify_header %s */
int config_response_notify_header (uii_connection_t *uii, char *st) {

  if (IRR.ll_response_notify_header == NULL)
    IRR.ll_response_notify_header = LL_Create (0);
  if (st == NULL) 
    st = " ";
  LL_Add (IRR.ll_response_notify_header, strdup (st));
  config_add_module (0, "responsenotifyheader", get_config_responsenotifyheader, NULL);
  return (1);
}

void get_config_responseforwardheader () {
  char *st;

  LL_Iterate (IRR.ll_response_forward_header, st) {
    config_add_output ("response_forward_header %s\r\n", st);
  }
}

/* response_foward_header %s */
int config_response_forward_header (uii_connection_t *uii, char *st) {

  if (IRR.ll_response_forward_header == NULL)
    IRR.ll_response_forward_header = LL_Create (0);
  if (st == NULL) 
    st = " ";
  LL_Add (IRR.ll_response_forward_header, strdup (st));
  config_add_module (0, "responseforwardheader", get_config_responseforwardheader, NULL);
  return (1);
}

/* pgp_dir %s */  
void get_config_pgpdir () { 
  config_add_output ("pgp_dir %s\r\n", IRR.pgp_dir);
}

int config_pgpdir (uii_connection_t *uii, char *pgpdir) {
  IRR.pgp_dir = pgpdir;
  config_add_module (0, "PGP directory configuration", 
		     get_config_pgpdir, NULL);
  return (1);
}

/* override_cryptpw %s */
void get_config_override () { 
  config_add_output ("override_cryptpw %s\r\n", IRR.override_password);
}

int config_override (uii_connection_t *uii, char *override) {
  IRR.override_password = override;
  config_add_module (0, "override cryptpw", 
		     get_config_override, NULL);
  return (1);
}

/* irr_server %s */
void get_config_irr_host () { 
  config_add_output ("irr_server %s \r\n", IRR.irr_host);
}

int config_irr_host (uii_connection_t *uii, char *host) {
  IRR.irr_host = host;
  config_add_module (0, "irr_server configuration", 
		     get_config_irr_host, NULL);
  return (1);
}

void get_irr_ftp_dir () { 
  config_add_output ("ftp directory %s \r\n", IRR.ftp_dir);
}

/* ftp directory %s */
int config_export_directory (uii_connection_t *uii, char *dir) {

  IRR.ftp_dir = dir;
  config_add_module (0, "ftp directory",
		     get_irr_ftp_dir, NULL);
  return (1);
}

void get_tmp_directory () { 
  config_add_output ("tmp directory %s\r\n", IRR.tmp_dir);
}

/* tmp directory %s */
int config_tmp_directory (uii_connection_t *uii, char *dir) {

  IRR.tmp_dir = dir;
  config_add_module (0, "tmp directory",
		     get_tmp_directory, NULL);
  return (1);
}

/* irr_database %s no-clean */
int config_irr_database_no_clean (uii_connection_t *uii, char *name) {
  irr_database_t *database;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  database->no_dbclean = 1;

  if (database->clean_timer) {
    Timer_Turn_OFF (database->clean_timer);
  }

  irrd_free(name);
  return (1);
}

/* irr_database %s clean %d */
int config_irr_database_clean (uii_connection_t *uii, char *name, int seconds) {
  irr_database_t *database;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    return (-1);
  }

  database->no_dbclean = 0;

  if (database->clean_timer == NULL) {
    database->clean_timer = (mtimer_t *) 
      New_Timer (irr_clean_timer, seconds, "IRR dbclean", database);
    timer_set_jitter (database->clean_timer, (int) (seconds/ 3.0));
    Timer_Turn_ON ((mtimer_t *) database->clean_timer);
  }
  else {
    Timer_Turn_OFF ((mtimer_t *) database->clean_timer);
    Timer_Set_Time (database->clean_timer, seconds);
    timer_set_jitter (database->clean_timer, (int) (seconds/ 3.0));
    Timer_Turn_ON ((mtimer_t *) database->clean_timer);
  }

  irrd_free(name);
  return (1);
}

/* Look for input of the form ~(...) and remove the '~(' and ')'.
 *
 * Return:
 *  1 if the above form is found in the input string
 *  0 otherwise
 *
 * If ~(...) is found in the input then remove it from the input
 * string and reset the input string.
 */
int remove_tilda (char **t) {
  char *q, *r, *s = *t;
  
  /* Find the first printable character */
  for (; isspace ((int) *s); s++);
  
  /* Look for a string like this: ~(...). 
   * Remove the opening '~(' and closing ')'
   */
  if (*s == '~'                        && 
      (q = strchr (s, '('))    != NULL &&
      (r = strrchr (++q, ')')) != NULL &&
      r > q) {
    *r = '\0';
    s = strdup (q);
    free (*t);
    *t = s;
    return 1;
  }
  
  return 0;
}

/* Search all known object types and check for a match
 * with the input string, "name".
 *
 * Return:
 *   The corresponding 'enum OBJECT_FILTERS' value that
 *   matches the input string (see scan.h for 'enum OBJECT_FILTERS' types).
 *  XXX_F otherwise (ie, if no match is found).
 */
enum OBJECT_FILTERS find_object_type (char *name) {
  int i;

  for (i = 0; i < MAX_OBJECT_FILTERS; i++)
    if (!strcasecmp (o_filter[i].name, name))
      return o_filter[i].filter_f;

  return XXX_F;
}

/* Set the bit field data structure, "database->obj_filter", which
 * controls which objects are used/accepted/kept in the database.
 * This is useful when we are dealing with VERY large databases like RIPE
 * that have lots of objects that we do NOT want to index or return queries
 * for.  "database->obj_filter" affects all loading operations, ie, mirroring,
 * updates/submissions, bootstrap loads and reloads.
 *
 * The filter is defined by the user in irrd.conf.  Filters define objects to keep in 
 * the DB.  The filter should be constructed as so:
 *
 * irr_database radb filter route|autnum|mntner
 * (allow only routes, autnum's and maintainer objects)
 *
 * irr_database radb filter ~(route|autnum)
 * (allow all objects except routes and autnums)
 *
 * Also recognized are the keywords "routing-registry-objects" and "non-critical"
 * which filter on the routing registry objects only (see scan.h).
 *
 * irr_database radb filter non-critical
 * (use all routing registry objects)
 *
 * Return:
 *   1 if a valid filter is found
 *  -1 if a non-valid filter is found
 * 
 *  "database->obj_filter" is set to the new filter is a valid filter is found.
 *  Otherwise "database->obj_filter" is left unchanged.
 */
int config_irr_database_filter (uii_connection_t *uii, char *name, char *object) {
  irr_database_t *database;
  char *last = NULL;
  char *filter, *s;
  enum OBJECT_FILTERS F;
  u_long filter_bak;
  int ret_val = 1, tilda;

  if ((database = find_database (name)) == NULL) {
    config_notice (ERROR, uii, "Database %s not found!\r\n", name);
    irrd_free(name);
    if (object != NULL)
      irrd_free(object);
    return (-1);
  }

  if (object == NULL) {
    trace (TR_ERROR, default_trace,
	   "config_irr_database_filter (): NULL object input filter: ABORT! \n");
    irrd_free(name);
    return -1;
  }

  s                    = strdup (object);
  filter_bak           = database->obj_filter;
  database->obj_filter = ~0;

  tilda = remove_tilda (&s);

  filter = strtok_r (s, "|", &last);
  while (filter != NULL) {
    /* put a '0' in proper bit position ==> use/keep/accept this object type */
    if ((F = find_object_type (filter)) != XXX_F)
      database->obj_filter &= ~F;
    else if (!strcasecmp (filter, "routing-registry-objects") ||
	     !strcasecmp (filter, "non-critical"))
      database->obj_filter &= ~ROUTING_REGISTRY_OBJECTS;
    else {
      database->obj_filter = filter_bak;
      trace (TR_ERROR, default_trace, "Unknown filter %s\n", filter);
      ret_val = -1;
      break;
    }

    filter = strtok_r (NULL, "|", &last);
  }

  if (ret_val != -1) {
    /* Check to make sure at least one filter was applied */
    if (!(~database->obj_filter)) {
      database->obj_filter = filter_bak;
      trace (TR_ERROR, default_trace, "DB object filter unchanged!\n");
      trace (TR_ERROR, default_trace, "Did not recognize any object types: (%s)\n", object);
      ret_val = -1;
    }
    else {
      /* If a ~(...) was given we need to reverse the filter logic */
      if (tilda)
	database->obj_filter = ~database->obj_filter;
      trace (NORM, default_trace, "Config filter %s (%s)\n", database->name, object);
    }
  }

  irrd_free(name);
  irrd_free(object);
  free (s);
  return ret_val;
}
