/* config.h.  Generated automatically by configure.  */
/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * config.h -- Autoconf/local configuration
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-04 16:51:00 GMT
 * Version:             $Revision: 1.16 $
 * Last modified:       $Date: 1999/06/03 12:30:40 $
 * Keywords:            ISDN, Euracom, Ackermann
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

/*
 * $Id: config.h,v 1.16 1999/06/03 12:30:40 bus Exp $
 */

#if !defined(_CONFIG_H)
#define _CONFIG_H

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define as 1 if you have paths.h */
#define HAVE_PATHS_H		0

/* If set to 1, we have to declare sys_errlist as external */
#define NEED_SYS_ERRLIST	0

/* Define as 1 if you want debugging messages to be compiled into
   the programs.  You DO want this, trust me */
#define DEBUG			1

/* Define as 1 if you want support for KIT DUMP. */
#define KIT_DUMP_MODE		1

/* Define this if euracom has problems detecting CTS */
#define DONT_CHECK_CTS		0

/* Set this to the major version number of the firmware you're using */
#define FIRMWARE_MAJOR		3

/* Directory where to write the device lock file */
#define LOCKPATH		"/var/lock"

/* Where to write PID file */
#define PIDFILE			"/var/run/euracom.pid"

/* Drop connection after that amount of idle time */
#define SHUTDOWN_TIMEOUT	120

/* Try to re-establish connection after waiting this amount of time */
#define RECOVERY_TIMEOUT	900

/* Define this to your local countrycode */
#define COUNTRYCODE		"+49"

/* Define this to your local areacode (including countrycode) */
#define AREACODE		"+492364"

/* Name of local currency */
#define LOCAL_CURRENCY		"DEM"

/* Name of DB to connect */
#define DEF_DB			"isdn"

/* Name of recovery file (in case connection to DB is down) */
#define RECOVERY_FILE		"/tmp/euracom.recovery"

/* --------------------------------------------------------------------------
 * To be adjusted manually!
 * --------------------------------------------------------------------------
 */

/*
 * Facility for syslog()
 */
#define DEF_LOGFAC		LOG_LOCAL0

/*
 * Shall I use the time/date spec as given by the PABX
 * or may I trust the System Clock?
 * Note that the euracom clock is buggy in f/w 1.1x.
 * !! THIS OPTION CURRENTLY HAS NO EFFECT
 */
#define	USE_DATE_FROM		pabx
/* #define	USE_DATE_FROM	system */

/* --------------------------------------------------------------------------
 * V24/interface options
 * --------------------------------------------------------------------------
 */

/*
 * Use TERMIOS or TERMIO
 */
#define HAVE_TERMIOS
/* #undef  HAVE_TERMIO */

/*
 * Use my weird macros
 */
#define elsif                   else if
#define unless(s)               if (!(s))
#define strredup(ptr, str)      safe_free(ptr),ptr=strdup(str)

#define get_strerror            strerror(errno)

#if DEBUG
#define debug(level,args...)    log_debug(level,##args)
#else
#define debug(level,args...)    do { } while(0)
#endif

#endif
