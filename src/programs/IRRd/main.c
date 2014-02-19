 /*
 * $Id: main.c,v 1.37 2002/10/17 20:02:31 ljb Exp $
 * originally Id: main.c,v 1.57 1998/08/03 17:29:08 gerald Exp 
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifndef SETPGRP_VOID
#include <sys/termios.h>
#endif /* SETPGRP_VOID */
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>  /* need for OSF? */
#endif /* HAVE_STROPTS_H */
#include <ctype.h> 
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#include "mrt.h"
#include "trace.h"
#include "config_file.h"
#include "irrd.h"

trace_t *default_trace;
irr_t IRR = {0};

/* JW this is a hack and should be removed later
 * when atomic transactions are the default 
 */
int atomic_trans;

/* local functions */
void init_irrd_commands ();
void init_rps_regexes();
static void daemonize ();

int main (int argc, char *argv[])
{
    char *name = argv[0];
    int c, x_flag = 1, v_flag = 0;
    int default_cache = 0;
    char *s;
    extern char *optarg;	/* getopt stuff */
    int errors = 0;
    int daemon = 1;
    irr_database_t tmp1;	/* just used to build linked list */
    irr_connection_t tmp2;	/* just used to build linked list */
    rollback_t rb_ll;           /* atomic transaction support */
    struct passwd *pw;
    struct group *gr;
    int group_id = 0, user_id = 0;
    char *user_name = NULL;
    char *group_name = NULL;
    char *password = NULL;
    char *usage = 
"Usage: %s\n"
"   [-a turn on atomic transaction mode]\n"
"   [-d <irr_directory>]\n"
"   [-f <irrd.conf file>]\n"
"   [-g <groupname>]\n"
"   [-l <username>]\n"
"   [-n do not daemonize]\n"
"   [-s <password>]\n"
"   [-u don't allow privileged commands]\n"
"   [-v verbose mode]\n"
"   [-w <irr_port>]\n"
"   [-x cancel bootstrap missing DB auto-fetch]\n";

    char *config_file = "/etc/irrd.conf";
    int UNPRIV_FLAG = 0;

#if defined(HAVE_LIBPTHREAD) && (GLIB_MINOR_VERSION < 32)
    g_thread_init(NULL);  /* This is required to make GLIB thread-safe */
#endif

    default_trace = New_Trace2 ("irrd");
    IRR.submit_trace = New_Trace2 ("irrd");
    set_trace (default_trace, TRACE_FLAGS, NORM, 0);
    atomic_trans = 0; /* default to off */

    /*
     * set some defaults 
     */
    IRR.expansion_timeout = 0;	/* timeout of zero means no timeout */
    IRR.max_connections = MAX_TOTAL_CONNECTIONS; /* default max connections */
    IRR.mirror_interval = 60*10; /* mirror every ten minutes */
    IRR.irr_port = IRR_DEFAULT_PORT;
    IRR.tmp_dir = IRR_TMP_DIR;
    IRR.path = NULL;
    IRR.statusfile = NULL;
    IRR.roa_database = NULL;

    while ((c = getopt (argc, argv, "w:axmrHhnkvf:ud:g:l:s:")) != -1)
	switch (c) {
	case 'a':
	  atomic_trans = 1;
	  break;
	case 'd':	/* database directory */
	  IRR.database_dir = optarg;
	  break;
	case 'f':		/* config file */
	    config_file = optarg;
	    break;
	case 'g':	/* setgid to this group -- dropping priv's is good */
	  group_name = optarg;
	  if (group_name == NULL) {
	    fprintf(stderr, "null group defined\n");
	    exit(-1);
	  }
	  gr = getgrnam(group_name);
	  if (gr == NULL) {
	      fprintf(stderr, "unknown group: \"%s\"\n", group_name);
	      exit(-1);
	  }
	  group_id = gr->gr_gid;
	  break;
	case 'l':	/* setuid to this user -- dropping priv's is good */
	  user_name = optarg;
	  if (user_name == NULL) {
	    fprintf(stderr, "null user defined\n");
	    exit(-1);
	  }
	  pw = getpwnam(user_name);
	  if (pw == NULL) {
	      fprintf(stderr, "unknown user: \"%s\"\n", user_name);
              exit(-1);
 	  }
	  user_id = pw->pw_uid;
	  break;
	case 'n':		/* do not daemonize */
	  daemon = 0;
	  break;
	case 's':
	  password = strdup ((char *) optarg);
	  break;
	case 'u':
	  /* don't allow privlidged commands -- we're a public server */
	  UNPRIV_FLAG = 1; 
	  break;
	case 'v':		/* verbose */
	    set_trace (default_trace, TRACE_FLAGS, TR_ALL,
		       TRACE_LOGFILE, "stdout",
		       NULL);
	    daemon = 0;
	    v_flag = 1;
	    break;
	case 'w':
	  IRR.irr_port = (u_short) atol (optarg);
	  break;
	case 'x':
	  x_flag = 0;
	  break;
	case 'm':	/* old command line options -- no longer do anything */
	case 'r':
	  break;
	case 'H':
	case 'h':
	default:
	  errors++;
	  break;
	}

    if (errors) {
	fprintf (stderr, usage, name);
	printf ("\nIRRd %s compiled on %s\n",
		IRRD_VERSION, __DATE__);
	exit (1);
    }

    init_trace (name, daemon);
    set_trace(default_trace, TRACE_PREPEND_STRING, "IRRD", 0);

    init_mrt (default_trace);
    init_mrt_reboot (argc, argv);
    init_uii (default_trace);

    set_uii (UII, UII_PROMPT, UII_UNPREV, "Password: ", 0);
    set_uii (UII, UII_PROMPT, UII_NORMAL, "IRRd> ", 0);

    init_irrd_commands (UNPRIV_FLAG);

    /* default trace flag should be set by here */
    IRR.ll_database = LL_Create (LL_Intrusive, True, 
				 LL_NextOffset, LL_Offset (&tmp1, &tmp1.next),
				 LL_PrevOffset, LL_Offset (&tmp1, &tmp1.prev),
				 0);
    IRR.ll_database_alphabetized = LL_Create (0);
    IRR.ll_connections = LL_Create (LL_Intrusive, True, 
				    LL_NextOffset, LL_Offset (&tmp2, &tmp2.next),
				    LL_PrevOffset, LL_Offset (&tmp2, &tmp2.prev),
				    0);

    IRR.connections_hash = g_hash_table_new(g_str_hash, g_str_equal);

    /* add mutex lock for routines wishing to lock all database */
    pthread_mutex_init (&IRR.lock_all_mutex_lock, NULL);

    /* initialize the mutex */
    pthread_mutex_init (&IRR.connections_mutex_lock, NULL);

    /*
     * read configuration here
     */
    if (config_from_file (default_trace, config_file) < 0) {
      config_create_default ();
    }

    /* default the cache if the user does not specify */
    /* should also set default statusfile here? */
    if (IRR.database_dir == NULL) {
      IRR.database_dir = strdup (DEF_DBCACHE);
      default_cache = 1;
    }

    if (IRR.statusfile == NULL) {
      char statusfilename[512];

      strcpy(statusfilename, IRR.database_dir);
      strcat(statusfilename, "/IRRD_STATUS");
      IRR.statusfile = InitStatusFile(statusfilename);
    }
    
    /* sort the databases alphabetically */
    irr_sort_database ();
  
    if (password != NULL) 
      UII->password = password;

    /* first time access -- no password  
    if (UII->password == NULL) {
      UII->initial_state = UII_NORMAL;
      set_uii (UII, UII_PROMPT, UII_NORMAL, "IRRd--Setup> ", 0);
    }
    else 
      set_uii (UII, UII_PROMPT, UII_NORMAL, "IRRd> ", 0);

    set_uii (UII, UII_PROMPT, UII_CONFIG, "Config> ", 0);
	*/
	set_uii (UII, UII_PROMPT, UII_UNPREV, "Password: ", 0);
    set_uii (UII, UII_PROMPT, UII_NORMAL, "IRRd> ", 0);
    set_uii (UII, UII_PROMPT, UII_ENABLE, "IRRd# ", 0);
    set_uii (UII, UII_PROMPT, UII_CONFIG, "Config> ", 0);
    set_uii (UII, UII_PROMPT, UII_CONFIG_INTERFACE, "Interface> ", 0);
    set_uii (UII, UII_PROMPT, UII_CONFIG_LINE, "Line> ", 0);

    if (daemon) {
	/*
	 * Now going into daemon mode 
	 */
	MRT->daemon_mode = 1;
	daemonize ();
    }

    /* listen for UII commands */
    if (listen_uii2 ("irrd") < 0) {
      fprintf (stderr, "**** Error could not bind to UII port. Is another irrd running?\n");
      exit (-1);
    }

    /* did the user fail to specify a DB cache? */
    if (v_flag &&
	default_cache) {
	printf ("WARNING: No DB cache specified\n");
	printf ("WARNING: This means you have no data and all queries will return NULL\n");
	interactive_io ("Do you want to continue?");
    }

    /* DB cache checks.
     *
     * if the DB cache does not exist or there are system problems write
     * them to log.  Also try to create the cache should it
     * not exist */
    if ((s = dir_chks (IRR.database_dir, 1)) != NULL) {
      trace (ERROR, default_trace, "WARNING: %s\n", s);
      if (v_flag) {
	printf ("WARNING: %s\n", s);
	free (s);
	interactive_io ("Do you want to continue?");
      }
    }

    /* check for any transactions that did not complete */
    if (atomic_trans) {
      rb_ll.first = rb_ll.last = NULL;
      /* control process of rolling back any DB's which did not complete */
      if (!rollback_check (&rb_ll)) {
	trace (ERROR, default_trace, "Error occured during bootstrap transaction "
	       "check.  Need DB admin to assist.  exit (0)\n");
	fprintf (stderr, "Error occured during bootstrap transaction "
		 "check.  Need DB admin to assist.  exit (0)\n");
	exit (0);
      }
    }

    /* load the DB's and build the indexes */
    c = irr_load_data (x_flag, v_flag);

    /* scan the ROA database if configured */
    if (IRR.roa_database) {
      struct stat stat_buf;
      struct tm tbuf;
      int n;

      scan_irr_file (IRR.roa_database, NULL, 0, NULL);
      n = fstat(fileno(IRR.roa_database->db_fp), &stat_buf);
      IRR.roa_database_mtime = stat_buf.st_mtime;
      if (gmtime_r(&stat_buf.st_mtime, &tbuf))
        n = strftime(IRR.roa_timebuffer, sizeof(IRR.roa_timebuffer), "t=%Y-%m-%dT%H:%M:%SZ\n", &tbuf);
    }

    /* re-apply any transactions that didn't complete */
    if (atomic_trans        && 
	rb_ll.first != NULL &&
	!reapply_transaction (&rb_ll)) {
      trace (ERROR, default_trace, "Error occured during bootstrap transaction "
	     "reapplication.  Need DB admin to assist.  exit (0)\n");
      fprintf (stderr, "Error occured during bootstrap transaction "
	       "reapplication.  Need DB admin to assist.  exit (0)\n");
      exit (0);
    }
    
    /* Checks for the newbie
     *
     *  -warn if no DB's were config'd
     *  -warn if there are empty DB's
     */
    if (v_flag) {
      /* no configured DB's found in irrd.conf file */
      switch (c) {
      case -1: /* No DB's config'd */
	printf ("WARNING: No DB's configured\n");
	interactive_io ("Do you want to continue?");
	break;
      case 0:  /* No problems */
	break;
      default: /* 1..N empty DB's */
	printf ( 
	       "WARNING: (%d) DB's were found empty on bootstrap and "
	       "could not be fetched\n", c);
	printf ("WARNING: These DB's will return NULL results on queries.\n");
	interactive_io ("Do you want to continue?");
	break;
      }
    }

    /* listen for whois queries */
    if ((IRR.sockfd = listen_telnet (IRR.irr_port)) < 0) {
      fprintf (stderr, 
	       "**** Error could not bind to port %d. Is another irrd running?\n",
	       IRR.irr_port);
      exit (-1);
    }

    /* drop privileges */
    if (group_name != NULL)
            if (setgid(group_id) < 0) {
               fprintf(stderr, "setuid(%d): %s", (int)group_id,
                 strerror(errno));
               exit (-1);
            }
 
 
    if (user_name != NULL) {
            if (getuid() == 0 && initgroups(user_name, group_id) < 0) {
                fprintf(stderr, "initgroups(%s, %d): %s",
                  user_name, (int)group_id, strerror(errno));
                exit (-1);
            }
 
            endgrent();
            endpwent();
            if (setuid(user_id) < 0) {
               fprintf(stderr, "setuid(%d): %s", user_id, strerror(errno));
               exit (-1);
            }
    }

    mrt_main_loop ();
    /* NOT REACHED */
    return(-1);
}

