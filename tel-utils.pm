use Postgres;

#
# tel-utils.pm - Some useful routines for searching in PG database
#
# $Id: tel-utils.pm,v 1.2 1997/09/26 10:06:08 bus Exp $
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
  my ($num) = 0;
  my ($key, $value, $residual);

  $cmd="SELECT nummer,name FROM $table WHERE prefix_match('$input'::text, nummer)";

  $res=$db->execute($cmd) || warn "$cmd: $Postgres::error";
  if (($num=$res->ntuples())>4) {
    warn "More than 4 tuples for $input";
    $num=4;
  }

  if ($num==0) {
    debug("no match!");
    return($input, 0, 0);

  } elsif ($num==1) {
    # 1 Match, easy to manage
    ($key, $value) = $res->fetchrow();
    $residual=substr($input, length($key));
    debug("$key -> $value + $residual ");

  } else {
    debug("($num tuples) ");

    # Get all tuples and store them in a temporary array
    for ($i=0; $i<$num; $i++) {
      push @data, [ $res->fetchrow() ];
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
