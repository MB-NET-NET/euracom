/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.h -- Various function prototypes and externals
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-27 09:30:04 GMT
 * Version:             $Revision: 1.10 $
 * Last modified:       $Date: 1998/02/04 09:22:56 $
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

#if !defined(_EURACOM_H)
#define _EURACOM_H

/* Use local configuration */
#include "config.h"

/*
 * Use my weird macros
 */
#define elsif else if
#define unless(s) if (!(s))

/*
 * select which terminal handling to use (currently only SysV variants)
 */
#if defined(HAVE_TERMIOS)
#include <termios.h>
#define TTY_GETATTR(_FD_, _ARG_) tcgetattr((_FD_), (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) tcsetattr((_FD_), TCSANOW, (_ARG_))
#endif

#if defined(HAVE_TERMIO)
#include <termio.h>
#include <sys/ioctl.h>
#define TTY_GETATTR(_FD_, _ARG_) ioctl((_FD_), TCGETA, (_ARG_))
#define TTY_SETATTR(_FD_, _ARG_) ioctl((_FD_), TCSETAW, (_ARG_))
#endif

#ifndef TTY_GETATTR
#error MUST DEFINE ONE OF "HAVE_TERMIOS" or "HAVE_TERMIO"
#endif

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
struct SerialFile {
  int  fd;
  char *protocol_filename;
  char *fd_device;
  struct termios term;
};

extern struct SerialFile *serial_allocate_file(void);
extern void serial_deallocate_file(struct SerialFile *sf);
extern void serial_set_protocol_name(struct SerialFile *, const char *str);
extern void serial_set_device(struct SerialFile *, const char *str);
extern int serial_query_fd(const struct SerialFile *);
extern BOOLEAN serial_open_device(struct SerialFile *sf);
extern BOOLEAN serial_close_device(struct SerialFile *sf);
extern char *readln_rs232(struct SerialFile *sf);

#endif