static void daemonize ()
{
    int pid, t;
    int time_left;

    t = 0; /* This gets rid of a warning when t is #ifdef'd out of existance */

    /* alarm's time may not inherited by fork */
    time_left  = alarm (0);

    if ((pid = fork ()) == -1) {
	perror ("fork");
	exit (1);
    }
    else if (pid != 0) {	/* parent */
	exit (0);
    }

#ifdef HAVE_SETSID
    (void) setsid();
#else
#ifdef SETPGRP_VOID
    if ((t = setpgrp ()) < 0) {
	perror ("setpgrp");
	exit (1);
    }

    signal (SIGHUP, SIG_IGN);

    /* fork again so that not being a process group leader */
    if ((pid = fork ()) == -1) {
	perror ("fork");
	exit (1);
    }
    else if (pid != 0) {	/* parent */
	exit (0);
    }

#else /* !SETPGRP_VOID */
    if ((t = setpgrp (0, getpid ())) < 0) {
	perror ("setpgrp");
	exit (1);
    }

    /* Remove our association with a controling tty */
    if ((t = open ("/dev/tty", O_RDWR, 0)) >= 0) {
      if (ioctl (t, TIOCNOTTY, NULL) < 0) {
	    perror ("TIOCNOTTY");
	    exit (1);
	}
	(void) close (t);
    }
#endif /* SETPGRP_VOID */

#ifdef notdef
    /* Close all open files --- XXX need to check for logfiles */
    for (t = 0; t < 255; t++)
	(void) close (t);
#endif
#endif /* HAVE_SETSID */

    umask (0022);
    if (time_left)
      alarm (time_left);
}

