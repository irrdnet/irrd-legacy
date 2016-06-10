#!/usr/bin/perl -s

use POSIX;

#for timestamp
$starttime = strftime("%m/%d/%y %I:%M:%S %p",localtime);

# Directory where log file is kept
$DIRECTORY = "/irr/log";
$FILE = "irrd.log";
$SUBMITFILE = "pipeline.log";
$RECIPIENTS = "irr-support\@somecompany.com"; 
$SENDMAIL = "/usr/lib/sendmail";

#initialize our counters to zero
$TOTAL_CONNECTIONS = 0;
$TOTAL_CONNECTIONS6 = 0;
$TOTAL_QUERIES = 0;
$TOTAL_QUERIES6 = 0;
$deniedcount = 0;
$deniedcount6 = 0;
$rejectedcount = 0;

#regexes for ipv6 addresses that chew up cpu
@ipv6_patterns = (
		  "(?:[a-f0-9]{1,4}:){7}[a-f0-9]{1,4}",
		  "[a-f0-9]{0,4}::",
		  ":(?::[a-f0-9]{1,4}){1,6}",
		  "(?:[a-f0-9]{1,4}:){1,6}:",
		  "(?:[a-f0-9]{1,4}:)(?::[a-f0-9]{1,4}){1,6}",
		  "(?:[a-f0-9]{1,4}:){2}(?::[a-f0-9]{1,4}){1,5}",
		  "(?:[a-f0-9]{1,4}:){3}(?::[a-f0-9]{1,4}){1,4}",
		  "(?:[a-f0-9]{1,4}:){4}(?::[a-f0-9]{1,4}){1,3}",
		  "(?:[a-f0-9]{1,4}:){5}(?::[a-f0-9]{1,4}){1,2}",
		  "(?:[a-f0-9]{1,4}:){6}(?::[a-f0-9]{1,4})",
		  "(?:0:){5}ffff:(?:\d{1,3}\.){3}\d{1,3}",
		  "(?:0:){6}(?:\d{1,3}\.){3}\d{1,3}",
		  "::(?:ffff:)?(?:\d{1,3}\.){3}\d{1,3}",
		 ); 

$today = &today;
# move IRRd logfile
system ("/bin/mv -f $DIRECTORY/$FILE $DIRECTORY/$FILE.$today");
system ("/bin/touch $DIRECTORY/$FILE");
system ("/bin/mv -f $DIRECTORY/$SUBMITFILE $DIRECTORY/$SUBMITFILE.$today");
system ("/bin/touch $DIRECTORY/$SUBMITFILE");
&scan_pipeline;
&scan_log_file;
$endtime = strftime("%m/%d/%y %I:%M:%S %p",localtime);
&mail_report;

# Compress the log files
system ("/usr/bin/gzip -f $DIRECTORY/$FILE.$today &");
system ("/usr/bin/gzip -f $DIRECTORY/$SUBMITFILE.$today &");

# keep for stats
@n = keys %MIRROR_CLIENT;
open (OUT, ">> /irr/log/stats");
printf OUT "$today $TOTAL_CONNECTIONS\t";
printf OUT "$TOTAL_QUERIES\t";
printf OUT "%-6.2f\t", $TOTAL_BYTES / (1024 * 1024);
printf OUT "%d\t", $#hosts + 1;
printf OUT "%d\t", @n+1;	# mirror clients
printf OUT "%d\t%d\n", $NUM_OKAY + 0, $NUM_FAILED + 0;
printf OUT "%d\t%d\n", $R6_NUM_OKAY + 0, $R6_NUM_FAILED + 0;
close (OUT);

exit;

sub scan_pipeline {
  open (IN, "<$DIRECTORY/$SUBMITFILE.$today") || warn "Could not open $DIRECTORY/$SUBMITFILE.$today: $!\n"; 

  while (<IN>) {
    if (/\sOK\:/) {
      if (/\[route6\]/) {
	$R6_NUM_OKAY++;
      } else {
	$NUM_OKAY++;
      }
      next;
    }
    if (/\sFAILED\:/) {
      if (/\[route6\]/) {
        $R6_NUM_FAILED++;
      } else {
	$NUM_FAILED++;
      }
    }
  }
  close (IN);
}

