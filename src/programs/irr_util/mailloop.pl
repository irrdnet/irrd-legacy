#!/usr/bin/perl
#
# check for mail bounce, if find it, divert message to specified account
# otherwise, write to stdout
#
# $Id: mailloop.pl,v 1.2 2001/06/12 18:07:16 ljb Exp $
#
 
$divertTo = "rradmin\@radb.ra.net";
$passMessage = 1;
$saveMsg = "";
$inHeader = 1;
$note = "\n";
 
while (<STDIN>)
{
	next if m/^X-pgp-(signature|error)/i;
        $saveMsg .= $_;
        $inHeader = 0  if ( /^$/ );
	if (/^Dear radb\@/) {
		$passMessage=0;
		$note .= "The message contained 'Dear radb\@', usually sent by autoresponders.";
	}
	if ($inHeader && /^From:.*[^a-z]support\@digex/) {
		$passMessage = 0;
		$note .= "Got hit by the autoresponder at digex.\n";
	}
	if (/^From: routing\@noc.netcom.net/) {
		$passMessage=0;
		$note .= "The From line matched the bozos who set up an automail bouncing loop.\n";
	}
        if ( /^From / && /(MAILER-DAEMON|[Pp]ostmaster|postmast)/i ) {
			$passMessage = 0;
			$note .= "The 'From ' line was from MAILER-DAEMON or postmaster - probably a mail bounce.\n";
		}

        if ( /^From / && ( / auto-dbm /i || /auto-dbm\@radb.ra.net/i) ) {
			$passMessage = 0;
			$note .= "The 'From ' line was from auto-dbm - probably a mail bounce.\n";
		}
        if ( $inHeader && /^From: /i &&
		     ( / auto-dbm /i || /auto-dbm\@radb.ra.net/i) ) {
			$passMessage = 0;
			$note .= "The 'From:' line was from auto-dbm - probably a mail bounce.\n";
		}

		# only bother with this message if haven't already detected problem
        if ( $inHeader && $passMessage && /^From: /i && 
		     /db-admin\@(ra|.+\.ra)\.net/i ) {

			$passMessage = 0;
			$note .= "The 'From:' line was from DB-admin - this might be a mail bounce/loop.\n";
			$note .= "This could also be a legitimate message - investigate carefully.\n";
		}
 
        if ( $inHeader && /^From: /i && /MAILER-DAEMON/i ) {
			$passMessage = 0;
			$note .= "The 'From:' line was from MAILER-DAEMON - probably a mail bounce.\n";
		}

        if ( $inHeader && /^Received:\s+\(from auto-dbm\@localhost\) by radb.ra.net/i) {
			$passMessage = 0;
			$note .= "A 'Received:' line containing '(from auto-dbm\@localhost) by radb.ra.net' was\n";
			$note .= "found.  This message is probably a loop back of a message originating\n";
			$note .= "from auto-dbm.\n";
		}
		
		$PrtFromSpace = "\t$_" if ( $passMessage && /^From / );
		$PrtFromColon = "\t$_" if ( $inHeader && /^From: / );
		$PrtSubject = "\t$_" if ( $inHeader && /^Subject: / );
		$PrtReturnPath = "\t$_"  if ( $inHeader && /^Return-Path: /i );
		$PrtSender = "\t$_"  if ( $inHeader && /^To: /i );

		$sawAuthorize = 1 if ( !$inHeader && /^\s*(authorise|\*ua):/ );
		$sawOverride = 1 if ( !$inHeader && /^\s*(override|\*uo):/ );

		$FromSpace = $1 if ( /^From\s+(\S+)/i );
		$FromColon = $1 if ( /^From:\s+(\S+)/i );
		$Subject = $1 if ( /^Subject:\s+(.*)/i );
		$To = $1 if ( /^To:\s+(\S+)/i );
}

		# if this message is going to db-admin and it's a response and
		# we haven't already flagged it, then, well, flag it

		if ($passMessage && $To =~ /db-admin\@.*((ra|merit).net|merit.edu)/i &&
		    $Subject =~ /^Re:/i ) {

			$passMessage = 0;
			$note .= "This is a message going to db-admin in which the subject starts with 'Re:'\n";
			$note .= "it might be an indication of a mail loop.\n";
			$note .= "NOTE: THIS COULD BE A LEGITIMATE MESSAGE OR A SUBTLE PROBLEM - review closely.\n";
		}
 
if ( $passMessage )
{
        print $saveMsg;

}
else
{
		$note .= "\nThis message has NOT been processed by dbupdate.\n";
		$note .= "Please check out the message and take the appropriate action.\n";

        open (MAIL, "|/usr/lib/sendmail $divertTo") ||
                        die "Cannot open MAIL: $!";
        print MAIL <<"EOF";
Reply-To: $divertTo
To: $divertTo
From: Bounced mail checker <$divertTo>
Subject: Trapped an apparent mail bounce/loop on radb
 
This seems to be bounced mail or a mail loop to auto-dbm\@radb.ra.net:
$note 
======================================================================
>$saveMsg
EOF
        close(MAIL);
}
 
exit 0;

