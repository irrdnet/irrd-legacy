/* 
 * $Id: main.c,v 1.8 2000/09/26 20:12:40 gerald Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


#include <irrauth.h>
#include <irr_check.h>
#include <irr_notify.h>
#include <hdr_comm.h>



/* Global config file information struct
 * (See read_conf.c)
 */
config_info_t ci;
trace_t *default_trace;
FILE *fin;
/*, *dfile = stderr;*/
/* JW take out
FILE *tmp;
*/


int main(int argc, char *argv[]) {
  char *name = argv[0], *config_fname = "/etc/irrd.conf";
  char *file_name = NULL;
  int errors, c /*, JW take out ret_code, ack_lock*/;
  int NULL_NOTIFICATIONS, DAEMON_FLAG, RPS_DIST_FLAG;
  /* JW take out
  FILE *trans_fp = NULL;
  int lock_fp;
  */
  extern int optind;
  src_t sstart;
  source_t *obj;
  char *usage = "Usage: %s [options] [filename]\n  -v verbose logging, turn on debugging\n  -c <crypted password> (default 'foo')\n  -E DB admin address for new maintainer requests\n  -f <IRRd config file location> (default '/etc/irrd.conf')\n  -h <IRRd host> (default 'localhost')\n  -l <log directory> (default 'irrd_directory')\n  -p <IRRd port> (default 43)\n  -r <pgp directory>  (default is ~/.pgp)\n  -s <DB source> source is authoritative\n  The file is chosen by irr_submit\n  -R RPS Dist mode\n  -D Inetd mode, read/write to STDIN/STDOUT\n  -x do not send notifications\n  The '-x' flag will cause updates to be sent to IRRd only.\n  The default is to send all notifications.\n\n  Command line options will override irrd.conf options.\n  -F \" enclosed response footer string";

  /* Initialization */
  default_trace = New_Trace ();
  errors = NULL_NOTIFICATIONS = DAEMON_FLAG = RPS_DIST_FLAG = 0;
  sstart.first = sstart.last = NULL;
  memset (&ci, 0, sizeof (config_info_t));

  /* Loop and process the command line flags */
  while ((c = getopt (argc, argv, "vc:RDxtf:p:h:l:r:s:E:F:")) != -1)
    switch (c) {
    case 'c':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid encrypted password!\n", optarg);
	errors++;
      }
      else 
	 ci.super_passwd = optarg;
      break;
    case 'E':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid email address!\n", optarg);
	errors++;
      }
      else 
	 ci.db_admin = optarg;
      break;
    case 'F':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid email address!\n", optarg);
	errors++;
      }
      else 
	 ci.footer_msg = strdup (optarg);
      break;
    case 'h':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid IRRd host name!\n", optarg);
	errors++;
      }
      else
	 ci.irrd_host = optarg;
      break;
    case 'l':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid directory name!\n", optarg);
	errors++;
      }
      else
	 ci.log_dir = optarg;
      break;
    case 'p':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid port!\n", optarg);
	errors++;
      }
      else
	 ci.irrd_port = atoi (optarg);
      break;
    case 'r':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid directory name!\n", optarg);
	errors++;
      }
      else
	 ci.pgp_dir = optarg;
      break;
    case 's':
      if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid DB name!\n", optarg);
	errors++;
      }
      else {
	obj = create_source_obj (optarg, 1);
	add_src_obj (&sstart, obj);    
      }
      break;
    case 'R':
      RPS_DIST_FLAG = 1;
      break;
    case 'x':
      NULL_NOTIFICATIONS = 1;
      break;
    case 'f':
      if (!strcasecmp (optarg, "stderr") ||
	  !strcasecmp (optarg, "stdout")) {
	fprintf (stderr, "Invalid IRRd configuration file!\n");
	errors++;
      }
      else if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid IRRd configuration file!\n", optarg);
	errors++;
      }
      else 
	config_fname = optarg;
      break;
    case 'd':
    case 'D':
      DAEMON_FLAG = 1;
      break;
    case 'v':	/* turn on debugging */
      set_trace (default_trace, TRACE_FLAGS, TR_ALL,
		 TRACE_LOGFILE, "stdout",
		 NULL);
      break;
    default:
      errors++;
      break;
    }

  /* input file */
  if (!errors)
    for (; optind < argc; optind++) {
      if (file_name == NULL)
	file_name = argv[optind];
      else {
	errors++;
	break;
      }
    }

  if (DAEMON_FLAG && file_name != NULL) {
    fprintf (stderr, 
	     "Two input sources specified: '-d/D' option and %s!\n", file_name);
    errors++;
  }    
  
  if (errors) {
    fprintf (stderr, usage, name);
    fprintf (stderr,"\nirr_submit compiled on %s\n",__DATE__);
    exit (0);
  }

  if (DAEMON_FLAG || file_name == NULL)
    fin = stdin;
  else if ((fin = fopen (file_name, "r")) == NULL) {
    fprintf (stderr, "Error opening input file \"%s\": %s\n", 
	     file_name, strerror(errno));
    exit (0);
  }

  /* Parse the irrd.conf file */
  ci.srcs = sstart.first;
  if (parse_irrd_conf_file (config_fname, default_trace) < 0) 
    fprintf (stderr, "Warning: could not open the IRRd conf file!\n");

  /* JW debug
  {
    source_t *obj;

    printf ("JW: source list...\n");
    for (obj = ci.srcs; obj != NULL; obj = obj->next) {
      printf ("\n---------\nname (%s)\n", obj->source);
      printf ("  authoritative (%d)\n", obj->authoritative);
      printf ("  rpsdist flag (%d)\n", obj->rpsdist_flag);
      printf ("  rpsdist trusted (%d)\n", obj->rpsdist_trusted);
      printf ("  rpsdist auth (%d)\n", obj->rpsdist_auth);
      printf ("  rpsdist host (%s)\n", ((obj->rpsdist_host == NULL) ? "NULL" : obj->rpsdist_host));
     printf ("  rpsdist port (%s)\n", ((obj->rpsdist_port == NULL) ? "NULL" : obj->rpsdist_port)); 
     printf ("  rpsdist hexid (%s)\n", ((obj->rpsdist_hexid == NULL) ? "NULL" : obj->rpsdist_hexid)); 
     printf ("  rpsdist accept host (%s)\n", ((obj->rpsdist_accept_host == NULL) ? "NULL" : obj->rpsdist_accept_host)); 
     printf ("  rpsdist pgppass (%s)\n", ((obj->rpsdist_pgppass == NULL) ? "NULL" : obj->rpsdist_pgppass)); 
    }

    } */

  /* Successful invokation and good irrd.conf file.
   * Now we can process DB submission */
  call_pipeline (default_trace, fin, NULL_NOTIFICATIONS, 
		 DAEMON_FLAG, RPS_DIST_FLAG);

  trace (NORM, default_trace, "\n\n");
  exit(0);
}
