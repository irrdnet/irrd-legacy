#include <stdio.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

main (int argc, char *argv[]) {
  char *encrypted, cleartxt[256], salt[5];
  char *usage = "Usage: %s [-s] cleartext password\n  -s <salt> (default 'pf;tn;')\n";
  char *name = argv[0], c;
  int errors;

  /* default salt */
  strcpy (salt, "pf;tn;");
  cleartxt[0] = '\0';
  errors = 0;

  while ((c = getopt (argc, argv, "s:")) != -1)
    switch (c) {
    case 's':
      strcpy (salt, optarg);
      if (strlen (salt) < 4) {
	fprintf (stderr, 
		 "salt \"%s\" is too short, must be at least 4 characters\n", salt);
	exit (1);
      }
      break;
    default:
      errors++;
      break;
    }


  if (!errors)
    for (; optind < argc; optind++) {
      if (cleartxt[0] == '\0')
	strcpy (cleartxt, argv[optind]);
      else {
	errors++;
	break;
      }
    }

  if (cleartxt[0] == '\0') {
    fprintf (stderr, "No clear text password specified!\n");
    errors++;
  }

  if (errors) {
    fprintf (stderr, usage, name);
    exit (1);
  }

  encrypted = crypt (cleartxt, salt);

  if (encrypted == NULL) {
    printf ("Bad password, try again...\n");
    exit (0);
  }
  else
    printf ("encrypted passwd is \"%s\"\n", encrypted);

  /* check 
  printf ("Match? : (%d)\n", !strcmp (crypt (cleartxt, encrypted), encrypted));
  */
}
