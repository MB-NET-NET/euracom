/* Euracom.h
   $Id: euracom.h,v 1.1 1996/10/27 09:30:04 bus Exp $
   $File$
*/

enum TVerbindung { FEHLER=0, GEHEND, KOMMEND, VERBINDUNG};

typedef char TelNo[32];

struct GebuehrInfo {
  enum TVerbindung art;
  int teilnehmer;
  time_t datum;	/* Datum/Zeit Verbindungsaufbau */
  time_t doe;	/* Datum/Zeit Eintrag (approx. Verbindungsende) */
  TelNo nummer;
  int einheiten;
  float betrag;
};
