/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * serial.c -- Low/Midlevel RS232 routines
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-19 10:58:42 GMT
 * Version:             $Revision: 1.5 $
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

static char rcsid[] = "$Id: serial.c,v 1.5 1997/10/04 16:51:01 bus Exp $";

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "log.h"
#include "utils.h"
#include "fileio.h"

#include "euracom.h"

/* Private vars */
static char *protocol_filename = NULL;
static char *euracom_device = NULL;

static int euracom_fd;

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

void serial_set_protocol_name(const char *str)
{
  log_msg(ERR_JUNK, "serial: Setting protocol name to %s", str);
  if (protocol_filename) { free(protocol_filename); }
  protocol_filename=strdup(str);
}

void serial_set_device(const char *str)
{
  log_msg(ERR_JUNK, "serial: Euracom device is %s", str);
  if (euracom_device) { free(euracom_device); }
  euracom_device=strdup(str);
}

int serial_query_fd()
{
  return(euracom_fd);
}

BOOLEAN lockfile_check(BOOLEAN create)
{
  char lockpath[128] = LOCKPATH;
  FILE *lockfile;
  char *s;

  /* Construct filename */
  unless ((s=strrchr(euracom_device,'/'))) {
    log_msg(ERR_FATAL, "Invalid device name: Can't convert to lockpath");
    return(FALSE);
  }
  s++;
  strcat(lockpath, "/LCK.."); strcat(lockpath, s);

  if (!create) {
    return(delete_file(lockpath));
  } else {
    log_msg(ERR_JUNK, "Checking status of %s", lockpath);
    if ((lockfile=fopen(lockpath, "r"))) {
      char pidstr[15];

      fscanf(lockfile, "%11s", pidstr);
      log_msg(ERR_FATAL, "Device %s already locked by PID %s",
        s, pidstr);
      fclose(lockfile);
      return(FALSE);
    } else {
      log_msg(ERR_DEBUG, "Locking device %s by lockfile %s", s, lockpath);
      unless (lockfile=fopen(lockpath, "w")) {
        log_msg(ERR_FATAL, "Writing logfile failed: %s", strerror(errno));
        return(FALSE);
      }
      fprintf(lockfile, "%11d", getpid());
      fclose(lockfile);
    }
  }
  return(TRUE);
}


/*------------------------------------------------------*/
/* int init_euracom_port()                              */
/* */
/* Öffnet RS232 device, setzt RTS                       */
/* */
/* RetCode: fd, -1: Error                               */
/*------------------------------------------------------*/
BOOLEAN serial_initialize()
{
  struct termios term;
  int flags;

  log_msg(ERR_INFO, "Initializing Euracom RS232 port...");

  unless (euracom_device) {
    log_msg(ERR_FATAL, "No device given!");
    return(FALSE);
  }

  /* Create lockfile */
  unless (lockfile_check(TRUE)) {
    log_msg(ERR_FATAL, "Couldn't lock device %s", euracom_device);
    return(FALSE);
  }

  /* Open euracom port */
  if ((euracom_fd=open(euracom_device, O_RDWR))<0) {
    log_msg(ERR_CRIT, "Error opening %s: %s", euracom_device, strerror(errno));
    return(FALSE);
  }

  fcntl(euracom_fd, F_SETFL, O_RDONLY);

  /* Setup TTY (9600,N,8,1) */
  if (TTY_GETATTR(euracom_fd, &term) == -1 ) {
    log_msg(ERR_CRIT, "tcgetattr: %s", strerror(errno));
    return(-1);
  }

  /* Put line in transparent state */
  cfmakeraw(&term);
  /* Sets: iflag: !IGNBRK, !BRKINT, !PARMRK, !ISTRIP, !INLCR, !IGNCR, !ICRNL, !IXON
           oflag: !OPOST
	   lflag: !ECHO, !ECHONL, !ICANON, !ISIG, !IEXTEN
	   cflag: !CSIZE, !PARENB, CS8
  */
  term.c_iflag|=IGNCR; /* So this is utter nonsense?! */

  /* Set in/out speed to 9600 baud */
  cfsetispeed(&term, B9600);
  cfsetospeed(&term, B9600);

  if (TTY_SETATTR(euracom_fd, &term)==-1) {
    log_msg(ERR_CRIT, "tcsetattr: %s", strerror(errno));
    return(FALSE);
  }

  /* Give her a bit time to react upon RTS */
  sleep(1);

  ioctl(euracom_fd, TIOCMGET, &flags);
  if (!(flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS not set");
    serial_shutdown();
    return(FALSE);
  }
  if (!(flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "Euracom did not raise CTS. Check connection");
    serial_shutdown();
    return(FALSE);
  }

  return(TRUE);
}


/*------------------------------------------------------*/
/* int close_euracom_port()                             */
/* */
/* Schließt RS232 port, setzt RTS zurück                */
/* */
/* RetCode: 0: O.k, -1: Error                           */
/*------------------------------------------------------*/
int serial_shutdown()
{
  int flags = 0;

  log_msg(ERR_INFO, "Shutting down Euracom RS232 port");

  lockfile_check(FALSE);

  /* Clear all TTY flags */
  ioctl(euracom_fd, TIOCMSET, &flags);

  /* Wait a sec */
  sleep(1);

  /* Check for successful termination */
  ioctl(euracom_fd, TIOCMGET, &flags);

  /* Anyway, I won't care */
  close(euracom_fd);

  /* Free memory */
  if (euracom_device) { free(euracom_device); }
  if (protocol_filename) { free(protocol_filename); }

  /* Feedback to user */
  if ((flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS still set");
    return(FALSE);
  }
  if ((flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "CTS still set");
    return(FALSE);
  }

  return(TRUE);
}


/*------------------------------------------------------*/
/* char *readln_rs232()                                 */
/* */
/* Daten liegen an fd an. Lesen bis 0A 0D 00 oder timeo */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
char *readln_rs232()
{
  static char buf[1024];
  char *cp=buf;
  int inbuf[2];

  do {
    if (read(euracom_fd, &inbuf[0], 1)!=-1) {
      *cp=inbuf[0];
      if (*cp=='\0') {
	*(cp-1)='\0';
	break;
      } else {
	cp++;
      }
    } else {
      log_msg(ERR_CRIT, "RS232 read I/O error: %s", strerror(errno));
      return(NULL);
    }
  } while (1);

  /* Make a copy of line if requested */
  if (protocol_filename) {
    FILE *fp = fopen(protocol_filename, "a");

    if (fp) {
      fputline(fp, buf);
      fclose(fp);
    }
  }
  return(buf);
}
