#!/usr/bin/perl

my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$datestr);
my $savecrypts = 1;
my $newmnt = 0;
my $mntner;

if (open(GZIP, "|/usr/bin/gzip -q -c") < 0) {
  exit (-1);
}

if ($savecrypts == 1) {
  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday) = localtime();
  $datestr = sprintf("%04d%02d%02d", $year + 1900, $mon + 1, $mday);
  $cryptdbname = "/var/log/irr-pw/cryptpw.$datestr";
  if (-e $cryptdbname) {    # if file already exists, don't generate again
    $savecrypts = 0;
  } else {
    open(CRYPTDB, "> $cryptdbname");
  }
}

while (<>) {
  if ($savecrypts == 1) {
    if (/^mntner:/) {
      $mntner = $_;
      $newmnt = 1;
      $deletedmnt = 0;
    }
    if (/^\*xxner:/) {
      $deletedmnt = 1;
    }
  }
  if (/^(auth:\s+CRYPT-PW\s+)(.{13})(.*)$/) {
    print GZIP "${1}HIDDENCRYPTPW${3}\n";
    if ($savecrypts == 1 && $deletedmnt == 0) {
      if ($newmnt == 1) {
        print CRYPTDB $mntner;
        $newmnt = 0;
      }
      print CRYPTDB $_;
    }
  }
  else {
    print GZIP $_;
  }
}

if ($savecrypts == 1) {
  close(CRYTPDB);
}
close(GZIP);
exit(0);
