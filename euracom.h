/* Euracom.h
   $Id: euracom.h,v 1.2 1996/11/02 15:20:21 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/euracom.h,v $
*/

#if !defined(_EURACOM_H)
#define _EURACOM_H

/* Includes */
#include <time.h>

/* Maximale Länge einer Telefonnummer bzw. deren einz. Einheiten */
#define MAX_PHONE_LEN	32	/* Gesamtlänge */
#define MAX_AVON_LEN	10	/* Vorwahl */
#define MAX_REST_LEN	32	/* Nachwahlziffern (hinter WKN) */
#define MAX_AVONNAME_LEN 40	/* Name des Ortsnetzes */
#define MAX_WKNAME_LEN	40	/* WK Name */

enum TVerbindung { FEHLER=0, GEHEND, KOMMEND, VERBINDUNG};

typedef char TelNo[MAX_PHONE_LEN+1];

/* Aufbau Gebühreninfo */
struct GebuehrInfo {
  enum TVerbindung art;
  int teilnehmer;
  time_t datum;		/* Datum/Zeit Verbindungsaufbau */
  time_t doe;		/* Datum/Zeit Eintrag (approx. Verbindungsende) */
  TelNo nummer;
  int einheiten;
  float betrag;
};

/* Fully Qualified Telephone Number */
struct FQTN {
  char avon[MAX_AVON_LEN+1];
  char telno[MAX_PHONE_LEN+1];
  char rest[MAX_REST_LEN+1];
  char avon_name[MAX_AVONNAME_LEN+1];
  char wkn[MAX_WKNAME_LEN+1];
};

/* Default database names */
#define AVON_DB_NAME	"/var/lib/euracom/avon.gdbm"
#define WKN_DB_NAME	"/var/lib/euracom/wkn.gdbm"

#define AVON_TXT_NAME	"/etc/isdnlog/avon"
#define WKN_TXT_NAME	"/var/lib/euracom/wkn.dat"

#endif
