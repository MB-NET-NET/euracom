#!/usr/bin/perl -w

use Pg;

require 'getopts.pl';
require 'tel-utils.pm';

#
# Telno -> FQTN converter
#
# $Id: avon.pl,v 1.6 1997/10/05 09:16:59 bus Exp $
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
$opt_H=$opt_D=$opt_d=$main::debugp=undef;
&Getopts('H:D:d');
$main::db_host= $opt_H || "tardis";
$main::db_db  = $opt_D || "isdn";
$main::debugp = $opt_d || "";

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