void init_irrd_commands (int UNPRIV_FLAG) { 
  /* if unpriv flag NOT set, allow privlidged commands */
  if (!UNPRIV_FLAG) { 
    UII->initial_state = 0;
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "config", 
		      (int (*)()) start_config, 
		      "Configure IRRd options");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM,
		      "show config", show_config, 
		      "Display current configuration");
    uii_add_command2 (UII_CONFIG, COMMAND_NORM,
		      "show config", show_config, 
		      "Display current configuration");
    
    uii_add_command2 (UII_NORMAL, COMMAND_NORM,
		      "show timers", uii_show_timers, "Display timer information");
    
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "write", (int (*)()) config_write, 
		      "Save configuration to disk");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "reload %s", 
		      (int (*)()) uii_irr_reload, 
		      "Reload an IRR database file");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "irrdcacher %s", 
		      (int (*)()) uii_irr_irrdcacher, 
		      "Fetch a remote database and reload using irrdcacher");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "mirror %s", 
		      (int (*)()) uii_irr_mirror_last,
		      "Synchronize database with remote server");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "mirror %s %d", 
		      (int (*)()) uii_irr_mirror_serial,
		      "Synchronize database with remote server, supply LAST");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "dbclean %s", 
		      (int (*)()) uii_irr_clean,
		      "Clean database -- remove deleted entries on disk");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "set serial %s %d", 
		      (int (*)()) uii_set_serial,
		      "Set the serial number of a database");

    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "reboot", (int (*)()) mrt_reboot,
		      "Reboot IRRd");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "export %s",
		      (int (*)()) uii_export_database, "Create copy of database for ftp");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "kill", 
		      (int (*)()) kill_irrd,
		      "Kill off the IRRd daemon");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "read %s database %s", 
		      (int (*)()) uii_read_update_file,
		      "Read an add/deleted update file (for debugging)");
    uii_add_command2 (UII_NORMAL, COMMAND_NORM, "delete route %sdatabase %m %dAS", 
		      (int (*)()) uii_delete_route,
		      "Delete a route");
  }
    
  uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show connections", 
		    (int (*)()) show_connections,
		    "Show current IRR connections");

  uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show database", 
		    (int (*)()) show_database,
		    "Show database status");
  uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show ip %p [%s]", 
		    (int (*)()) uii_show_ip,
		    "Show exact, less or more specific routes");

  uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show mirror-status %s",
                    (int (*)()) uii_mirrorstatus_db,
		    "Get Database Mirror Status");

  uii_add_command2 (UII_NORMAL, COMMAND_NORM, "show statusfile",
                    (int (*)()) uii_show_statusfile,
		    "Show statusfile location.");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_directory %s", 
		    (int (*)()) config_irr_directory,
		    "Where the IRR database files live");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "ftp directory %s", 
		    (int (*)()) config_export_directory,
		    "Where we put the database for FTP");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "irr_database %s export %d [%s]", 
		    (int (*)()) config_irr_database_export,
		    "Configure the database for ftp export");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "irr_database %s remote_ftp_url %s", 
		    (int (*)()) config_irr_remote_ftp_url,
		    "Configure remote the ftp URL for irrdcacher fetches");

  /* JW: probably will take out at a later time.  For now consoladating
     into a single command.

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "irr_database %s repos_hexid %s", 
		    (int (*)()) config_irr_repos_hexid,
		    "Repository hex ID of the PGP key associated with this DB");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "irr_database %s pgppass %s", 
		    (int (*)()) config_irr_pgppass,
		    "PGP password for this DB used to sign floods");
  */

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "rpsdist_database %s",
		    (int (*)()) config_rpsdist_database,
		    "rpsdist database files to use");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "rpsdist_database %s trusted %s %d",
		    (int (*)()) config_rpsdist_database_trusted,
		    "Trusted rpsdist database configuration");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "rpsdist_database %s authoritative %s"
		    , (int (*)()) config_rpsdist_database_authoritative,
		    "Authoritative rpsdist database configuration");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "rpsdist accept %s",
		    (int (*)()) config_rpsdist_accept,
		    "Accept rpsdist connections from host");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "rpsdist %s accept %s",
		    (int (*)()) config_rpsdist_accept_database,
		    "Accept rpsdist connections for database from host");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, 
		    "irr_database %s no-default",
		    (int (*)()) config_irr_database_nodefault,
		    "Do not include this database by default when responding to queries");

