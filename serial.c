/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * serial.c -- Lowlevel RS232 routines
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-19 10:58:42 GMT
 * Version:             $Revision: 1.4 $
 * Last modified:       $Date: 1997/09/26 10:06:07 $
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

static char rcsid[] = "$Id: serial.c,v 1.4 1997/09/26 10:06:07 bus Exp $";

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <sys/errno.h>
#include "log.h"
#include "utils.h"
#include "fileio.h"


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

/* Defines */
#define LOCKPATH		"/var/lock"

/* Globals */
static char lock[80] = LOCKPATH;


/*------------------------------------------------------*/
/* BOOLEAN do_locking()                                 */
/* */
/* Checkt lockfile, wenn nicht vorhanden, dev locken    */
/* */
/* RetCode: FALSE: Error, TRUE: O.k                     */
/*------------------------------------------------------*/
BOOLEAN do_locking(const char *device)
{
  FILE *lockfile;
  char *s = strrchr(device, '/');

  if (!s) {
    log_msg(ERR_FATAL, "do_locking(): Junkdevice");
    return(FALSE);
  }
  s++;
  strcat(lock, "/LCK..");
  strcat(lock, s);
  log_msg(ERR_JUNK, "Checking status of %s", lock);

  if ((lockfile=fopen(lock, "r"))==NULL) {
    log_msg(ERR_DEBUG, "Locking device %s by lockfile %s",
      s, lock);
    lockfile=fopen(lock, "w");
    fprintf(lockfile, "%11d", getpid());
    fclose(lockfile);
    return(TRUE);
  } else {
    char pidstr[15];

    fscanf(lockfile, "%11s", pidstr);
    log_msg(ERR_FATAL, "Device %s already locked by PID %s",
	    s, pidstr);
    fclose(lockfile);
    return(FALSE);
  }
}

/*------------------------------------------------------*/
/* int init_euracom_port()                              */
/* */
/* Öffnet RS232 device, setzt RTS                       */
/* */
/* RetCode: fd, -1: Error                               */
/*------------------------------------------------------*/
int init_euracom_port(const char *device)
{
  struct termios term;
  int fd;
  int flags;

  /* Anyone else using this device? */
  if (!do_locking(device)) {
    return(-1);
  }

  log_msg(ERR_DEBUG, "Initializing Euracom RS232 port (%s)", device);

  if ((fd=open(device, O_RDWR))<0) {
    log_msg(ERR_CRIT, "Error opening %s: %s", device, strerror(errno));
    return(-1);
  }

  fcntl(fd, F_SETFL, O_RDONLY);

  /* Setup TTY (9600,N,8,1) */
  if (TTY_GETATTR(fd, &term) == -1 ) {
    log_msg(ERR_CRIT, "tcgetattr: %s", strerror(errno));
    return(-1);
  }

  cfmakeraw(&term);
  term.c_iflag|=IGNCR;
  cfsetispeed(&term, B9600);
  cfsetospeed(&term, B9600);

  if (TTY_SETATTR(fd, &term)==-1) {
    log_msg(ERR_CRIT, "tcsetattr: %s", strerror(errno));
    return(-1);
  }

  /* Give her a bit time to react upon RTS */
  sleep(1);

  ioctl(fd, TIOCMGET, &flags);
  if (!(flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS not set");
    close_euracom_port(fd);
    return(-1);
  }
  if (!(flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "Euracom did not raise CTS. Check connection");
    close_euracom_port(fd);
    return(-1);
  }

  return(fd);
}


/*------------------------------------------------------*/
/* int close_euracom_port()                             */
/* */
/* Schließt RS232 port, setzt RTS zurück                */
/* */
/* RetCode: 0: O.k, -1: Error                           */
/*------------------------------------------------------*/
int close_euracom_port(fd)
{
  int flags = 0;

  log_msg(ERR_DEBUG, "Shutting down Euracom RS232 port");

  /* Remove lockfile */
  log_msg(ERR_DEBUG, "Removing lockfile %s", lock);
  if (unlink(lock)) {
    log_msg(ERR_CRIT, "Could not remove lockfile %s: %s",
	    lock, strerror(errno));
  }

  /* Clear all TTY flags */
  ioctl(fd, TIOCMSET, &flags);

  /* Wait a sec */
  sleep(1);

  /* Check for successful termination */
  ioctl(fd, TIOCMGET, &flags);

  /* Anyway, I won't care */
  close(fd);

  /* Feedback to user */
  if ((flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS still set");
    return(-1);
  }
  if ((flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "CTS still set");
    return(-1);
  }

  return(0);
}


/*------------------------------------------------------*/
/* char *readln_rs232()                                 */
/* */
/* Daten liegen an fd an. Lesen bis 0A 0D 00 oder timeo */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
char *readln_rs232(int fd)
{
  static char buf[1024];
  char *cp=buf;
  int inbuf[2];
  size_t n, i;

  do {
    if (read(fd, &inbuf[0], 1)!=-1) {
      *cp=inbuf[0];
      if (*cp=='\0') {
	*(cp-1)='\0';
	return(buf);
      } else {
	cp++;
      }
    } else {
      log_msg(ERR_CRIT, "RS232 read I/O error: %s", strerror(errno));
      return(NULL);
    }
  } while (1);
}
