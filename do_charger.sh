#! /bin/sh

#
# Telefongebührenauswertung
# 108537, 13155, 4275, 4095
# $Id: do_charger.sh,v 1.5 1997/08/09 15:01:06 bus Exp $
#

EXEFILE="/var/lib/euracom/charger"
GEBFILE="/var/lib/euracom/gebuehr.dat"
LASTFILE="/var/lib/euracom/last.checked"
MAILTO="bus@goliath"
ISDNREP="/usr/bin/isdnrep"

ALL_MSN="0 1 2"

MSN_0="108537"
APP_0="11,41,42"

MSN_1="13155"
APP_1="12,21,22,23"

MSN_2="4275"
APP_2="31"

###############################################################
#
# End of user serviceable parts
#

#
# Check timestamp
#
TIME_FROM=0
if [ -f $LASTFILE ] ; then
	TIME_FROM=`cat $LASTFILE`
fi

# Update timestamp file
getdate "now" >$LASTFILE

for ndx in $ALL_MSN; do
	eval THIS_MSN=\$MSN_$ndx
	eval THIS_APP=\$APP_$ndx

	ERRFILE=/tmp/euracom.$$.err
	OUTFILE=/tmp/euracom.$THIS_MSN.$$.out

	$EXEFILE -g $GEBFILE -t "$THIS_APP" -v$TIME_FROM 2>>$ERRFILE >$OUTFILE

# Mail results to admin
	mail -s"Gebührenauswertung +49 2364 $THIS_MSN" $MAILTO <$OUTFILE

# Clean up
	/bin/rm -f $OUTFILE
	/bin/rm -f $ERRFILE
done

# Perform S0 analysis
$ISDNREP -av | mail -s"ISDN S0 Analyse" $MAILTO
$ISDNREP -d -