sub scan_log_file {
  open (IN, "<$DIRECTORY/$FILE.$today") || warn "Could not open $DIRECTORY/$FILE.$today: $!\n";
  
  $start = "";
  while (<IN>) {
    if (/^(\w+\s+\d+\s+[\d\:]+)/) {
      if ($start eq "") {
	$start = $1;
      }
      $end = $1;
    }

    if (/IRRD Command\:/) {
      $TOTAL_QUERIES++;
      if (/\[(\d+)\] IRRD Command\: \!nIRRj/) {
        $thread = $1;
        $ip = $CONNECTION{"$thread"};
        $IRRJ{$ip}++;
      }
      if (/IRRD Command\:\s+[0-9A-Fa-f]{1,4}\:/) {
        foreach $ipv6pat (@ipv6_patterns) {
	  if (/IRRD Command:.*($ipv6pat)/) {
	    $TOTAL_QUERIES6++;
	    last;
	  }
        }
      }
      next;
    }

    if (/\[(\d+)\] IRRD Sent (\d+)/) {
      $thread = $1; $bytes = $2;
      $ip = $CONNECTION{"$thread"};
      $TOTAL_BYTES += $bytes;
      $HOST_BYTES{"$ip"} += $bytes;
      next;
    }

    if (/\[(\d+)\] (IRRD|IRRD IRR) connection from/) {
      if (/\[(\d+)\] IRRD(.*)connection from (\d+\.\d+\.\d+\.\d+)/) {
      	$thread = $1; $ip = $3;
      	$TOTAL_CONNECTIONS++;
      	$CONNECTION{"$thread"} = "$ip";
      	$HOST_CONNECTIONS{"$ip"}++;
      } else {
	foreach $ipv6pat (@ipv6_patterns) {
	  if (/\[(\d+)\] IRRD(.*)connection from ($ipv6pat)/) {
	    $thread6 = $1; $ip6 = $3;
	    $TOTAL_CONNECTIONS6++;
	    $CONNECTION6{"$thread6"} = "$ip6";
	    $HOST_CONNECTIONS6{"$ip6"}++;
	    last;
	  }
	}
      }
    }

    #check for mirrors that are failing
    if (/\*ERROR\* (\w+) \((\w+)\)\s\w+ to mirror/) {
      $dbname = $2;
      $TOTAL_FAILURES++;
      $FAILED_MIRROR{"$dbname"}++;
      next;
    }

    #count the number of denied connections
    if (/\[(\d+)\] IRRD(.*)connection DENIED from\s+/) {
      if (/(\d+\.\d+\.\d+\.\d+)/) {
      	$deniedcount++;
      } else {
	$denied6count++;
      }
      next;
    }  

    if (/Processing mirror request from/) {
      if (/(\d+\.\d+\.\d+\.\d+) for ([\w\d\.\-]+)/) {
	$MIRROR_CLIENT{$1}++;			
	if (!($MD{$1} =~ /$2/)) {
	  $MD{$1} .= "$2 ";
	}
      } else { 
        foreach $ipv6pat (@ipv6_patterns) {
	  if (/($ipv6pat) for ([\w\d\.\-]+)/) {
	    $MIRROR_CLIENT6{$1}++;
	    if (!($MD{$1} =~ /$2/)) {
	      $MD{$1} .= "$2 ";
	    }
	    last;
	  }
	}
      }
      next;
    }

    #count the number of rejected connections
    if (/\[(\d+)\] IRRD Too many connections -- REJECTING/) {	
      $rejectedcount++;
    }
  }
}

