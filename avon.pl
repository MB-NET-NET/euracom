#!/usr/bin/perl -w

#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# avon.pl -- TelNo -> FQTN converter
#
# Copyright (C) 1996-1997 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-09-02 11:03:41 GMT
# Version:             $Revision: 1.7 $
# Last modified:       $Date: 1998/01/15 15:08:01 $
# Keywords:            ISDN, Euracom, Ackermann
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public Licence as published by the
# Free Software Foundation; either version 2 of the licence, or (at your
# opinion) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of merchanability
# or fitness for a particular purpose.  See the GNU Public Licence for
# more details.
#**************************************************************************

#
# $Id: avon.pl,v 1.7 1998/01/15 15:08:01 bus Exp $
#

use Pg;

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
$db = Pg::connectdb("host=$main::db_host dbname=$main::db_db");
if ($db->status!=PGRES_CONNECTION_OK) { 
  $msg=$db->errorMessage;
  die "Open DB failed: $msg";
}
$my_db=$db->db; $my_user=$db->user; $my_host=$db->host; $my_port=$db->port;
debug("connected to table $my_db on $my_user\@$my_host:$my_port\n");

while (<>) {
  chop;

  printf "\t%s\n", print_fqtn($_);
}
debug("Connection closed\n");

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
  if ($avon) { $msg.="($avon) "; }
  if ($telno) { $msg.="$telno"; } else { $msg.="(No number)"; }
  if ($rest) { $msg.=" - $rest"; }
  $msg.=";";
  if ($wkn) { $msg.=" $wkn"; }
  if ($avon_name) { $msg.=" ($avon_name)"; }
  return($msg);
}
