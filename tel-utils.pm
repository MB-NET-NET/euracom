#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# tel-utils.pm -- Perl utilities
#
# Copyright (C) 1996-1998 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-09-25 11:25:24 GMT
# Version:             $Revision: 1.11 $
# Last modified:       $Date: 1999/10/29 09:46:06 $
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
# $Id: tel-utils.pm,v 1.11 1999/10/29 09:46:06 bus Exp $
#

use Pg;

# I'll use "Pg" in "new-style mode"
# Actually I'm _not_ a friend of C++

#
# Uses $main::db as DB handle
#      $main::debugp as debug-mode var
#

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
  my ($num, $i, $j, $tuples);
  my (@datas);

  debug("SQLselect: Executing $cmd using callback $callback\n");
  $db->exec("BEGIN") || die "BEGIN failed";
  $db->exec("DECLARE cx CURSOR FOR $cmd") || die "DECLARE CURSOR failed";

  $num=0;
  do {
    debug("Fetching next 50...");
    $res=$db->exec("FETCH FORWARD 50 IN cx") || die "FETCH failed";
    $tuples=$res->ntuples;
    $i=0;
    debug("Got $tuples records\n");
    while ($i<$tuples) {
      for ($j=0; $j<$res->nfields; $j++) {
        $datas[$j]=$res->getvalue($i, $j);
      }
      &$callback(@datas);
      $i++;
    }
    $num+=$i;
  } until ($tuples!=50);

  $db->exec("CLOSE cx") || die "CLOSE failed";
  $db->exec("END") || die "END failed";
  debug("Total $num records retrieved\n");
  return($num);
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
  my (@data, $res);
  my ($num) = 0;
  my ($key, $value, $residual);

#
# Use external prefix_match function
#
  $cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";

#
# Use Oracle compat functions
#
#  $cmd="SELECT nummer,name FROM $table WHERE nummer=substr('$input', 1, length(nummer))";

  $res=$db->exec($cmd);
  if (!$res) {
    $msg=$db->errorMessage;
    warn "\n! $cmd: $msg !\n";
  }

  if (($num=$res->ntuples)>4) {
    warn "\n! More than 4 tuples for $input!\n";
    $num=4;
  }

  if ($num==0) {
    debug("no match!");
    return($input, 0, 0);

  } elsif ($num==1) {
    # 1 Match, easy to manage

    # Get field numbers
    $num_nummer=$res->fnumber("nummer");
    $num_name  =$res->fnumber("name");

    $key=$res->getvalue(0,$num_nummer); $value=$res->getvalue(0,$num_name);
    $residual=substr($input, length($key));
    debug("$key -> \"$value\" + \"$residual\"");

  } else {
    debug("($num tuples) ");

    # Get field numbers
    $num_nummer=$res->fnumber("nummer");
    $num_name  =$res->fnumber("name");

    # Get all tuples and store them in a temporary array
    for ($i=0; $i<$num; $i++) {
      push @data, [ ($res->getvalue($i, $num_nummer), $res->getvalue($i, $num_name)) ];
    }

    # Sort array by length of field 0 (nummer)
    @data = sort { length(${$a}[0]) <=> length(${$b}[0]) } @data;

    # Key is number with smallest length
    $key=$data[0][0];

    # Use best-match as value
    $value=$data[$num-1][1];

    # Print nice residual
    $residual=substr($input, length($key));
    if ($resi_best=substr($input, length($data[$num-1][0]))) {
      $residual=substr($residual,0, length($residual)-length($resi_best))."/".$resi_best;
    }
  }

  return($key, $value, $residual);
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

  debug("Converting $num:");

  # Try to locate number in cache
  if (exists $fqtn_cache{$num}) {
    debug(" --> found  in cache");
    ($avon, $telno, $rest, $avon_name, $wkn) = @{ $fqtn_cache{$num} };
  } else {

    # First look up AVON info
    debug("Querying database\n\tSearch avon: ");
    ($avon, $avon_name, $telno) = (&split_text("avon", $num, 1));
    if (!$avon_name) { $telno=$avon; $avon=""; }

    # Look up WKN info
    debug("\n\tWKN info: ");
    ($telno, $wkn, $rest) = (&split_text("wkn", $avon.$telno, length($avon)));
    $telno=substr $telno, length($avon);

    # Strip +49 from avon
    $avon=~s/^\+49/0/;

    # Add int. exit code
    $avon=~s/^\+/00/;

    # Insert in cache
    $fqtn_cache{$num}=[ ($avon, $telno, $rest, $avon_name, $wkn) ];
  }

  debug("\n\t\tAVON : $avon ($avon_name)\n\t\tTelNo: $telno ($wkn)\n\t\tPost : $rest\n");
  return($avon, $telno, $rest, $avon_name, $wkn);
}

#
# Prints debugging info if requested
#
sub debug()
{
  print STDERR @_ if ($main::debugp);
}

#
# Successful loading
#
1;
