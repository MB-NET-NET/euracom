#!/usr/bin/perl -w

#***************************************************************************
# cidlookup.pl -- TelNo -> FQTN converter
#
# Copyright (C) 2014 Michael Bussmann
#
# Authors:             Michael Bussmann <bus@mb-net.net>
# Created:             2014-04-16 12:20:19 GMT
# Keywords:            ISDN, Euracom, Ackermann
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public Licence version 2 as published
# by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of merchanability
# or fitness for a particular purpose.  See the GNU Public Licence for
# more details.
#**************************************************************************

#
# Debug:
# agi_callerid: 01786263810
# agi_callerid: 003521331
# agi_callerid: 60706
# agi_callerid: 6070642
# agi_callerid: 6070645
# agi_callerid: 607060
# agi_callerid: 022263884
#

use strict;
use Asterisk::AGI;
use DBI;
use DBD::Pg;

#
# Use unbuffered I/O
#
$|=1;

my %fqtn_cache;

$main::debugp=0;
$main::dbh="";

#
# Prototypes
#
sub do_lookup($);

# initialize the AGI module and read incoming arguments
my $AGI = new Asterisk::AGI;
my %input = $AGI->ReadParse();

# extract the caller ID
my $callerid = $input{'callerid'};

my $cidnum = $callerid;

# if the caller ID is in "name" <number> form, extract the number 
$cidnum = $1 if $cidnum =~ /<(\d*)>/;

# remove all other non-digits
$cidnum =~ s/[^\d]//g;

# depending on where you live, the following part is going to be
# different. We're assuming local calls have no prefix, national
# calls have a single zero prefix, and worldwide calls have a
# two zeros prefix. Additionally, we're assuming that numbers
# in our databse start with the country code.

if ($cidnum =~ /^00/) {
        # International call: 003521331 -> +3521331
        $cidnum =~ s/^00/+/;
} elsif($cidnum =~ /^0/) {
        # if it's a national call, remove the prefix...
        $cidnum =~ s/^0//;
        # ..and add our own country code
        $cidnum = '+49' . $cidnum;
} else {
	# internal call: 2 digits
	if (length($cidnum)>2) {
        	# if it's a local call, add our own country and local codes
        	$cidnum = '+492364' . $cidnum;
	}
}
 
# look the normalized number up
my $caller = do_lookup($cidnum);

if($caller) {
        # finally, pass the name along with the normalized number
        # back to Asterisk, if lookup was successful
        $AGI->set_callerid('"' . $caller . '"<' . $callerid . '>');
}
 
# we don't have to do anything if we don't change the caller ID
exit 0;

#
# Lookup routine
#

sub do_lookup($)
{
	my ($callerid) = @_;
	my $caller;
	my $msg="";
	my %tel;

	$main::dbh=DBI->connect("dbi:Pg:dbname=isdn", "phone", "Iphei5jeev",
		{RaiseError=>1, AutoCommit=>0}) || die "Connect failed: $DBI::errstr";

	%tel=convert_fqtn($callerid);

	$main::dbh->disconnect() || warn $DBI::errstr;

	# Strip +49 from avon
	$tel{'avon'}=~s/^\+49/0/;

	# Add int. exit code
	$tel{'avon'}=~s/^\+/00/;

	# Construct HTML
	$msg="";

	if ($tel{'wkn'}) {
		$msg.="$tel{'wkn'}";
		$msg.=" - $tel{'rest'}" if ($tel{'rest'});
		$msg.=" ($tel{'avon_name'})" if ($tel{'avon_name'});
	} else {
		$msg.="($tel{'avon'}) " if ($tel{'avon'});
		$msg.=($tel{'telno'}?$tel{'telno'}:"(No number)");
		$msg.=" - $tel{'rest'}" if ($tel{'rest'});
		$msg.=";";
		$msg.=" ($tel{'avon_name'})" if ($tel{'avon_name'});
	}
		
	return($msg);
}

