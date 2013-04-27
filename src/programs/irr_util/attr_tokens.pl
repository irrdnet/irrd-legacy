#!/usr/bin/perl

open (OUT1, ">../irr_rpsl_check/attrs.rpsl") or die "Can't open output file-(attrs.rpsl)\n";
open (OUT2, ">tokens.rpsl") or die "Can't open output file-(tokens.rpsl)\n";
open (OUT3, ">../../include/irr_rpsl_tokens.h") or die "Can't open output file-(irr_rpsl_tokens.h)\n";
open (IN, "<$ARGV[0]") or die "Can't open input file-($ARGV[0])\n";
$tokens = $bison_tokens = $lex_rules = "";
$enum_attrs  = "enum ATTRS {\n";
$enum_objs   = "enum OBJS {\n";
$n = 0;    # this will be the number of known attributes
$m = 0;    # this will be the number of known objects
%map = (); # this hash will map attr name to integer needed by the C-arrays
%obj = ();
@attrs = ();
@obj_order = ();
@country = ();


while (<IN>) {

    next if (/^\#/);
    next if (/^\s*$/);
    
    last if (/^ENDCONF/);
    
    if (/^ATTR\s+(..)\s+(\S+)\s+([^\s\#]+\s)?/) { 
	$ATTR{$1}=$1; $ATTL{$1}=$2;

	$map{$1} = $n;
	
	$MAXATTRIBUTELENGTH=length($2) if (length($2)>$MAXATTRIBUTELENGTH);
	
	($upper = $1) =~ tr/a-z/A-Z/;	

	
	# this is the bison 'enum ATTRS' enumeration over each field
	if ($n == 0) {
	    $enum_attrs .= "    F_${upper} = 0";
	}
	elsif ($n % 8 == 0) {
	    $enum_attrs .= "    F_${upper}";
	}
	elsif ($n % 8 == 7) {
	    $enum_attrs .= ", F_${upper},\n";
	}
	else {
	    $enum_attrs .= ", F_${upper}";
	}

	# this is the token -> int mapping array
	if ($n % 7 == 0) {
	    $tokens .= "T_${upper}_KEY";
	}
	elsif ($n % 7 == 6) {
	    $tokens .= ", T_${upper}_KEY,\n"; 
	}
	else {
	    $tokens .= ", T_${upper}_KEY"; 
	}
	
	# these are the bison tokens for each known field
	if ($n % 7 == 0) {
	    $bison_tokens .= "%token  T_${upper}_KEY";
	}
	elsif ($n % 7 == 6) {
	    $bison_tokens .= " T_${upper}_KEY\n";
	}
	else {
	    $bison_tokens .= " T_${upper}_KEY";
	}
	$n++;
	push (@attrs, $1);

	# this is the long field char name
	push (@long_attrs, $2);
	$map_long{$1} = $2;
	next;
    }
    
    
    if (s/^OBJ\s+(\S\S)\s+(\S+)\s+//) {
	
	$type=$1;
	$field=$2;
	
        # this is the bison 'enum OBJS' enumeration over each object
	if (!defined ($obj{$1})) {
	    ($upper = $1) =~ tr/a-z/A-Z/;
	    if ($m == 0) {
		$enum_objs .= "    O_${upper} = 0";
	    }
	    elsif ($m % 8 == 0) {
		$enum_objs .= "    O_${upper}";
	    }
	    elsif ($m % 8 == 7) {
		$enum_objs .= ", O_${upper},\n";
	    }
	    else {
		$enum_objs .= ", O_${upper}";
	    }
	    $m++;
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


    if (/^COUNTRY\s+(..)/) { 
	$countries++;
	push (@country, $1);
    }
    
}

print OUT1 "/*--------Bison legal attr------*/\n";
$legal_attrs = "short legal_attrs[MAX_OBJS][MAX_ATTRS] = {\n";
foreach $type (@obj_order) {
    %legalfield = ();
    $wt = 1;
    foreach $i (split(/\s+/, $OBJATSQ{$type})) {
#	$legalfield{$i}=1;
	$legalfield{$i}=$wt++;
    }
    foreach $i (("ud","uo","uw","ue")) {
	$legalfield{$i}= 1024;
    }

    $count = 0;
    $legal_attrs .= "{";
    foreach $field (@attrs) {
	$count++;
	if (defined ($legalfield{$field})) {
	    $wt = $legalfield{$field};
	    if ($count % 20 == 0) {
		$legal_attrs .= " $wt,\n";
	    }
	    else {
		$legal_attrs .= " $wt,";
	    }
	}
	else {
	    if ($count % 20 == 0) {
		$legal_attrs .= " 0,\n";
	    }
	    else {
		$legal_attrs .= " 0,";
	    }
	}
    }
    $legal_attrs .= "},\n";
}
$legal_attrs .= "};\n";
print OUT1 "$legal_attrs";

print OUT1 "\n/*--------Bison multiple_attrs------*/\n";
$mult_attrs = "short mult_attrs[MAX_OBJS][MAX_ATTRS] = {\n";
foreach $type (@obj_order) {
    %multfield = ();
    foreach $i (split(/\s+/, $OBJMULT{$type})) {
	$multfield{$i}=1;
    }

    $count = 0;
    $mult_attrs .= "{";
    foreach $field (@attrs) {
	$count++;
	if (defined ($multfield{$field})) {
	    if ($count % 20 == 0) {
		$mult_attrs .= " 1,\n";
	    }
	    else {
		$mult_attrs .= " 1,";
	    }
	}
	else {
	    if ($count % 20 == 0) {
		$mult_attrs .= " 0,\n";
	    }
	    else {
		$mult_attrs .= " 0,";
	    }
	}
    }
    $mult_attrs .= "},\n";
}
$mult_attrs .= "};\n";
print OUT1 "$mult_attrs";

$attr_name = "char *attr_hash[MAX_OBJS][MAX_ATSQ_LEN] = {\n";
$attr_map_field = "short attr_map_field[MAX_OBJS][MAX_ATSQ_LEN] = {\n";
$max_atsq_len = 0;
foreach $type (@obj_order) {
    $count = 1;
    $attr_name .= "{";
    $attr_map_field .= "{";
    foreach $i (split(/\s+/, $OBJATSQ{$type})) {
	$count++;
	$attr_name .= "\"$map_long{$i}\", ";
        $attr_map_field .= " $map{$i},";
    }
    
    $attr_name .= "\"\"},\n";
    $attr_map_field .= " -1},\n";
    $max_atsq_len = $count if (++$count > $max_atsq_len);
}
$attr_name .= "};\n";
$attr_map_field .= "};\n";
print OUT1 "\n/*--------attr_hash------*/\n";
print OUT1 "$attr_name";
printf OUT1 "\n";
print OUT1 "\n/*--------attr_map_field------*/\n";
print OUT1 "$attr_map_field";


print OUT1 "\n/*--------Bison mandatory's------*/\n";
$mands = "short mand_attrs[MAX_OBJS][MAX_MANDS] = {\n";
$max_mands = 0;
foreach $type (@obj_order) {
    $count = 0;
    $mands .= "{";
    foreach $i (split(/\s+/, $OBJMAND{$type})) {
	$count++;
	$mands .= " $map{$i},";
    }
    
    $mands .= " -1},\n";
    $max_mands = $count if (++$count > $max_mands);
}
$mands .= "};\n";
print OUT1 "$mands";

print OUT1 "\n/*--------attrs which are keys-----------*/\n";
$attr_is_key = "short attr_is_key[MAX_ATTRS] = {\n";
for ($i=0;$i < $n;$i++) {
  $attr_is_key[$i] = -1;
}
$i = 0;
foreach $key (@obj_order) {
  $attr_is_key[$map{$key}] = $i++;
}
for ($i = 0;$i < $n;$i++) {
  if ($i == 0) {
    $attr_is_key .= "$attr_is_key[$i]";
  }
  elsif (($i + 1) % 15 == 0) {
    $attr_is_key .= ", $attr_is_key[$i],\n";
  }
  elsif (($i + 1) % 15 == 1) {
    $attr_is_key .= "$attr_is_key[$i]";
  }
  else {
    $attr_is_key .= ", $attr_is_key[$i]";
  }
}
if ($i % 15 == 0) {
    $attr_is_key .= "};\n";
}
else {
    $attr_is_key .= "\n};\n";
}
print OUT1 "$attr_is_key";

print OUT1 "\n/*-------long attributes char array------*/\n";
$count = 0;
$longs = "char *attr_name[MAX_ATTRS] = {\n";
foreach $field (@long_attrs) {
    $count++;
    if ($count % 5 == 1) {
	$longs .= "    \"$field\"";
    }
    elsif ($count % 5 == 0) {
	$longs .= ", \"$field\",\n";
    }
    else {
	$longs .= ", \"$field\"";
    }
}
if ($count % 5 == 0) {
    $longs .= "};\n";
}
else {
    $longs .= "\n};\n";
}
print OUT1 "$longs";


#print OUT1 "\n/*-------short attributes char array------*/\n";
#$count = 0;
#$shorts = "char *attr_sname[MAX_ATTRS] = {\n";
#foreach $field (@attrs) {
#    $count++;
#    if ($count % 10 == 1) {
#	$shorts .= "    \"*$field\"";
#    }
#    elsif ($count % 10 == 0) {
#	$shorts .= ", \"*$field\",\n";
#    }
#    else {
#	$shorts .= ", \"*$field\"";
#    }
#}
#if ($count % 10 == 0) {
#    $shorts .= "};\n";
#}
#else {
#    $shorts .= "\n};\n";
#}
#print OUT1 "$shorts";

print OUT1 "\n/*-------object types char array------*/\n";
$count = 0;
$longs = "char *obj_type[MAX_OBJS] = {\n";
foreach $i (@obj_order) {
    $count++;
    $field = $map_long{$i};
    if ($count % 5 == 1) {
	$longs .= "    \"$field\"";
    }
    elsif ($count % 5 == 0) {
	$longs .= ", \"$field\",\n";
    }
    else {
	$longs .= ", \"$field\"";
    }
}
if ($count % 5 == 0) {
    $longs .= "};\n";
}
else {
    $longs .= "\n};\n";
}
print OUT1 "$longs";

print OUT1 "\n/* -------Countries char array------ */\n";
$count = 0;
$origin = "#define MAX_COUNTRIES $countries\n";
$origin .= "char *countries[MAX_COUNTRIES] = {\n";
foreach $field (@country) {
    $count++;
    if ($count % 10 == 1) {
        $origin .= "    \"$field\"";
    }
    elsif ($count % 10 == 0) {
        $origin .= ", \"$field\",\n";
    }
    else {
        $origin .= ", \"$field\"";
    }
}
if ($count % 10 == 0) {
    $origin .= "};\n";
}
else {
    $origin .= "\n};\n";
}
print OUT1 "$origin\n";

close (OUT1);

print OUT2 "\n\n\n/********************** rpsl.y **************************/\n";
print OUT2 "\n/*--------Bison tokens--------*/\n";
if ($n % 7 != 6) {
    $bison_tokens .= "\n";
}
print OUT2 "$bison_tokens";


print OUT2 "\n\n\n/********************** rpsl.fl **************************/\n";
print OUT2 "\n/*--------token -> int map----*/\n";
print OUT2 "int attr_tokens[MAX_ATTRS] = {\n";
print OUT2 "$tokens";
print OUT2 "};\n";

close (OUT2);


print OUT3 "#define     F_NOATTR      -1\n";
print OUT3 "#define     MAX_ATTRS $n\n";
print OUT3 "#define     MAX_OBJS $m\n";
print OUT3 "#define     MAX_ATSQ_LEN $max_atsq_len\n"; 
print OUT3 "#define     MAX_MANDS $max_mands\n";

print OUT3 "\n/* --------Bison enum FIELDS--------*/\n";
if ($n % 8 != 7) {
    $enum_attrs .= "\n};";
}
else {
    $enum_attrs .= "};\n";
}
print OUT3 "$enum_attrs";

print OUT3 "\n/*--------Bison enum OBJS--------*/\n";
if ($m % 8 != 7) {
    $enum_objs .= "\n};\n";
}
else {
    $enum_objs .= "};\n";
}
print OUT3 "$enum_objs";

close (OUT3);

1;
