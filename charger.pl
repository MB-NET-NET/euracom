#!/usr/bin/perl -w

use Postgres;

require 'getopts.pl';

#
# Telefon Gebuehrenauswertung
#
# $Id: charger.pl,v 1.2 1997/09/02 13:08:23 bus Exp $
#

#
# CmdLineParameters
# charger.pl 
#	-H host 
#	-D database
#	-v von
#	-b bis
#	-t Interne Nummer
#	-X Titel MSN
#	-d
#

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
&debug("Opening connection to database...");
$db = db_connect($db_db,$db_host,"") || die "Open DB failed: $Postgres::error";
&debug("o.k.\n");

#
# Create filter expression and build internal list
#
&debug("Creating filter expression...");
$filter_count=0;
if ($von) { $filter_cmd[$filter_count++]="vst_date>='$von'"; }
if ($bis) { $filter_cmd[$filter_count++]="vst_date<='$bis'"; }
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
$db->execute("BEGIN") || die "BEGIN: $Postgres::error";
$db->execute("DECLARE cx CURSOR FOR SELECT int_no,remote_no,vst_date,vst_time,sys_date,sys_time,einheiten,geb_art,factor,pay,currency FROM euracom $filter_cmd ORDER BY vst_date,vst_time") || die "DECLARE: $Postgres::error";
do {
  $res=$db->execute("FETCH forward 1 IN cx") || die "FETCH: $Postgres::error";
  if (@data=$res->fetchrow()) {
    print TMPFILE join(";", @data)."\n";
  }
} while (@data);
$db->execute("END") || die "END: $Postgres::error";
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
  @data=split(";", $_);
  &htmlize_data(@data) if (@data);
}
close(TMPFILE);

#
# Print footer
#
$res=$db->execute("SELECT sum(einheiten), sum(pay) from euracom $filter_cmd");
@data=$res->fetchrow();
print "<tr><td></td><td></td><td>$counter Gespr&auml;che</td><td>$data[0]</td><td>$data[1] DEM</td></tr>\n";
printf "<tr><td></td><td></td><td>Grundgeb&uuml;hr</td><td></td><td>%.2f DEM</td></tr>\n", $charge;
printf "<tr><td></td><td></td><td>GESAMT:</td><td></td><td><B>%.2f DEM</B></td></tr>\n", $data[1]+$charge;
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
  $arr[2]=~tr/-/./;
  print "</TD><TD>$arr[2] ".substr($arr[3], 0, 5)."</TD>";

  # Je nach Art
  SWITCH: {
    if ($arr[7] eq "V") {
      print "<TD>";
      if ($arr[1] eq "") {
        print "Eingehender Anruf"
      } else {
        print "Eingehender Anruf von ", &convert_fqtn($arr[1]);
      }
      print "</TD><TD></TD><TD></TD>";
      last SWITCH;
    }

    if ($arr[7] eq "G") {
     print "<TD>";
      if ($arr[1] eq "") {
        print "???";
      } else {
        print &convert_fqtn($arr[1]);
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
        print "Vergeblicher Anruf von ", &convert_fqtn($arr[1]);
      }
      print "</TD><TD></TD><TD></TD>";
      last SWITCH;
    }

    print "Falscher Typ $arr[7] fuer $arr[1]";
  }

  print "</TR>\n";
}


#
# sub convert_fqtn()
#
# Converts number "+492364108537" in a HTMLized string
# -> <B>+49 2364 108537</B><BR>
#    WKN name <I>OKZ name<I> 
#
sub convert_fqtn()
{
  my ($num) = @_;
  my ($avon, $avon_name, $telno, $wkn, $rest);
  my ($msg);

  # Look up WKN info
  ($telno, $wkn, $rest) = (&split_text("wkn", $num));

  # Look up AVON info
  ($avon, $avon_name, $telno) = (&split_text("avon", $telno));

  # Strip +49 from avon
  $avon=~s/^\+49/0/;

  # Construct HTML
  $msg="<B>$avon $telno</B>";
  if (($avon_name) || ($wkn)) {
    $msg.="<BR>$wkn <I>($avon_name)</I>";
  }
  return($msg);
}

sub split_text()
{
  my ($table, $input) = @_;
  my (@data, $res);
  my ($in) = $input;
  my ($rest) = "";

  while (length($in)>1) {
    $res=$db->execute("SELECT name FROM $table WHERE nummer='$in'") || die "SELECT: $Postgres::error";
    if (@data=$res->fetchrow()) {
      return($in, $data[0],$rest);
      last;
    }
    $rest=chop($in) . $rest;
  }
  return($input, "", "");
}

sub debug()
{
  if ($debugp) {
    print @_;
  }
}
