#!/usr/bin/perl -w

use Postgres;

require 'getopts.pl';

#
# Telno -> FQTN converter
#
# $Id: avon.pl,v 1.2 1997/09/02 11:03:41 bus Exp $
#

#
# CmdLineParameters
# charger.pl 
#	-H host 
#	-D database
#

#
# Parse CMD line params
#
$opt_H=$opt_D="";

&Getopts('H:D:');
$db_host= $opt_H || "tardis";
$db_db  = $opt_D || "isdn";

#
# Fire up connection
#
$db = db_connect($db_db,$db_host,"") || die "Open DB failed: $Postgres::error";

while (<>) {
  chop;

  printf "\t%s\n", convert_fqtn($_);
}

#
# sub convert_fqtn()
#
sub convert_fqtn()
{
  my ($num) = @_;
  my ($key, $value, $rest);
  my ($avon, $avon_name, $telno, $tel, $wkn, $rest);
  my ($msg);

  # First look up AVON info
  ($avon, $avon_name, $telno) = (&split_text("avon", $num, 1));
  if (!$avon_name) { $telno=$avon; $avon=""; }

  # Look up WKN info
  ($telno, $wkn, $rest) = (&split_text("wkn", $avon.$telno, length($avon)));
  $telno=substr $telno, length($avon);

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

sub split_text()
{
  my ($table, $input, $residual) = @_;
  my (@data, $res);
  my ($in) = $input;
  my ($rest) = "";

  while (length($in)>$residual) {
    $cmd="SELECT name FROM $table WHERE nummer='$in'";
    $res=$db->execute($cmd) || warn "$cmd: $Postgres::error";
    if (@data=$res->fetchrow()) {
      return($in, $data[0], $rest);
      last;
    }
    $rest=chop($in) . $rest;
  }
  return($input, 0, 0);
}
