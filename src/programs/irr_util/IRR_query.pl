#!/usr/bin/perl
#
# IRR_query
#
# Open a TCP connection to database and execute command, i.e.:
#   ./IRR_query.pl gas3
#
#


require "getopts.pl";

# Defaults
$SERVER = "whois.radb.net";
$PORT = 43;
$ECHO = 0;

&Getopts("vs:c:p:");
if (defined $opt_s) {
    $SERVER = $opt_s;
}
if (defined $opt_v) {
    $DEBUG = 1;
}
if (defined $opt_p) {
    $PORT = $opt_p;
}

&open_IRR_connection ($SERVER, $PORT);

$command = $ARGV[0];
$result = &IRR_command($command);
print "$result\n";

sub open_IRR_connection {
    local ($server, $port) = @_;
    $AF_INET = 2;
    $SOCK_STREAM = 2;
    $sockaddr = 'S n a4 x8'; 

    ($name, $aliases, $proto) = getprotobyname("tcp");
    #print "proto=<$proto>\n";
    #socket(TCP, PF_INET, SOCK_STREAM, $proto) || die "socket failed: $!\n";
    socket(TCP, $AF_INET, $SOCK_STREAM, $proto) || die "socket failed:	$!\n";
    #socket(TCP, PF_INET, SOCK_STREAM, $proto) || die "socket failed: $!\n";

    # make the socket line-bufffered
    select(TCP); $| = 1; select(stdout);

    ($name, $aliases, $addrtype, $length, @addrs) = gethostbyname($server);
    $saddr = pack($sockaddr, $AF_INET, $port, $addrs[0]);

    if ($DEBUG == 1) {
	print "Connecting to $server ($port)\n";
    }

    connect(TCP, $saddr) || die "** Error ** $!\n";

    if ($DEBUG == 1) {
	print "Connection to $server:$PORT opened\n";
    }

    # might be a comment
    $line = &read_line;
    if ($line ne "") {
	print "$line\n";
    }

    #print TCP "!!\n";
}



sub IRR_command {
    local ($command) = @_;

    #print "COMMAND: $command\n";
    if ($DEBUG == 1) {
	print "SENDING !$command\n";
    }
    if ($ECHO == 1) {print TCP "!$command\r\n";}
    else {print TCP "!!\n!$command\n";}

    if ($ECHO == 1) {
	# read our own command back
	$data = "";
	$line = "";
	$line = &read_line;
	
	if ($DEBUG == 1) {print "ECHO $line\n";}
    }

    $data = "";
    $line = "";
    $line = &read_line;

    if ($DEBUG == 1) {
	if ($line eq "") {
	    print "NULL\n";
	}
	else {print "ANSWER $line\n";}
    }

    if (! ($line =~ /^A/)) {
	if ($DEBUG == 1) {print "LINE $line \n";}
	return "";
    }

    if ($line =~ /^A(\d+)/) {
	$len = $1;
	#if ($DEBUG == 1) {print "LEN $len\n";}
	$total = 0;

	while ($total < $len) {
	    $buf = "";
	    $rin = $ein = $win = '';
	    vec ($rin, fileno(TCP), 1) = 1;
	    $nfound = select ($rout=$rin, undef, undef, 2);

	    $total += sysread (TCP, $buf, $len - $total);
	    $data .= $buf;
	    #print $buf;
	}

	$line = &read_line;
	
	if ($line eq "") {return ("$data");}
	if ($line eq "C") {return ("$data");}

	print "Error -- bad return code:$line:\n";
	exit;
    }
    else {return "";}
}

sub bitlen {
    local ($mask) = @_;

    $total = 0;
    @byte = split (/\./, $mask);

    foreach $byte (@byte) {
        if ($byte == 255) {$total += 8; next;}
        
        $vector = pack ("C", $byte);
        $bits = unpack ("B*", $vector);
        @bits = split (//, unpack ("B*", $vector));
        #print "$mask $byte @bits\n"; exit;
        foreach $bit (@bits) {
            if ($bit == 1) {$total++;}
            else {last;}
        }
    }
    
    return $total;
}


sub read_line {
    local ($n, $buf);
    $n = 0;
    $buf = "";
    $c = "";

    $rin = $ein = $win = '';
    vec ($rin, fileno(TCP), 1) = 1;
    $nfound = select ($rout=$rin, undef, undef, 3);

    if ($nfound <= 0) {return "";}

    while (($r = sysread (TCP, $c, 1) > 0)) {
	#print "$c\n";
        if ($c eq "\n") {return ($buf);}
        #if ($c eq "\r") {return (join ('', $buf));}
	if ($c eq "\r") {next;}
	if (! ($c =~ /[\w\#\.\-\/\,\s\'\:\!\(\)\*]/)) { $ECHO = 1; next; }


	$buf .= $c;
	$c = "";

	$rin = $ein = $win = '';
	vec ($rin, fileno(TCP), 1) = 1;
	$nfound = select ($rout=$rin, undef, undef, 5);
    }
    exit;
}


