/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * serial.c -- Low/Midlevel RS232 routines
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-19 10:58:42 GMT
 * Version:             $Revision: 1.24 $
 * Last modified:       $Date: 1999/05/28 08:29:50 $
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

static char rcsid[] = "$Id: serial.c,v 1.24 1999/05/28 08:29:50 bus Exp $";

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

#include "euracom.h"

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


/*--------------------------------------------------------------------------
 * struct SerialFile *serial_allocate_file()
 *
 * Allocates serial file structure
 *
 * Inputs: -
 * RetCode: Ptr to freshly created structure
 *------------------------------------------------------------------------*/
struct SerialFile *serial_allocate_file()
{
  struct SerialFile *sf;

  sf=safe_malloc(sizeof(struct SerialFile));
  sf->fd=0;
  sf->protocol_filename=sf->fd_device=NULL;
  sf->buffer=(char *)malloc(1024);

  debug(3, "serial: Allocation request");
  return(sf);
}

/*--------------------------------------------------------------------------
 * void serial_deallocate_file()
 *
 * Deallocates serial file structure
 *
 * Inputs: SerialFile
 * RetCode: -
 *------------------------------------------------------------------------*/
void serial_deallocate_file(struct SerialFile *sf)
{
  if (sf) { 
    safe_free(sf->protocol_filename);
    safe_free(sf->fd_device);
    safe_free(sf->buffer);
    safe_free(sf);
  }
}

/*--------------------------------------------------------------------------
 * void serial_set_protocol_name()
 *
 * Sets name of protocol file.
 *
 * Inputs: SerialFile, Filename (will be copied)
 * RetCode: -
 *------------------------------------------------------------------------*/
void serial_set_protocol_name(struct SerialFile *sf, const char *str)
{
  strredup(sf->protocol_filename, str);
  debug(2, "serial: Setting protocol name to %s", str);
}

/*--------------------------------------------------------------------------
 * void serial_set_device()
 *
 * Sets name of Euracom serial device. DON'T call in active subsystem
 *
 * Inputs: SerialFile, Device (will be copied)
 * RetCode: -
 *------------------------------------------------------------------------*/
void serial_set_device(struct SerialFile *sf, const char *str)
{
  strredup(sf->fd_device, str);
  debug(2, "serial: Euracom device is %s", str);
}

/*--------------------------------------------------------------------------
 * static char *device2lockfile()
 *
 * Converts sf->fd_device name into lockfile name.  Can only be called
 * _after_ serial_set_device()
 *
 * Inputs: SerialFile, Buffer
 * RetCode: Lockfile name
 *------------------------------------------------------------------------*/
static char *device2lockfile(const struct SerialFile *sf, char *lock_file)
{
  char *s;

  /* Construct lockfile-name */
  unless ((s=strrchr(sf->fd_device,'/'))) {
    log_msg(ERR_CRIT, "Invalid device name %s: Can't convert to lockfile", sf->fd_device);
    return(NULL);
  }

  s++;
  sprintf(lock_file, "%s/LCK..%s", LOCKPATH, s);
  debug(3, "device2lockfile: %s -> %s", sf->fd_device, lock_file);
  return(lock_file);
}

/*--------------------------------------------------------------------------
 * int serial_query_fd()
 *
 * Returns fd of Euracom
 *
 * Inputs: SerialFile
 * RetCode: Fd (0 if subsystem isn't initialized)
 *------------------------------------------------------------------------*/
int serial_query_fd(const struct SerialFile *sf)
{
  return(sf->fd);
}

/*--------------------------------------------------------------------------
 * BOOLEAN serial_open_device()
 *
 * Opens SerialFile file (with locking)
 *
 * Inputs: SerialFile
 * RetCode: TRUE: O.k FALSE: Error
 *------------------------------------------------------------------------*/
BOOLEAN serial_open_device(struct SerialFile *sf)
{
  FILE *fp;
  int flags;
  struct termios term;
  char lock_file[128];

  debug(1, "Initializing RS232 port %s...", sf->fd_device);
  device2lockfile(sf, lock_file);

  /* Part 1a - Check for old lockfile */
  debug(2, "Checking status of lockfile %s", lock_file);
  if ((fp=fopen(lock_file, "r"))) {
    pid_t pid;

    fscanf(fp, "%d", &pid);
    if (kill(pid, 0)) {
      log_msg(ERR_WARNING, "Stale lock file found in %s (pid %d)", lock_file, pid);
    } else {
      log_msg(ERR_CRIT, "Device already locked by PID %d", pid);
      return(FALSE);
    }   /* IF kill */
    fclose(fp);
  }

  /* Part 1b - Lock device */
  if ((fp=fopen(lock_file, "w"))) {
    pid_t pid = getpid();
    
    debug(2, "Locking device using %s (pid %d)", lock_file, pid);
    fprintf(fp, "%11d", pid);
    fclose(fp);
  } else {
    log_msg(ERR_FATAL, "Writing lockfile %s failed: %s", lock_file, strerror(errno));
    return(FALSE);
  }
  
  /* Part 2 - Open device */
  if ((sf->fd=open(sf->fd_device, O_RDWR | O_NOCTTY | O_NDELAY))<0) {
    log_msg(ERR_CRIT, "Error opening %s: %s", sf->fd_device, strerror(errno));
    sf->fd=0;
    serial_close_device(sf);
    return(FALSE);
  }

  fcntl(sf->fd, F_SETFL, O_RDWR);

  /* Part 3 - Set line parameters (9600, N, 8, 1) */

  /* Make a copy of original line settings */
  if (TTY_GETATTR(sf->fd, &(sf->term))==-1) {
    log_msg(ERR_CRIT, "Could not backup line settings: %s", strerror(errno));
    serial_close_device(sf);
    return(FALSE);
  }
  if (TTY_GETATTR(sf->fd, &term)==-1) {
    log_msg(ERR_CRIT, "Could get working set of line settings: %s", strerror(errno));
    serial_close_device(sf);
    return(FALSE);
  }

  /* Put line in transparent state */
  cfmakeraw(&term);

  /* Set in/out speed to 9600 baud */
  cfsetispeed(&term, B9600);
  cfsetospeed(&term, B9600);

  if (TTY_SETATTR(sf->fd, &term)==-1) {
    log_msg(ERR_CRIT, "tcsetattr: %s", strerror(errno));
    serial_close_device(sf);
    return(FALSE);
  }

  /* Make sure RTS is set */
  ioctl(sf->fd, TIOCMGET, &flags);
  flags|=TIOCM_RTS;
  ioctl(sf->fd, TIOCMSET, &flags);

  /* Give her a bit time to react upon RTS */
  sleep(1);

  ioctl(sf->fd, TIOCMGET, &flags);
  unless (flags & TIOCM_RTS) {
    log_msg(ERR_CRIT, "Serial driver did not set RTS");
    serial_close_device(sf);
    return(FALSE);
  }
#if !DONT_CHECK_CTS
  unless (flags & TIOCM_CTS) {
    log_msg(ERR_CRIT, "Euracom did not raise CTS. Check connection");
    serial_close_device(sf);
    return(FALSE);
  }
#endif

  debug(3, "serial_open_device: Device %s opened, fd is %d", sf->fd_device, sf->fd);
  return(TRUE);
}

