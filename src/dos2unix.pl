#!/usr/local/bin/perl

@files = @ARGV;

foreach $file (@files) {
	system "dos2unix $file > l";
	system "mv l $file";
} 
