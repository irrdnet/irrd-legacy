/* 
 * $Id: main.c,v 1.6 2002/10/17 20:22:09 ljb Exp $
 * originally Id: main.c,v 1.6 1998/07/28 23:04:38 gerald Exp 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <irr_rpsl_check.h>

/* Global config file information struct
 * (See read_conf.c)
 */
config_info_t ci;

extern int optind;
extern char *optarg;
extern FILE *yyin;
int line_num;
extern int yydebug;
int DAEMON_FLAG = 0;
extern int CANONICALIZE_OBJS_FLAG;
extern int QUICK_CHECK_FLAG;
extern int INFO_HEADERS_FLAG;
extern int yyparse(void);
FILE *dfile;
int verbose = 0;
config_info_t ci;
void daemonize ();
trace_t *default_trace = NULL;

int main (int argc, char *argv[]) {
  int errors, c;
  char *usage = "Usage: %s [-cdfhoqtv] [filename]\n  -c turn on object canonicalization\n  -d run in daemon mode\n  -f specify the IRRd config file (default '/etc/irrd.conf')\n  -s send notifications to the sender only\n  -h include parse information headers at the begining of each object\n  -o <output file> divert output to file, default to STDOUT\n  -q quick check; no object output, report OK or report first error and stop\n  -t <trace file> turn on tracing information to <trace file>\n  [filename] file for input in non-daemon mode\n\n  This programs syntax checks IRR objects.  The -q quick check option implies\n  no -c or -h options (i.e., no canonicalization or header information).\n  -v turn on debug verbose mode\n";
  char *name = argv[0];
  char *file_name = NULL, *config_fname = NULL;
  char pid_string[100];

  dfile = stdout;
  yydebug = 0; 
  errors = 0;

  default_trace = New_Trace2 ("irr_rpsl_check");
  sprintf (pid_string, "PID%d", (int) getpid ());
  set_trace (default_trace, TRACE_PREPEND_STRING, pid_string, 0);
  

  while ((c = getopt (argc, argv, "vcdf:hqt:o:")) != -1)
    switch (c) {
    case 'd':
      DAEMON_FLAG = 1;
      break;
    case 'c':
      CANONICALIZE_OBJS_FLAG = 1;
      break;
    case 'h': /* the header processing assumes canonicalization
               * is also occuring.  header processing pulls items
               * from the canonicalize buffer.
	       */
      INFO_HEADERS_FLAG = 1;
      CANONICALIZE_OBJS_FLAG = 1;
      break;
    case 'o':
      if (!strcasecmp (optarg, "stderr"))
	ofile = stderr;
      else if (!strcasecmp (optarg, "stdout"))
	ofile = stdout;
      else if (*optarg == '-') {
	fprintf (stderr, "\"%s\" does not look like a valid debug output file!\n", optarg);
	errors++;
      }
      else if (optind == argc && !DAEMON_FLAG) {
	fprintf (stderr, "Missing input file!\n");
	errors++;
      }
      else if ((ofile = fopen (optarg, "w")) == NULL) {
	fprintf (stderr, "Error opening output file \"%s\"\n", optarg);
	errors++;
      }
      break;
    case 'q':
      QUICK_CHECK_FLAG = 1;
      break;
    case 'v':
      set_trace (default_trace, TRACE_FLAGS, TR_ALL,
		 TRACE_LOGFILE, "stdout",
		 NULL);
      verbose = 1;
      break;
    case 't':
      if (!strcasecmp (optarg, "stderr"))
	dfile = stderr;
      else if (!strcasecmp (optarg, "stdout"))
	dfile = stdout;
      else if (*optarg == '-') {
	fprintf (stderr, "\"%s\" does not look like a valid debug output file!\n", optarg);
	errors++;
      }
      else if (optind == argc && !DAEMON_FLAG) {
	fprintf (stderr, "Missing input file!\n");
	errors++;
      }
      else if ((dfile = fopen (optarg, "w")) == NULL) {
	fprintf (stderr, "Error opening redirect debug file \"%s\"\n", optarg);
	errors++;
      }
      break;
    case 'f':
      if (!strcasecmp (optarg, "stderr") ||
	  !strcasecmp (optarg, "stdout"))
	errors++;
      else if (*optarg == '-') {
	fprintf (stderr, 
		 "\"%s\" does not look like a valid IRRd configuration file!\n", optarg);
	errors++;
      }
      else if (optind == argc && !DAEMON_FLAG) {
	fprintf (stderr, "Missing input file!\n");
	errors++;
      }
      else 
	config_fname = optarg;
      break;
    default:
      errors++;
      break;
    }
  
  /* input file */
  if (!errors)
    for ( ; optind < argc; optind++) {
      if (file_name == NULL)
	file_name = argv[optind];
      else {
	errors++;
	break;
      }
    }

  /* output file */
  if (!errors && ofile == NULL)
    ofile = stdout;

  /* trace debug file */
  if (!errors && dfile == NULL) {
    if ((dfile = fopen ("/dev/null", "w")) == NULL) {
      fprintf (stderr, "Could not open /dev/null for debug output, exit!\n");
      exit (1);
    }
  }

  if (!DAEMON_FLAG && (file_name == NULL)) {
    fprintf (stderr, "No input file specified!\n");
    errors++;
  }
  
  
  if (QUICK_CHECK_FLAG && 
      (CANONICALIZE_OBJS_FLAG || INFO_HEADERS_FLAG)) {
    fprintf (stderr, "-q flag implies no -c and -h flags!\n");
    errors++;
  }
  
  if (errors) {
    fprintf (stderr, usage, name);
    printf ("\nirr_rpsl_check compiled on %s\n",__DATE__);
    exit (1);
  }
  

  /* pick off the authoritative sourcess */
  if (parse_irrd_conf_file (config_fname, default_trace) < 0) 
    exit (0);

  if (DAEMON_FLAG) {
    daemonize ();
    return 0;
  }

  if ((yyin = fopen (file_name, "r")) == NULL) {
    fprintf (stderr, "Error opening input file \"%s\", exit!\n", file_name);
    exit (1);
  }

  /* parse_irrd_conf_file (&ci, config_fname);*/
  yyparse ();
  return 0;
}

/* called from inetd */
void daemonize () {
  yyin = stdin;
  yyparse ();

}
