/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.h -- Various function prototypes and externals
 *
 * Copyright (C) 1996-2004 Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@mb-net.net>
 * Created:             1997-10-27 09:30:04 GMT
 * Version:             $LastChangedRevision$
 * Last modified:       $Date$
 * Keywords:            ISDN, Euracom, Ackermann, PostgreSQL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public Licence version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of merchanability
 * or fitness for a particular purpose.  See the GNU Public Licence for
 * more details.
 **************************************************************************/

#if !defined(_EURACOM_H)
#define _EURACOM_H

#include "termios.h"
struct SerialFile {
  int  fd;
  char *protocol_filename;
  char *fd_device;
  struct termios term;
  char *buffer;
};

enum TVerbindung { GEHEND=1, KOMMEND};
typedef char TelNo[33];

/* Aufbau Gebühreninfo */
struct GebuehrInfo {
  enum TVerbindung art;
  int    teilnehmer;    /* Interner Teilnehmer */
  TelNo  nummer;        /* Remote # */
  time_t datum_vst;     /* Datum/Zeit Verbindungsaufbau (von OVSt bzw. Euracom) */
  time_t datum_sys;     /* Datum/Zeit Eintrag (approx. Verbindungsende) */
  int    einheiten;     /* Anzahl verbrauchter Einheiten */
  int    length;	/* Duration of call (in s) */
  float  betrag;        /* Gesamtbetrag */
};

/* Database subsystem: postgres.c, msql.c */
extern void database_set_host(const char *str);
extern void database_set_port(const char *str);
extern void database_set_db(const char *str);
extern void database_set_shutdown_timeout(int i);
extern void database_set_recovery_timeout(int i);

extern BOOLEAN database_initialize(void);
extern BOOLEAN database_shutdown(void);

extern void database_check_state();
extern BOOLEAN database_log(const char *cp);
extern BOOLEAN database_geb_log(const struct GebuehrInfo *geb);

/* Serial subsystem */
extern struct SerialFile *serial_allocate_file(void);
extern void serial_deallocate_file(struct SerialFile *sf);
extern void serial_set_protocol_name(struct SerialFile *, const char *str);
extern void serial_set_device(struct SerialFile *, const char *str);
extern int serial_query_fd(const struct SerialFile *);
extern BOOLEAN serial_open_device(struct SerialFile *sf);
extern BOOLEAN serial_close_device(struct SerialFile *sf);
extern char *readln_rs232(struct SerialFile *sf);

#endif
