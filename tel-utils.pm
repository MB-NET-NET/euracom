#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# tel-utils.pm -- Perl utilities
#
# Copyright (C) 1996-1998 by Michael Bussmann
#
# Authors:             Michael Bussmann <bus@fgan.de>
# Created:             1997-09-25 11:25:24 GMT
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
# $Id: tel-utils.pm,v 1.12 1999/12/27 18:23:48 bus Exp $
#

use DBI;

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
  my ($num, $i, $j, $tuples);
  my (@data);

  debug("SQLselect: Executing $cmd using callback $callback\n");
  prepexec("BEGIN");
  $sth=prepexec("DECLARE cx CURSOR FOR $cmd");

  $num=0;
  do {
    debug("Fetching next 50...");
    $sth=prepexec("FETCH FORWARD 50 IN cx");
    $tuples=$sth->{NUM_OF_FIELDS};
    $i=0;
    debug("Got $tuples records\n");
    while (@data=$sth->fetchrow_array) {
      &$callback(@data);
      $i++;
    }
    $num+=$i;
  } until ($tuples!=50);

  prepexec("CLOSE cx");
  prepexec("END");
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
  my (@data, @row);
  my ($num);
  my ($key, $value, $residual, $resi_best);

#
# Use external prefix_match function
#
  $cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";

#
# Use Oracle compat functions
#
#  $cmd="SELECT nummer,name FROM $table WHERE nummer=substr('$input', 1, length(nummer))";

  $sth=prepexec($cmd) || warn "$cmd failed: $DBI::errstr";
  $num=0;
  while (@row = $sth->fetchrow_array) {
    $num++;
    push @data, [ (@row) ];
  }
  die $sth->errstr if $sth->err;

  $sth->finish;
  return($input, 0,0 ) if (@data==0);
  return($data[0][0], $data[0][1], substr($input, length($data[0][0]))) if (@data==1);

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
# (sth) prepexec(statement)
#
# Prepares and executes SQL statement
#
sub prepexec()
{
  my ($statement) = @_;
  my ($rc);

  $sth=$main::dbh->prepare($statement) || warn "Prepare $statement failed: $DBI::errstr";
  $rc=$sth->execute || warn "Execute $statement failed: $DBI::errstr";
  return $sth;
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
