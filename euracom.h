/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.h -- Various function prototypes and externals
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-04 17:30:00 GMT
 * Version:             $Revision: 1.8 $
 * Last modified:       $Date: 1997/10/04 16:51:01 $
 * Keywords:            ISDN, Euracom, Ackermann, PostgreSQL
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

/* Database subsystem: postgres.c */
extern void database_set_host(const char *str);
extern void database_set_port(const char *str);
extern void database_set_db(const char *str);
extern void database_set_shutdown_timeout(int i);
extern void database_set_recovery_timeout(int i);

extern BOOLEAN database_initialize(void);
extern BOOLEAN database_shutdown(void);

extern void database_check_state();
extern BOOLEAN database_log(const char *cp);

/* Serial subsystem: serial.c */
extern void serial_set_protocol_name(const char *str);
extern void serial_set_device(const char *str);
extern int serial_query_fd();

extern BOOLEAN serial_initialize();
extern int serial_shutdown();

extern char *readln_rs232();