/*--------------------------------------------------------------------------
 * BOOLEAN serial_close_device()
 *
 * Closes RS232 port, resets RTS
 *
 * Inputs: SerialFile
 * RetCode: FALSE: Error; TRUE: O.k
 *------------------------------------------------------------------------*/
BOOLEAN serial_close_device(struct SerialFile *sf)
{
  char lock_file[128];

  debug(1, "Shutting down RS232 port %s...", sf->fd_device);

  /* In case we got called from the signal handler during init of v24 */
  if (sf->fd) {
    int flags = 0;
    
    debug(2, "Resetting V24 line");

    /* Reset line to original state */
    if (TTY_SETATTR(sf->fd, &(sf->term))==-1) {
      log_msg(ERR_CRIT, "tcsetattr failed: %s: Serial line may remain in a weird state", strerror(errno));
    }

    /* Clear all control lines */
    ioctl(sf->fd, TIOCMSET, &flags);

    /* Give her a bit time to answer on RTS loss */
    sleep(1);

    /* Check for successful termination */
    ioctl(sf->fd, TIOCMGET, &flags);

    /* Anyway, I won't care */
    close(sf->fd); sf->fd=0;

    /* Feedback to user */
    if ((flags & TIOCM_RTS)) {
      log_msg(ERR_CRIT, "Serial driver did not unset RTS");
    }
    if ((flags & TIOCM_CTS)) {
      log_msg(ERR_CRIT, "Euracom did not unset CTS");
    }
  }

  /* Remove lockfile */
  if (device2lockfile(sf, lock_file)) {
    delete_file(lock_file);
  }

  return(TRUE);
}

/*--------------------------------------------------------------------------
 * char *readln_rs232()
 *
 * Reads complete line (terminated by 0A 0D 00 [ASCIIZ]) from SerialFile
 *
 * Inputs: SerialFile
 * RetCode: Ptr to static string; NULL: Error/Timeout
 *------------------------------------------------------------------------*/
char *readln_rs232(struct SerialFile *sf)
{
  char *cp=sf->buffer;

  do {
    struct timeval tv;
    int retval;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(sf->fd, &rfds);
    tv.tv_sec=15; tv.tv_usec=0;
    retval=select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

    if (retval==0) {	/* Timeout */
      log_msg(ERR_ERROR, "Timeout while reading RS232 input (losing %d chars)", 
        (cp-sf->buffer));
      *cp='\0';
      log_msg(ERR_NOTICE, "Line noise: %s", sf->buffer);
      return(NULL);
    } elsif (retval>0) {
      int inbuf[2];

      read(sf->fd, &inbuf[0], 1);
      *cp=inbuf[0];
      debug(6, "readln_rs232: Read char %d", *cp);
#if (FIRMWARE_MAJOR<2)
      if (*cp=='\0') {	/* 1.x: blah (0A) 0D 00 */
        *(cp-2)='\0';
#else
      if (*cp==0x0d) {	/* 2.x: blah 0A 0D */
        *(cp-1)='\0';
#endif
        break;
      } else {
	cp++;
        /* I think this is unlikely to happen, but:
           "Be prepared... that's the Boy Scout's solemn creed"
        */
        if ((cp-(sf->buffer))>1023) {
	  log_msg(ERR_CRIT, "Buffer overflow while reading RS232 input");
          return(NULL);
        }
      }
    } else {
      log_msg(ERR_CRIT, "select() failed in readln_rs232: %s", strerror(errno));
      return(NULL);
    }
  } while (1);  /* Argh! */

  debug(5, "Full line read from serial port");
  /* Make a copy of line if requested */
  if (sf->protocol_filename) {
    FILE *fp = fopen(sf->protocol_filename, "a");

    if (fp) {
      debug(5, "Appending to protocol file");
      fputline(fp, sf->buffer);
      fclose(fp);
    }
  }
  return(sf->buffer);
}
