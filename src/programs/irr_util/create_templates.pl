#!/usr/bin/perl

%obj = ();
@attrs = ();
@obj_order = ();
%max_attr_len = ();

$m = 0;

while (<STDIN>) {

    next if (/^\#/);
    next if (/^\s*$/);
    
    
    if (/^ATTR\s+(..)\s+(\S+)\s+([^\s\#]+\s)?/) { 
	$ATTR{$1}=$1; 

	# these are the same
	$ATTL{$1}=$2;
	if (($len = length ($2)) > $m) {
	    $m = $len;
	}
	$map_long{$1} = $2;

	push (@attrs, $1);

	# this is the long field char name
	push (@long_attrs, $2);
	next;
    }
    
    
    if (s/^OBJ\s+(\S\S)\s+(\S+)\s+//) {
	
	$type=$1;
	$field=$2;
	
        # this is the bison 'enum OBJS' enumeration over each object
	if (!defined ($obj{$1})) {

	    $obj{$1} = 1;
	    push (@obj_order, $type);
	}
	
	s/\s+/ /g;
	s/ $//;
	
	if ($field eq "ATSQ") {
	    if ($OBJATSQ{$type}) {
		$OBJATSQ{$type}=join(" ", $OBJATSQ{$type}, $_);
	    }
	    else {
		$OBJATSQ{$type}=$_;
	    }
	}
	elsif ($field eq "MAND") {
	    if ($OBJMAND{$type}) {
		$OBJMAND{$type}=join(" ", $OBJMAND{$type}, $_);
	    }
	    else {
		$OBJMAND{$type}=$_;
	    }
	}
	elsif ($field eq "MULT") {
	    if ($OBJMULT{$type}) {
		$OBJMULT{$type}=join(" ", $OBJMULT{$type}, $_);
	    }
	    else {
		$OBJMULT{$type}=$_;
	    }
	}

	# print STDERR "-",$KEYS{$type}, "-\n";
	
	next;
	
    }
}

########################################################################

# compute the attr max length foreach obj
foreach $type (@obj_order) {
    $max_attr_len{$type}=0;
    foreach $j (split(/\s+/, $OBJATSQ{$type})) {
	next if ($j eq "ud");
	next if ($j eq "uo");
	next if ($j eq "up");
	next if ($j eq "uc");
	if (length ($map_long{$j}) > $max_attr_len{$type}) {
	    $max_attr_len{$type}=length ($map_long{$j});
	}
    }
}

$n = 0;
foreach $type (@obj_order) {

# for each attribute for this object
    $width = $max_attr_len{$type} + 1;
    $first_line = 1;
    foreach $j (split(/\s+/, $OBJATSQ{$type})) {
	next if ($j eq "ud");
	next if ($j eq "uo");
	next if ($j eq "up");
	next if ($j eq "uc");

    # print a line
	if (($pos=index ($OBJMAND{$type}, $j)) > -1) {
	    $mand = 'mandatory';
	}
	else {
	    $mand = 'optional';
	}

	if (index ($OBJMULT{$type}, $j) > -1) {
	    $mult = 'multiple';
	}
	else {
	    $mult = 'single';
	}

	if (!$first_line) {
	    print "\n";
	}

	if ($map_long{$j} eq "method" ||
	    $map_long{$j} eq "owner"  ||
	    $map_long{$j} eq "fingerpr") {
	    printf ("\"%-${width}s   %-11s   %-10s   ", 
		    $map_long{$j}.":", "["."generated"."]", "[".$mult."]");
	}
	else {
	    printf ("\"%-${width}s   %-11s   %-10s   ", 
		    $map_long{$j}.":", "[".$mand."]", "[".$mult."]");
	}
	
	if ($first_line) {
            $savefirst = $j;
	    if ($map_long{$j} eq "person" || $map_long{$j} eq "role") {
	    	print "[look-up key]\\n\"";
	    } else {
		print "[primary/look-up key]\\n\"";
	    }
	}
	elsif ($map_long{$j} eq "origin" && $map_long{$savefirst} ne "ipv6-site") {
	    print "[primary key]\\n\"";
	}
	elsif ($map_long{$j} eq "nic-hdl") {
	    print "[primary/look-up key]\\n\"";
	}
	elsif ($map_long{$j} eq "prefix") {
	    	print "[look-up key]\\n\"";
	}
	else {
	    print "[ ]\\n\"";
	}
	$first_line = 0;
    }
    print ",\n\n";

    $n++;
}

print "number of objects ($n)\n";


##########################################################################


close (OUT);

sub no_temp {
    my ($num_lines) = @_;

    while ($num_lines-- > 0) {
	print "\"%  No template available\\n\\n\",\n";	
    }
    print "\n";
}

1;