/*  uii_add_command2 (UII_CONFIG, COMMAND_NORM,
		    "irr_database %s routing-table-dump",
		    (int (*)()) config_irr_database_routing_table_dump,
		    "This DB file contains a routing table dump");
*/

  uii_add_command2 (UII_CONFIG, COMMAND_NORM,
		    "irr_database %s roa-data",
		    (int (*)()) config_irr_database_roa_data,
		    "This DB file contains ROA data");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "roa-disclaimer %S",
		    (int (*)()) config_roa_disclaimer,
		    "Disclaimer for ROA data contained in whois repsonses");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_path %s", 
		    (int (*)()) config_irr_path,
		    "Add a component(s) to your PATH environment variable");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s", 
		    (int (*)()) config_irr_database,
		    "The IRR database files to use");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "no irr_database %s", 
		    (int (*)()) no_config_irr_database,
		    "Remove an IRR Database");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s mirror_host %s %d", 
		    (int (*)()) config_irr_database_mirror,
		    "The IRR database mirroring");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s mirror_protocol %d",
                    (int (*)()) config_irr_database_mirror_protocol,
                    "IRR mirroring protocol");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s authoritative", 
		    (int (*)()) config_irr_database_authoritative,
		    "Allow write updates for IRR database");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "no irr_database %s authoritative", 
		    (int (*)()) no_config_irr_database_authoritative,
		    "Disable authoritative for an IRR database");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s write-access %d", 
		    (int (*)()) config_irr_database_access_write,
		    "Access list for updates for IRR database");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s cryptpw-access %d",
                    (int (*)()) config_irr_database_access_cryptpw,
                    "Access list for CRYPTPW info for IRR database");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s access %d", 
		    (int (*)()) config_irr_database_access,
		    "Access list for IRR database");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s mirror-access %d", 
		    (int (*)()) config_irr_database_mirror_access,
		    "Access list for IRR mirroring");
  
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s compress_script %s", 
		    (int (*)()) config_irr_database_compress_script, 
		    "Configure the compress script for db exports");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s filter %s", 
		    (int (*)()) config_irr_database_filter,
		    "Filter object from database");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s no-clean", 
		    (int (*)()) config_irr_database_no_clean,
		    "Turn off database cleaning");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_database %s clean %d", 
		    (int (*)()) config_irr_database_clean,
		    "Set database cleaning time");
     
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_port %d", 
		    (int (*)()) config_irr_port_2,
		    "The \"IRRd\" whois interface query port");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_port %d access %d", 
		    (int (*)()) config_irr_port,
		    "The \"IRRd\" whois interface query port");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_mirror_interval %d", 
		    (int (*)()) config_irr_mirror_interval,
		    "How often (seconds) between mirror updates");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_expansion_timeout %d", 
		    (int (*)()) config_irr_expansion_timeout,
		    "The maximum number of seconds a set expansion query is allowed to consume");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_max_connections %d", 
		    (int (*)()) config_irr_max_con,
		    "The maximum number of simultaneous connections");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "no debug server", 
		    no_config_debug_server, "Turn off server logging");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug server file-name %s", 
		    config_debug_server_file, "Configure logging information");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug server file-max-size %d", 
		    config_debug_server_size, "Limit the size of the logfile");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug server syslog", 
		    config_debug_server_syslog, 
		    "Send log information to syslog");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug server verbose", 
		    config_debug_server_verbose, 
		    "Turn on verbose logging");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "no debug server verbose", 
		    config_no_debug_server_verbose, 
		    "Turn off verbose logging");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "statusfile %s",
                    config_statusfile, "set statusfile name");
  
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "no debug submission", 
		    no_config_debug_submission, "Turn off submision logging");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug submission file-name %s", 
		    config_debug_submission_file, 
		    "Configure submission logging");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug submission verbose", 
		    config_debug_submission_verbose, 
		    "Turn on verbose submission logging");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug submission file-max-size %d", 
		    config_debug_submission_maxsize, 
		    "Maximum size of email/tcp submission log");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "debug submission syslog", 
		    config_debug_submission_syslog, 
		    "Use syslog for submission logging");
  
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "tmp directory %s", 
		    config_tmp_directory, 
		    "Configure a tmp/cache directory");

  /* pass through commands -- use by pipeline, but not us */
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "override_cryptpw %s",
		    (int (*)()) config_override,
		    "The override password");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "pgp_dir %s",
		    (int (*)()) config_pgpdir,
		    "Where to find the PGP keyring");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "irr_server %s",
		    (int (*)()) config_irr_host,
		    "IRRd server where we send submissions");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "db_admin %s",
		    (int (*)()) config_dbadmin,
		    "DB-admin email address");
  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "response_footer %S",
		    (int (*)()) config_response_footer,
		    "Text at bottom of email responses");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "response_notify_header %S",
		    (int (*)()) config_response_notify_header,
		    "Text at top of email responses");

  uii_add_command2 (UII_CONFIG, COMMAND_NORM, "response_forward_header %S",
		    (int (*)()) config_response_forward_header,
		    "Text at top of forwarded email responses");

}

