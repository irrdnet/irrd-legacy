#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <irr_notify.h>


/* Global var's */

int DAEMON_FLAG = 0;
config_info_t ci;
trace_t *default_trace;

int main (int argc, char *argv[]) {
  int errors, c;
  char *usage = "Usage: %s [-dfrt] [filename]\n  -d run in daemon mode\n  -f specify the IRRd config file (default '/etc/irrd.conf')\n  -t <trace file> turn on tracing information to <trace file>\n  [filename] file for input in non-daemon mode\n\n  This programs sends out DB transaction notifications.\n";
  extern char *optarg;
  extern int optind;
  char *name = argv[0];
  char pid_string[20];
  char *file_name = NULL, *config_fname = NULL;
  FILE *fin, *dfile = NULL;

  default_trace = New_Trace2 ("irr_notify");
  sprintf (pid_string, "PID%d", (int) getpid ());
  set_trace (default_trace, TRACE_PREPEND_STRING, pid_string, 0);

  errors = 0;

  while ((c = getopt (argc, argv, "vdf:t:")) != -1)
    switch (c) {
    case 'd':
      DAEMON_FLAG = 1;
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
      else if ((dfile = fopen (optarg, "r")) == NULL) {
	fprintf (stderr, "Error opening redirect debug file \"%s\"\n", optarg);
	errors++;
      }
      break;
    case 'v':
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
    for ( ; optind < argc; optind++) {
      if (file_name == NULL)
	file_name = argv[optind];
      else {
	errors++;
	break;
      }
    }

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
  
  if (errors) {
    fprintf (stderr, usage, name);
    printf ("\nirr_notify compiled on %s\n",__DATE__);
    exit (1);
  }
  

  if (DAEMON_FLAG)
    fin = stdin;
  else if ((fin = fopen (file_name, "r+")) == NULL) {
    fprintf (stderr, "Error opening input file \"%s\", exit!\n", file_name);
    exit (1);
  }

  parse_irrd_conf_file (config_fname, default_trace);

  /* When the dust finally settles, fix and uncomment out 
  notify (dfile, tempfname, fin, stdout, 0, 0, NULL, 0, ci.irrd_host, ci.irrd_port, NULL);
  */
  fclose (fin);

  return 0;
}
