#!/bin/perl

use strict;
use POSIX qw(tmpnam);
use utf8;

# TODO: random mirror

my $mirror = "https://cran.revolutionanalytics.com";
my $file = "/web/packages/available_packages_by_name.html";
my $local = "packages.html";
my $method = "curl";
my $output = "packages.json";

my $fetch = 0;
my $console = 0;

foreach my $arg (@ARGV){
  if( $arg =~ /--mirror=(.*)$/ ){ $mirror = $1; }
  elsif( $arg =~ /--output=(.*)$/ ){ $output = $1; }
  elsif( $arg =~ /--local=(.*)$/ ){ $local = $1; }
  elsif( $arg =~ /--method=(.*)$/ ){ $method = $1; }
  elsif( $arg =~ /--fetch/ ){ $fetch = 1; }
  elsif( $arg =~ /--console/ ){ $console = 1; }
  elsif( $arg =~ /(?:--help|-\?)/){
    print <<END;

This script reads the html package table available on CRAN mirrors and 
generates a JSON file containing short descriptions of packages.  I'm sure 
this data is available somewhere in a usable format, but I can't find it.

The intent is to host the package file on github, and fetch it via CDN.
We will update periodically so it should be read using the commit ID.

Usage: perl $0 options 
options: 
  --mirror=cran-mirror-url     a cran mirror expected to host the package table
  --output=output-json-file    path to generated json file (packages.json)
  --local=temp-html-file       path to temporary download file (packages.html)
  --method=(curl|wget)         select download method; defaults to curl
  --fetch                      if omitted, script will read existing html file
  --console                    write JSON to console instead of output file
  --help, -?                   print this message and exit

END
    exit;
  }
}

print "using mirror: $mirror\n";

if( $fetch ){
  # CRAN-listed mirrors generally include a trailing slash, clean up JIC
  $mirror =~ s/\/+$//g;
  my $url = $mirror . $file;
  print "fetching file...\n";
  `rm $local` if -e $local;
  if( $method eq "wget" ){ `wget -O $local $url`; }
  elsif( $method eq "curl" ){ `curl -o $local $url`; }
  else{ die( "invalid download method '$method'"); }
}
else {
  print "using existing file\n";
}

my $contents;
open( F, $local ) or die( "ERROR opening $local\n");
while( my $line = <F>){ $contents .= $line; }
close(F);

#
# I know there's a library for this.  this script attempts to minimize 
# dependencies.  this is the full set that I've seen in the list so far.
#

my %HTMLEntities = (
  "&ndash;" => "\x{2013}",
  "&mdash;" => "\x{2014}",
  "&lsquo;" => "\x{2018}",
  "&rsquo;" => "\x{2019}",
  "&ldquo;" => "\x{201c}",
  "&rdquo;" => "\x{201d}",
  "&gt;" => ">",
  "&lt;" => "<",
  "&amp;" => "&" # last
);

my ($reEntities) =
  map qr/$_/,
  join '|',
  map quotemeta,
  keys(%HTMLEntities);

my @entries;
while( $contents =~ /<tr>\s*<td>\s*<a href=".*?">(.*?)<\/a>\s*<\/td>\s*<td>(.*?)<\/td>\s*<\/tr>/gs ){
  my ($name, $desc) = ($1, $2);
  $desc =~ s/\"/\\"/g;
  $desc =~ s/\s+/ /g;
  $desc =~ s/($reEntities)/$HTMLEntities{$1}/g;
  push @entries, "\"$name\": \"$desc\"";
}

my $date = time();
my $count = scalar @entries;
my $entries = join(",\n    ", @entries);

my $json;
utf8::upgrade($json);

$json = <<END;
{
  "description": "This file is a map of CRAN package names to short descriptions, generated from a CRAN mirror.",
  "for-more-information": "https://github.com/sdllc/BERTConsole/tree/master/util",
  "source": "$mirror",
  "date": $date,
  "package-count": $count,
  "packages": {
    $entries
  }
}
END

utf8::encode($json);

if( $console ){
  print $json;
}
else {
  open( F, ">$output" ) or die( "ERROR opening $output for writing\n" );
  print F $json;
  close(F);
}

