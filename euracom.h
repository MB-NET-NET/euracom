/* Euracom.h
   $Id: euracom.h,v 1.4 1997/07/26 07:35:39 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/euracom.h,v $
*/

#if !defined(_EURACOM_H)
#define _EURACOM_H

/* Includes */
#include <time.h>

/* Maximale Länge einer Telefonnummer bzw. deren einz. Einheiten */
#define MAX_PHONE_LEN	 32	/* Gesamtlänge */
#define MAX_AVON_LEN	 10	/* Vorwahl */
#define MAX_REST_LEN	 32	/* Nachwahlziffern (hinter WKN) */
#define MAX_AVONNAME_LEN 40	/* Name des Ortsnetzes */
#define MAX_WKNAME_LEN	 40	/* WK Name */

enum TVerbindung { FEHLER, GEHEND, KOMMEND};

typedef char TelNo[MAX_PHONE_LEN+1];

/* Aufbau Gebühreninfo */
struct GebuehrInfo {
  int    teilnehmer;	/* Interner Teilnehmer */
  TelNo  nummer;	/* Remote # */
  time_t datum_vst;	/* Datum/Zeit Verbindungsaufbau (von VSt) */
  time_t datum_sys;	/* Datum/Zeit Eintrag (approx. Verbindungsende) */
  int    einheiten;	/* Anzahl verbrauchter Einheiten */
  enum TVerbindung art;
  float  betrag_base;	/* Betrag für eine EH */
  float  betrag;        /* Gesamtbetrag */
  char   waehrung[4];	/* Währungsbezeichnung */
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

#endif
