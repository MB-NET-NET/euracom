#!/usr/bin/perl -w

#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# charger.pl -- Gebührenauswertung via PostgreSQL
#
# Copyright (C) 1996-1998 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-09-02 11:03:41 GMT
# Version:             $Revision: 1.13 $
# Last modified:       $Date: 1999/03/13 16:56:58 $
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
# $Id: charger.pl,v 1.13 1999/03/13 16:56:58 bus Exp $
#

use Pg;

# Add directory this script resides in to include path
if ($i=rindex($0, "/")) {
  push(@INC,substr($0,0,$i));
}

require 'getopts.pl';
require 'tel-utils.pm';

#
# This could be: sys_date (using timestamp when data was inserted into DB)
#              : vst_date (using time as reported by Euracom)
#
$usedate="sys_date";

#
# Make output unbuffered (e.g. for cgi scripts)
#
$| = 1;

#
# Parse CMD line params
#
$opt_h=$opt_H=$opt_D=$opt_v=$opt_b=$opt_X=$opt_t=$opt_B=$opt_d=$debugp=undef;

&Getopts('hdH:D:v:b:t:X:B:');

$db_host= $opt_H || "tardis";
$db_db  = $opt_D || "isdn";

$von    = $opt_v || "";
$bis    = $opt_b || "";
$MSN    = $opt_X || "";
$tln    = $opt_t || "";
$charge = $opt_B || 18.0;

$debugp = $opt_d || "";

#
# Print help
#
if ($opt_h) {
  print <<"EOF";
Usage: $0 [options]

  -H host	Sets database host
  -D name	Sets database name to connect to

  -v dspec	Date/Time
  -b dspec	Date/Time
  -t no[,no...]	Limit to these internal numbers

  -X msn	Sets title
  -B amount	Sets base charge

  -d		Enable debug mode
  -h		You're reading it
EOF
  exit(0);
}

#
# Fire up connection
#
debug("Opening connection...");
$db = Pg::connectdb("host=$db_host dbname=$db_db");
if ($db->status!=PGRES_CONNECTION_OK) {
  $msg=$db->errorMessage;
  die "Open DB failed: $msg";
}
$my_db=$db->db; $my_user=$db->user; $my_host=$db->host; $my_port=$db->port;
debug("connected to table $my_db on $my_user\@$my_host:$my_port\n");

#
# Create filter expression and build internal list
#
&debug("Creating filter expression...");
$filter_count=0;
if ($von) { $filter_cmd[$filter_count++]="$usedate>='$von'"; }
if ($bis) { $filter_cmd[$filter_count++]="$usedate<='$bis'"; }
if ($tln) {
  $flag=0; $tmp="(";
  @tln = split(",", $tln);
  map {
    if ($flag) { $tmp.=" OR "; } else { $flag=1; }
    $tmp.="int_no='$_'";
  } @tln;
  $filter_cmd[$filter_count++]=$tmp.")";
}

#
# Construct real filter statement
#
$flag=0; $filter_cmd="";
map {
  if ($flag) { $filter_cmd.=" AND "; } else { $filter_cmd.="WHERE "; $flag=1;}
  $filter_cmd.=$_;
} @filter_cmd;
&debug("o.k.: $filter_cmd\n");

#
# Print bloody HTML header
#
print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n";
print "<html><head><title>Telefonrechnung</title></head><body>\n";
print "<h1>Telefonrechnung f&uuml;r Anschlu&szlig; $MSN</h1>\n";
print "<center><table border=\"5\" cellspacing=\"2\" cellpadding=\"2\">\n";
print "<tr><th>Anschlu&szlig;</th><th>Datum</th> <th>Rufnummer</th><th>Einheiten</th><th>Betrag</th></tr>\n";

#
# Perform evaluation
#
$counter=0;
SQLselect("SELECT int_no,remote_no,date_part('epoch', $usedate),einheiten,direction,pay,currency,$usedate FROM euracom $filter_cmd ORDER BY $usedate", "eval_result");

#
# That's it
#
&debug("o.k.\n");

#
# Print footer
#
$res=$db->exec("SELECT sum(einheiten), sum(pay) from euracom $filter_cmd") || die "SELECT sum";
$val1=$res->getvalue(0,0);
$val2=$res->getvalue(0,1);
print "<tr><td></td><td></td><td>$counter Gespr&auml;che</td><td>$val1</td><td>$val2 DEM</td></tr>\n";
printf "<tr><td></td><td></td><td>Grundgeb&uuml;hr</td><td></td><td>%.2f DEM</td></tr>\n", $charge;
printf "<tr><td></td><td></td><td>GESAMT:</td><td></td><td><B>%.2f DEM</B></td></tr>\n", $val2+$charge;
print "</table></center><br><hr><address>";
print "Michael Bussmann, Im Brook 8, 45721 Haltern</address></body></html>\n";

debug("Closing connection\n");
undef $db;

#
# eval_result()
#
sub eval_result()
{
  # int_no, remote_no, *_date, EH, dir, pay, cur, *_date (human readable)
  # 0       1          2       3   4    5    6    7
  my (@arr) = @_;
  my ($num, $i);
  my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($arr[2]);

  # Interne Nummer und Datum
  print "<TR><TD>";
  print "$arr[0]" if ($arr[0]);
  printf "</TD><TD>%02d.%02d.19%2d %02d:%02d</TD>", $mday, $mon+1, $year, $hour, $min, $sec;

  # Je nach Art
  if ($arr[4] eq "I") {		# Incoming call
    print "<TD>";

    if ($arr[0]) {		# connection
      print "Eingehender Anruf";
      if ($arr[1]) {		# CLIP
        print " von ". &print_fqtn($arr[1]);
      }
    } else {			# no connection
      print "Vergeblicher Anruf";
      if ($arr[1]) {		# CLIP
        print " von ". &print_fqtn($arr[1]);
      }
    }
    print "</TD><TD></TD><TD></TD>";

  } elsif ($arr[4] eq "O") {	# Outgoing call
    print "<TD>";
    if ($arr[1] eq "") {
      print "???";
    } else {
      print &print_fqtn($arr[1]);
    }
    printf "</TD><TD>%d</TD><TD>%.2f %s</TD>", $arr[3], $arr[5], $arr[6];
    $counter++;
  } else {
    print "Falscher Typ $arr[4] für $arr[1]";
  }
  print "</TR>\n";
}


#
# sub print_fqtn()
#
# Converts number "+492364108537" in a HTMLized string
# -> <B>+49 2364 108537</B><BR>
#    WKN name <I>OKZ name<I> 
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
  $msg="<B>";
  if ($avon) { $msg.="$avon "; }
  if ($telno) { $msg.="$telno"; } else { $msg.="???"; }
  if ($rest) { $msg.="-$rest"; }
  $msg.="</B>";

  if (($avon_name) || ($wkn)) {
    $msg.="<BR>";
    if ($wkn) { $msg.="$wkn "; }
    if ($avon_name) { $msg.="<I>($avon_name)</I>"; }
  }
  return($msg);
}
