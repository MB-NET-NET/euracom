#!/usr/bin/perl -w

use Postgres;

require 'getopts.pl';
require 'tel-utils.pm';

#
# Telno -> FQTN converter
#
# $Id: avon.pl,v 1.4 1997/09/26 10:06:05 bus Exp $
#

#
# CmdLineParameters
# charger.pl 
#	-H host 
#	-D database
#

$| = 1;

#
# Parse CMD line params
#
&Getopts('H:D:d');
$db_host= $getopts::opt_H || "tardis";
$db_db  = $getopts::opt_D || "isdn";
$debugp = $getopts::opt_d || undef;

#
# Fire up connection
#
debug("Opening connection...");
$db = db_connect($db_db,$db_host,"") || die "Open DB failed: $Postgres::error";
debug("o.k.\n");

while (<>) {
  chop;

  printf "\t%s\n", print_fqtn($_);
}

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