sub mail_report {
  @hosts = keys %HOST_CONNECTIONS;
  @hosts6 = keys %HOST_CONNECTIONS6;
  @mirrors = keys %FAILED_MIRROR;

  $hostname = (split(/\./,`/bin/hostname`))[0];

  open (OUT, "|/usr/lib/sendmail -f daemon $RECIPIENTS");

  printf OUT "Subject: $today Daily IRRd Report $hostname\n";
  #  printf OUT "From: root+$hostname\@merit.edu\n";
  printf OUT "\n\n";
  printf OUT "IRRd Report:\t\t\t\t$start - $end\n";
  printf OUT "Start Time of Report Generation:\t$starttime\n";
  printf OUT "End Time of Report Generation:\t\t$endtime\n\n"; 

  printf OUT "Overall Statistics\n";
  printf OUT "Total Connections from IPv4 Hosts:\t$TOTAL_CONNECTIONS\n";
  printf OUT "Total Connections Denied:\t\t$deniedcount\n";
  printf OUT "Total Connections Rejected:\t\t$rejectedcount\n";
  printf OUT "Total Queries:\t\t\t\t$TOTAL_QUERIES\n";
  printf OUT "Total Bytes:\t\t\t\t%-6.2f (MB)\n", $TOTAL_BYTES / (1024 * 1024);
  printf OUT "Unique Hosts:\t\t\t\t%d\n", $#hosts + 1;
  printf OUT "Object Submissions:\t\t\t%d (%d okay, %d failed)\n\n", $NUM_OKAY+$NUM_FAILED, $NUM_OKAY, $NUM_FAILED;

  printf OUT "IPv6 Statistics\n";
  printf OUT "Total Connections from IPv6 Hosts:\t$TOTAL_CONNECTIONS6\n";
  printf OUT "Total IPv6 Object Queries:\t\t$TOTAL_QUERIES6\n";
  printf OUT "IPv6 Route Submissions:\t\t\t%d (%d okay, %d failed)\n\n", $NUM6_OKAY+$NUM6_FAILED, $NUM6_OKAY, $NUM6_FAILED;	
  
  printf OUT "\n";
  printf OUT "Top Ten Hosts by Volume of Queries:\n";

  @sortedhosts = sort byvolume @hosts;
  $i = 1;
  foreach $host (@sortedhosts) {
    $hostname = &lookup ($host);
    printf OUT " %-3s %-15s %-35s  %-9.2f (KB)  %-5d connections\n", "$i.",
      $host, $hostname, $HOST_BYTES{"$host"} / 1024, $HOST_CONNECTIONS{"$host"};
    if ($i++ > 10) {
      last;
    }
  }

  @n = keys %FAILED_MIRROR;
  print OUT "\nFailed mirror attempts (", $#n + 1, ")\n";
  foreach $mclient (sort failedcount keys %FAILED_MIRROR) {
    print OUT " $mclient (", $FAILED_MIRROR{$mclient}, ")\n";
  }

  @n = keys %MIRROR_CLIENT;
  print OUT "\nMirror Clients (", $#n + 1, ")\n";
  foreach $mclient (sort bycount keys %MIRROR_CLIENT) {
    $hostname = &lookup ($mclient);
    print OUT " $hostname (", $MIRROR_CLIENT{$mclient}, ")";
    chop ($MD{$mclient});
    print OUT "    [", $MD{$mclient}, "]\n";
  }

  @n = keys %IRRJ;
  print OUT "\nIRRj Users (", $#n + 1, ")\n";
  foreach $mclient (sort bycount keys %IRRJ) {
    $hostname = &lookup ($mclient);
    print OUT " $hostname (", $IRRJ{$mclient}, ")\n";
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

sub failedcount {
  $v1 = $MIRROR{$a};
  $v2 = $MORROR{$b};
  return ($v2 <=> $v1);
}

sub today {
  $today = time;
    
  ($a1, $a2, $a3, $mday, $mon, $year, @junk) = localtime ($today);

  $mon += 1;
  if ($mday < 10) {
    $mday = "0$mday";
  }
  if ($mon < 10) {
    $mon = "0$mon";
  }
  $year = $year % 100;
  if ($year < 10) {
    $year = "0$year";
  }
  return ("$year$mon$mday");
}

sub lookup {
  ($a) = @_;
  if ($a eq "") {
     print "Error: Empty name\n";
     return ($a);
  }
  $name = `nslookup -silent $a`;
  if ($name =~ /name\s\=\s+([\w\.\-\d]+)/) {
    $n = $1;
  } else {
    $n = $a;
  }
  return ($n);
}
