/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.h -- Main programme include file
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-27 09:30:04 GMT
 * Version:             $Revision: 1.6 $
 * Last modified:       $Date: 1997/09/26 10:06:07 $
 * Keywords:            ISDN, Euracom, Ackermann
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public Licence as published by the
 * Free Software Foundation; either version 2 of the licence, or (at your
 * opinion) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of merchanability
 * or fitness for a particular purpose.  See the GNU Public Licence for
 * more details.
 **************************************************************************/

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
