#!/usr/bin/perl
# RADB Query code

# need this to allow access to sockets
use Socket;

sub ParseUrlEncoded;  

$whoishost = "whois.radb.net";
$port =43;
$result = $ENV{QUERY_STRING};    # get the "searchst=<object>"
$SOCKADDR_IN = "S n A4 x8";
$PF_INET = 2;
$SOCK_STREAM = 2;  

# first two lines important for CGI
print "Content-type: text\/html\n";
print "\n"; 
{
    # convert each word.  Change + to space and %<char> to character
    foreach $word (split (/\&/, $result)) {
	($label, $val) = split(/=/, $word);
	$val = ParseUrlEncoded($val);
    }  
    
    # print the HTML header
    print "<head><title>RADB Search Form</title></head>\n";
    print "<body BGCOLOR=\"#FFFFFF\"><IMG SRC  = \"/icons/Merit/Merit-banner.gif\">";
    print "<h2>IRRWeb SearchForm</h2>\n";
    print "Search results for ", $val, ":\n<hr><pre>";

    # initialize the socket
    ($name, $aliases, $proto) = getprotobyname("tcp"); 
    socket(S, $PF_INET, $SOCK_STREAM, $proto) || die "socket failed: $!\n"; 

    # line buffered server
    select(S); $| = 1; select(STDOUT);
    ($name, $aliases, $type, $len, $thataddr) = gethostbyname($whoishost);
    $that = pack($SOCKADDR_IN, AF_INET, $port, $thataddr);
    connect(S, $that) || die "Connect failed\n";
    
    # send the request
    print S $val, "\n";
    print S "\n";
    
    # print response, each line seperated by <br>
    while ($line = <S>) {
	print $line;
	print "<br>";	
    }
    
    # close the socket and print the rest of HTML
    close(S);
    print "</pre><hr>";
    print "<div align=\"right\">";
    print "<font size=\"1\" face=\"Helvetica\">Merit Network";
    print " 4251 Plymouth Road  Suite C  Ann Arbor, MI 48105-2785";
    print "<br>734-764-9430 <br>info\@merit.edu";
    print "<hr>1999 Merit Network, Inc.<br>";
    print "<a href=\"mailto:www\@merit.edu\">www\@merit.edu</a></font>";
    print "</body></html>";
    }

sub ParseUrlEncoded {
    my ($value) = @_;
    
    # replace '+' with ' '
    $value =~ s/\+/ /g;
    
    # replace stuff like '%21' with '!'
    $value =~ s/%(..)/pack("c", hex($1))/ge;
    
    return $value;
} 
