use Postgres;

#
# tel-utils.pm - Some useful routines for searching in PG database
#
# $Id: tel-utils.pm,v 1.1 1997/09/25 11:25:24 bus Exp $
#

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

  $cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";
  $res=$db->execute($cmd) || warn "$cmd: $Postgres::error";
  if (@data=$res->fetchrow()) {
    debug($data[0]." -> ".$data[1]);
    return($data[0], $data[1], substr($input, length($data[0])));
  }
  return($input, 0, 0);
}

#
# (avon, telno, postfix, avon_name, wkn_name) convert_fqtn(num)
#
# Converts a int. telephone number into a FQTN structure
#
sub convert_fqtn()
{
  my ($num) = @_;
  my ($key, $value, $rest);
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
  if ($debugp) {
    print @_;
  }
}

#
# Successful loading
#
1;
