/* Euracom.h
   $Id: euracom.h,v 1.5 1997/08/28 09:30:44 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/euracom.h,v $
*/

#if !defined(_EURACOM_H)
#define _EURACOM_H

#define elsif else if
#define unless(s) if (!(s))

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

#endif
