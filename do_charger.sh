#! /bin/sh

#
# Telefongebührenauswertung
# 108537, 13155, 4275, 4095
# $Id: do_charger.sh,v 1.1 1996/10/27 12:38:35 bus Exp $
#

EXEFILE="/var/lib/euracom/charger"
GEBFILE="/var/lib/euracom/gebuehr.dat"
LASTFILE="/var/lib/euracom/last.checked"
MAILTO="bus@goliath"

ALL_MSN="0 1 2"

MSN_0="108537"
APP_0="-t 11"

MSN_1="13155"
APP_1="-t 12 -t 21 -t 22 -t 23"

MSN_2="4275"
APP_2="-t 31"

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

	$EXEFILE -g $GEBFILE $THIS_APP -v$TIME_FROM 2>>$ERRFILE >$OUTFILE

# Mail results to admin
	mail -s\"Gebührenauswertung +49 2364 $THIS_MSN\" $MAILTO <$OUTFILE

# Clean up
	/bin/rm -f $OUTFILE
	/bin/rm -f $ERRFILE
done
