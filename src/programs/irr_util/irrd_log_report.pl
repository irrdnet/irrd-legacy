#!/usr/bin/perl

# Directory whe log file is kept
$DIRECTORY = "/irr/log/";
$FILE = "irrd.log";
$RECIPIENTS = "irrd-support\@merit.edu";
#$RECIPIENTS = "labovit\@merit.edu";
$SENDMAIL = "/usr/lib/sendmail";



$today = &today;

&scan_log_file;
&mail_report;


# move IRRd logfile
system ("/bin/mv -f $DIRECTORY/$FILE $DIRECTORY/$FILE.$today");


exit;


sub scan_log_file {
  open (IN, "$DIRECTORY/$FILE") || die "Could not open $DIRECTORY/$FILE: $!\n";
  
  $start = "";
  while (<IN>) {
    if (/^(\w+\s+\d+\s+[\d\:]+)/) {
      if ($start eq "") {$start = $1;}
      $end = $1;
    }

    if (/\[(\d+)\]\s+IRRD IRR connection from\s+([\d\.]+)/) {
      $thread = $1; $ip = $2;
      $TOTAL_CONNECTIONS++;
      $CONNECTION{"$thread"} = "$ip";
      $HOST_CONNECTIONS{"$ip"}++;
    }
  
    if (/\[(\d+)\]\s+IRRD Sent (\d+) bytes/) {
      $thread = $1; $bytes = $2;
      $ip = $CONNECTION{"$thread"};
      $TOTAL_BYTES += $bytes;
      $HOST_BYTES{"$ip"} += $bytes;
    }
    if (/Command/) {
      $TOTAL_QUERIES++;
    }
    if (/Processing mirror request from ([\d\.]+) for ([\w\d\.\-]+)/) {
      $MIRROR_CLIENT{$1}++;
      if (!($MD{$1} =~ /$2/)) {
	$MD{$1} .= "$2 ";
      }
    }
  }
}


sub mail_report {
  @hosts = keys %HOST_CONNECTIONS;

  open (OUT, "|/usr/lib/sendmail -f daemon $RECIPIENTS");

  printf OUT "Subject: $today Daily IRRd Report\n";
  printf OUT "From: daemon\@radb1.merit.edu\n";
  printf OUT "\n\n";

  printf OUT "IRRd Report for $start to $end\n\n";

  printf OUT "Total TCP Connections: $TOTAL_CONNECTIONS\n";
  printf OUT "Total Queries: $TOTAL_QUERIES\n";
  printf OUT "Total Bytes: %-6.2f (MB)\n", $TOTAL_BYTES / 1000000;
  printf OUT "Unique Hosts: %d\n", $#hosts + 1;
  
  printf OUT "\n";
  printf OUT "Top Ten Hosts by Volume of Queries:\n";

  @hosts = sort byvolume @hosts;
  $i = 1;
  foreach $host (@hosts) {
    $hostname = &lookup ($host);
    printf OUT "  %-3s %-25s  %-9.2f (KB)     %-5d connections\n", "$i.",
    $hostname, $HOST_BYTES{"$host"} / 1000, $HOST_CONNECTIONS{"$host"};
    if ($i++ > 10) {last;}
  }

  @n = keys %MIRROR_CLIENT;
  print OUT "\nMirror Clients (", $#n + 1, ")\n";
  foreach $mclient (sort bycount keys %MIRROR_CLIENT) {
   $hostname = &lookup ($mclient);
   print OUT " $hostname (", $MIRROR_CLIENT{$mclient}, ")";
   chop ($MD{$mclient});
   print OUT "    [", $MD{$mclient}, "]\n";
 }

  close (OUT);
}


sub bycount {
  $v1 =$MIRROR_CLIENT{$a};
  $v2 =$MIRROR_CLIENT{$b};
  return ($v2 <=> $v1);
}

sub byvolume {
  $v1 = $HOST_BYTES{"$a"};
  $v2 = $HOST_BYTES{"$b"};

  return ($v2 <=> $v1);
}

sub today {
    $today = time;
    
    ($a1, $a2, $a3, $mday, $mon, $year, @junk) = localtime ($today);

    $mon += 1;
    if ($mday < 10) {$mday = "0$mday";}
    if ($mon < 10) {$mon = "0$mon";}

    return ("$year$mon$mday");
}


sub lookup {
  ($a) = @_;
  $name = `nslookup $a`;
  if ($name =~ /Name:\s+([\w\.\-\d]+)/)  {
    $n = $1;
  }
  else {
    $n = $a;
  }

  return ($n);
}
  

