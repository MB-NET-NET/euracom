#!/usr/bin/perl -w

use Postgres;

$conn = db_connect("isdn","tardis","") || die "Open DB failed: $Postgres::error";

print "Connected Database: ", $conn->db();
print "\nConnected Host: ", $conn->host();
print "\nConnection Options: ", $conn->options();
print "\nConnected Port: ", $conn->port();
print "\nConnected tty: ", $conn->tty();
print "\nConnection Error Message: ", $conn->errorMessage();

print "\nConverting data...Please wait...\n";

open(GEBFILE, "gebuehr.dat") || die "Gebfile not found";
while (<GEBFILE>) {
  chop;
  @data = split(/;/);

  # Verbindungsart in $art
  if ($data[0]==1) { 
    $art="G";
  } elsif ($data[0]==2) {
    $art="K";
  } elsif ($data[0]==3) {
    $art="V";
  } else {
    $art="U";
  }

  # Zeit angabe 1 (vst_date, vst_time)
  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($data[1]);
  $mon++;
  $vst_date="$mday-$mon-$year";
  $vst_time="$hour:$min:$sec";

  # Zeit angabe 2 (sys_date, sys_time);
  ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($data[2]);
  $mon++;
  $sys_date="$mday-$mon-$year";
  $sys_time="$hour:$min:$sec";

  # Interne Nummer in $intern
  $intern=$data[3];
  if ($intern==0) {$intern="";}

  # Externe Nummer in $nummer
  $nummer=$data[4];
  if ($nummer eq "???") { 
    $nummer="";
  };

  # Add OKZ
  $nummer=~s/^([123456789])(\d+)/02364$1$2/;

  # Add int exit code
  $nummer=~s/^00(\d+)/+$1/;

  # Add intl code
  $nummer=~s/^0(\d+)/+49$1/;

  # Einheiten in $einheiten
  $einheiten=$data[5];

  # Payload
  $pay=$data[6];


# Codes now:
# PG: int_no   remote_no   vst_date/time  sys_date/time  einheiten  geb_art  factor  pay     currency
# ty: int4     varchar30   date/time      date/time      int4       char     float8  float8  char4
# va: $intern  $nummer     $vst_date/t    $sys_date/t    $einheiten $art     0.12    $pay    DEM

  $query=$conn->execute("INSERT into euracom (int_no, remote_no, vst_date, vst_time, sys_date, sys_time, einheiten, geb_art, factor, pay, currency) values ('$intern','$nummer','$vst_date','$vst_time','$sys_date','$sys_time','$einheiten','$art','0.12','$pay','DEM')");

  if (!$query) {
    print "Error inserting DB entry: $Postgres::error";
  }
}

close(GEBFILE);

