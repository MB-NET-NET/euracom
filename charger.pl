#!/usr/bin/perl -w

#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# charger.pl -- Gebührenauswertung via PostgreSQL
#
# Copyright (C) 1996-1997 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-09-02 11:03:41 GMT
# Version:             $Revision: 1.7 $
# Last modified:       $Date: 1997/10/04 16:51:00 $
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
# CmdLineParameters
# charger.pl 
#	-H host 
#	-D database
#	-v von
#	-b bis
#	-t Interne Nummer [,Interne Nummer ...]
#	-X Titel MSN
#	-d
#

use Pg;

require 'getopts.pl';
require 'tel-utils.pm';

#
# Parse CMD line params
#
$opt_H=$opt_D=$opt_v=$opt_b=$opt_X=$opt_t=$opt_b=$opt_d="";

&Getopts('dH:D:v:b:t:X:b:');

$db_host= $opt_H || "tardis";
$db_db  = $opt_D || "isdn";

$von    = $opt_v || "";
$bis    = $opt_b || "";
$MSN    = $opt_X || "";
$tln    = $opt_t || "";
$charge = $opt_b || 18.0;
$debugp = $opt_d || "";

$TMPNAME="/tmp/charger.$$";

#
# Fire up connection
#
debug("Opening connection...");
$db = Pg::connectdb("dbname=$db_db");
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
if ($von) { $filter_cmd[$filter_count++]="sys_date>='$von'"; }
if ($bis) { $filter_cmd[$filter_count++]="sys_date<='$bis'"; }
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
# Copy data to temporary file
#
&debug("Fetching data from db...");
open(TMPFILE, ">$TMPNAME") || die "Cannot create temporary file";
$db->exec("BEGIN") || die "BEGIN: $db->errorMessage";
$db->exec("DECLARE cx CURSOR FOR SELECT int_no,remote_no,vst_date,vst_time,sys_date,sys_time,einheiten,geb_art,factor,pay,currency FROM euracom $filter_cmd ORDER BY sys_date,sys_time") || die "DECLARE: $db->errorMessage";

do {
  $res=$db->exec("FETCH forward 1 IN cx") || die "FETCH: $db->errorMessage";

  if ($num=$res->ntuples) {
    for ($i=0; $i<$res->nfields; $i++) {
      $val=$res->getvalue(0,$i);
      print TMPFILE "$val;";
    }
    print TMPFILE "\n";
  };

} while ($num);

$db->exec("CLOSE cx") || die "CLOSE: $db->errorMessage\n";
$db->exec("END") || die "END: $db->errorMessage\n";
close(TMPFILE);
&debug("o.k.\n");

#
# Print bloody HTML header
#
print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n";
print "<html><head><title>Telefonrechnung</title></head><body>\n";
print "<h1>Telefonrechnung f&uuml;r Anschlu&szlig; $MSN</h1>\n";
print "<center><table border=\"5\" cellspacing=\"2\" cellpadding=\"2\">\n";
print "<tr><th>Anschlu&szlig;</th><th>Datum</th> <th>Rufnummer</th><th>Einheiten</th><th>Betrag</th></tr>\n";

#
# Do evaluation
#
$counter=0;
open(TMPFILE, "$TMPNAME") || die "Cannot open temporary file";
while (<TMPFILE>) {
  if (@data=split(";", $_)) { &htmlize_data(@data); }
}
close(TMPFILE);

#
# Print footer
#
$res=$db->exec("SELECT sum(einheiten), sum(pay) from euracom $filter_cmd");
$val1=$res->getvalue(0,0); $val2=$res->getvalue(0,1);
print "<tr><td></td><td></td><td>$counter Gespr&auml;che</td><td>$val1</td><td>$val2 DEM</td></tr>\n";
printf "<tr><td></td><td></td><td>Grundgeb&uuml;hr</td><td></td><td>%.2f DEM</td></tr>\n", $charge;
printf "<tr><td></td><td></td><td>GESAMT:</td><td></td><td><B>%.2f DEM</B></td></tr>\n", $val2+$charge;
print "</table></center><br><hr><address>";
print "Michael Bussmann, Im Brook 8, 45721 Haltern</address></body></html>\n";

#
# That's it
#
undef $db;
unlink($TMPNAME);


#
# sub htmlize_data()
#
# Input : fetchrow() array
# Output: Table items
#
sub htmlize_data()
{
  my (@arr) = @_;
  # int_no, remote_no, vst_date, vst_time, sys_date, sys_time, EH, geb, fac, pay, cur
  # 0       1          2         3         4         5         6   7    8    9    10

  # Interne Nummer und Datum
  print "<TR><TD>";
  print "$arr[0]" if ($arr[0]);
  $arr[4]=~tr/-/./;
  print "</TD><TD>$arr[4] ".substr($arr[5], 0, 5)."</TD>";

  # Je nach Art
  SWITCH: {
    if ($arr[7] eq "V") {
      print "<TD>";
      if ($arr[1] eq "") {
        print "Eingehender Anruf"
      } else {
        print "Eingehender Anruf von ", &print_fqtn($arr[1]);
      }
      print "</TD><TD></TD><TD></TD>";
      last SWITCH;
    }

    if ($arr[7] eq "G") {
     print "<TD>";
      if ($arr[1] eq "") {
        print "???";
      } else {
        print &print_fqtn($arr[1]);
      }
      printf "</TD><TD>%d</TD><TD>%.2f %s</TD>", $arr[6], $arr[9], $arr[10];
      $counter++;
      last SWITCH;
    }
     
    if ($arr[7] eq "K") {
      print "<TD>";
      if ($arr[1] eq "") {
        print "Vergeblicher Anruf";
      } else {
        print "Vergeblicher Anruf von ", &print_fqtn($arr[1]);
      }
      print "</TD><TD></TD><TD></TD>";
      last SWITCH;
    }

    print "Falscher Typ $arr[7] fuer $arr[1]";
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
