#***************************************************************************
# euracom -- Euracom 18x Gebührenerfassung
#
# tel-utils.pm -- Perl utilities
#
# Copyright (C) 1996-2004 Michael Bussmann
#
# Authors:             Michael Bussmann <bus@mb-net.net>
# Created:             1997-09-25 11:25:24 GMT
# Version:             $Revision: 1.16 $
# Last modified:       $Date: 2002/05/10 07:04:29 $
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
# $Id: tel-utils.pm,v 1.16 2002/05/10 07:04:29 bus Exp $
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
  my ($i);
  my (@data);

  debug("SQLselect: Executing $cmd using callback $callback\n");
  $sth=prepexec("$cmd") || die "$cmd: $DBI::errstr";

  $i=0;
  while (@data=$sth->fetchrow_array) {
    &$callback(@data);
    $i++;
  }
  die $sth->errstr if $sth->err;
  $sth->finish;

  debug("Total $i records retrieved\n");
  return($i);
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
  my ($residual, $resi_best);
  my ($sth);

#
# Use external prefix_match function
#
  $cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";

#
# Use Oracle compat functions
#
#  $cmd="SELECT nummer,name FROM $table WHERE nummer=substr('$input', 1, length(nummer))";

  $sth=prepexec($cmd) || warn "$cmd failed: $DBI::errstr";
  while (@row = $sth->fetchrow_array) {
    push @data, [ (@row) ];
  }
  die $sth->errstr if $sth->err;
  $sth->finish;

  # Shortcuts
  return($input, 0, 0 ) if (@data==0);
  return($data[0][0], $data[0][1], substr($input, length($data[0][0]))) if (@data==1);

  # Sort array by length of field 0 (nummer)
  @data = sort { length(${$a}[0]) <=> length(${$b}[0]) } @data;

  # Print nice residual
  $residual=substr($input, length($data[0][0]));
  if ($resi_best=substr($input, length($data[-1][0]))) {
    $residual=substr($residual,0, length($residual)-length($resi_best))."/".$resi_best;
  }

  return($data[0][0], $data[-1][1], $residual);
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
    debug("$avon; $avon_name; $telno");
    if (!$avon_name) { $telno=$avon; $avon=""; }

    # Look up WKN info
    debug("\n\tWKN info: ");
    ($telno, $wkn, $rest) = (&split_text("wkn", $avon.$telno, length($avon)));
    debug("$telno; $wkn; $rest");
    $telno=substr $telno, length($avon);

    # Strip +49 from avon
    $avon=~s/^\+49/0/;

    # Add int. exit code
    $avon=~s/^\+/00/;

    # Insert in cache
    $fqtn_cache{$num}=[ ($avon, $telno, $rest, $avon_name, $wkn) ];
  }

  debug("\n\t\tAVON : $avon ($avon_name)\n\t\tTelNo: $telno ($wkn)\n\t\tPost : $rest\n");
  %tel=(
	avon => "$avon",
	avon_name => "$avon_name",
	prov => "01070",
	prov_name => "Dirty Provider",
	telno => "$telno",
	wkn => "$wkn",
	rest => "$rest"
  );
  return(%tel);
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
