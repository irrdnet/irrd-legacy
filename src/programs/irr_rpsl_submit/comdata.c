#include <irrauth.h>

const char pgpbegin[]   = "^-----BEGIN PGP SIGNED MESSAGE-----";
const char pgpend[]     = "^-----END PGP SIGNATURE-----";
const char pgpbegdet[]  = "^-----BEGIN PGP SIGNATURE-----";

/* The following are specific to pgp5 
const char pgpkeyid[]  = "(bits, Key ID |unknown keyid: 0x)(........)";
const char pgpmailid[] = "  \"[^<]*<([^>]+)>";
*/
/* pgp 2.6 */
/* 
const char pgpmailid[] = "^Good signature from user \"(.*)\"\.$";
*/
/* JW old 
const char pgpkeyid[]  = "(key, key ID |unknown keyid: 0x)(........)";
const char pgpmailid[] = "[^\"]+\"[^<]*<([^>]+)>";
*/
/* PGP 5 */
const char pgpkeyid[]  = "(bits, Key ID )(........)";
const char pgpgood[] = "^Good signature made";
/* JW changed: 11-4-98:
couple of sample cases:
PGP: (   "<jml@ans.net>"
PGP: "lawsonj@vt.edu"
PGP: (   "Gerald A. Winters <gerald@merit.edu>"
const char pgpmailid[] = "  \"[^<]+.<([^>]+)>";
*/

/* Our magic cookies */
/*
const char cookie[]      = "^(COOKIE): *(.*)";
*/
const char cookie[]      = "^COOKIE:";

const char dotstar[]	 = "(.*)";

/* where to place our cookie - in this case, after the source: line */
const char blankline[]  = "^[ \t]*\n?$";
const char cookieins[]	= "^[ \t]*(\\*so|source):";
const char authby[]     = "^[ \t]*(\\*at|auth):[ \t]*(.*)";
const char mntby[]      = "^[ \t]*(\\*mb|mnt-by):[\t]*(.*)";
const char origin[]     = "^[ \t]*(\\*or|origin):[\t]*(.*)";
const char password[]   = "^[ \t]*password:[ \t]*([^ \t\n](.*[^ \t\n])?)?[ \t\n]*$";


const char tmpfntmpl[] = "/var/tmp/irrauth.XXXXXX";
const char mailfrom[]  = "^From:[ \t]*([^<\n]*)(<([^>]+)>)?";
const char mailreplyto[]  = "^Reply-To:[ \t]*([^<\n]*)(<([^>]+)>)?";
const char mailfromnc[]= "^From[ \t]";
const char messid[]    = "^Message-id:[ \t]*([^\n]*)";
const char subj[]      = "^Subject:[ \t]*([^\n]*)";
const char date[]      = "^Date:[ \t]*([^\n]*)";
