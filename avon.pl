#!/usr/bin/perl -w

#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# avon.pl -- TelNo -> FQTN converter
#
# Copyright (C) 1996-1998 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-08-29 09:44:19 GMT
# Version:             $Revision: 1.12 $
# Last modified:       $Date: 1999/12/27 18:23:48 $
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
# $Id: avon.pl,v 1.12 1999/12/27 18:23:48 bus Exp $
#

use DBI;

# Add directory this script resides in to include path
if ($i=rindex($0, "/")) {
  push(@INC,substr($0,0,$i));
}

require 'getopts.pl';
require 'tel-utils.pm';

$| = 1;

#
# Parse CMD line params
#
$opt_H=$opt_D=$opt_d=$opt_h=$main::debugp=undef;
&Getopts('dhH:D:');
$main::db_host= $opt_H || "tardis";
$main::db_db  = $opt_D || "isdn";
$main::debugp = $opt_d || "";

#
# Print help
#
if ($opt_h) {
  print <<"EOF";
Usage: $0 [options]

  -H host       Sets database host
  -D name       Sets database name to connect to

  -d            Enable debug mode
  -h            You're reading it
EOF
  exit(0);
}

#
# Fire up connection
#
debug("Opening connection...");
$dbh=DBI->connect("dbi:Pg:dbname=$main::db_db;host=$main::db_host", "", "",
	{RaiseError=>1, AutoCommit=>0}) || die "Connect failed: $DBI::errstr";
debug("ok\n");

while (<>) {
  chop;

  printf "\t%s\n", print_fqtn($_);
}

#
# Disconnect gracefully
#
debug("Closing connection...");
$dbh->disconnect() || warn $DBI::errstr;
debug("ok\n");

#
# sub print_fqtn()
#
sub print_fqtn()
{
  my ($num) = @_;
  my ($avon, $avon_name, $telno, $wkn, $rest);
  my ($msg);

  ($avon, $telno, $rest, $avon_name, $wkn)=convert_fqtn($num);

  # Strip +49 from avon
  $avon=~s/^\+49/0/;

  # Add int. exit code
  $avon=~s/^\+/00/;

  # Construct HTML
  $msg="";
  $msg.="($avon) " if ($avon);
  $msg.=($telno?$telno:"(No number)");
  $msg.=" - $rest" if ($rest);
  $msg.=";";
  $msg.=" $wkn" if ($wkn);
  $msg.=" ($avon_name)" if ($avon_name);
  return($msg);
}
