#!/usr/bin/perl
#
# Pierre Thibaudeau <prt@Teleglobe.CA>
#
# The full ripe.db.gz is about 36Gbytes. To speed up the recovery process
# when we need it, it would be nice if the RIPE people routinely make
# available on their ftp server an "IRR only" subset of the ripe.db file.
# Can you make that suggestion to them? 
#
# Here is the script that I use to build my own IRR subset of the ripe.db
# when I need to do so. (You certainly have your own.) 
#

while (<>) {

  if ( /^#/ ) {                     # keep comment lines intact

    print ;

    next }

  next if ( /^\s*\n/ ) ;

  if ( /^\*(am|an|rt|mt): / ) {     # object types to keep

    print "\n";

    do { print $_ ; } while ( ($_ = <>) =~ /^\*/ ) ;   # print object to keep

    next }

  while ( ($_ = <>) =~ /^\*/ ) {}   # skip unwanted object

}