#
# (int) SQLselect(cmd, callback)
#
# Executes cmd (SELECT) statement, calls 'callback' for
# each row passing $res as a parameter
#
sub SQLselect()
{
  my ($cmd, $callback) = @_;
  my ($res);
  my ($sth);
  my ($i);
  my (@data);

	debug("SQLselect: Executing $cmd using callback $callback\n");
	$sth=$main::dbh->prepare($cmd) || warn "Prepare $cmd failed: $DBI::errstr";
	$sth->execute || warn "Execute $cmd failed: $DBI::errstr";

  $i=0;
  while (@data=$sth->fetchrow_array) {
    &$callback(@data);
    $i++;
  }
  die $sth->errstr if $sth->err;
  $sth->finish;

  debug("Total $i records retrieved\n");
  return($i);
}

#
# (key, value, residual) split_text(table, input)
#
# Performs a prefix search on table 'table' for 'input'
# Returns 'key', 'value' and 'residual'
#
sub split_text()
{
	my ($table, $input) = @_;
	my (@data, @row);
	my ($residual, $resi_best);
	my ($sth);
	my $cmd;

	#
	# Use external prefix_match function
	#
	$cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";

	#
	# Use Oracle compat functions
	#
	#  $cmd="SELECT nummer,name FROM $table WHERE nummer=substr('$input', 1, length(nummer))";

	$sth=$main::dbh->prepare($cmd) || warn "Prepare $cmd failed: $DBI::errstr";
  	$sth->execute || warn "Execute $cmd failed: $DBI::errstr";

  while (@row = $sth->fetchrow_array) {
    push @data, [ (@row) ];
  }
  die $sth->errstr if $sth->err;
  $sth->finish;

  # Shortcuts
  return($input, 0, 0 ) if (@data==0);
  return($data[0][0], $data[0][1], substr($input, length($data[0][0]))) if (@data==1);

  # Sort array by length of field 0 (nummer)
  @data = sort { length(${$a}[0]) <=> length(${$b}[0]) } @data;

  # Print nice residual
  $residual=substr($input, length($data[0][0]));
  if ($resi_best=substr($input, length($data[-1][0]))) {
    $residual=substr($residual,0, length($residual)-length($resi_best))."/".$resi_best;
  }

  return($data[0][0], $data[-1][1], $residual);
}

#
# (avon, telno, postfix, avon_name, wkn_name) convert_fqtn(num)
#
# Converts a int. telephone number into a FQTN structure
#
sub convert_fqtn()
{
  my ($num) = @_;
  my ($avon, $avon_name, $telno, $tel, $wkn, $rest);
  my ($msg);
  my %tel;

  debug("convert_fqtn(): Converting $num:\n");

  # Try to locate number in cache
  if (exists $fqtn_cache{$num}) {
    debug(" --> found  in cache");
    ($avon, $telno, $rest, $avon_name, $wkn) = @{ $fqtn_cache{$num} };
  } else {

    # First look up AVON info
    debug("Querying database\n\tSearch avon: ");
    ($avon, $avon_name, $telno) = (&split_text("avon", $num, 1));
    debug("$avon; $avon_name; $telno");
    if (!$avon_name) { $telno=$avon; $avon=""; }

    # Look up WKN info
    debug("\n\tWKN info: ");
    ($telno, $wkn, $rest) = (&split_text("wkn", $avon.$telno, length($avon)));
    debug("$telno; $wkn; $rest");
    $telno=substr $telno, length($avon);

    # Strip +49 from avon
    $avon=~s/^\+49/0/;

    # Add int. exit code
    $avon=~s/^\+/00/;

    # Insert in cache
    $fqtn_cache{$num}=[ ($avon, $telno, $rest, $avon_name, $wkn) ];
  }

  debug("\n\t\tAVON : $avon ($avon_name)\n\t\tTelNo: $telno ($wkn)\n\t\tPost : $rest\n");
  %tel=(
	avon => "$avon",
	avon_name => "$avon_name",
	prov => "01070",
	prov_name => "Dirty Provider",
	telno => "$telno",
	wkn => "$wkn",
	rest => "$rest"
  );
  return(%tel);
}

#
# Prints debugging info if requested
#
sub debug()
{
  print STDERR @_ if ($main::debugp);
}
