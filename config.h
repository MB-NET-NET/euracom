/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * config.h -- Local configuration
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-04 16:51:00 GMT
 * Version:             $Revision: 1.10 $
 * Last modified:       $Date: 1998/08/29 08:44:58 $
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

#if !defined(_CONFIG_H)
#define _CONFIG_H

/* 
   !! PLEASE _DO_ CHECK THE FOLLOWING LINES AND CHANGE THEM ACCORDING TO
      YOUR LOCAL REQUIREMENTS
   !!
*/


/* --------------------------------------------------------------------------
 * Local settings
 * --------------------------------------------------------------------------
 */

/*
 * Define this to your local countrycode
 */
#define COUNTRYCODE		"+49"

/*
 * Define this to your local areacode (including countrycode)
 */
#define AREACODE		"+492364"

/*
 * Name of local currency
 * (I like the int'l currency names (e.g USD for US Dollar), but feel free
 * to use whatever you like (as long as it's max. 4 characters long)
 */
#define LOCAL_CURRENCY		"DEM"

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
 * Euracom settings
 * --------------------------------------------------------------------------
 */
#define FIRMWARE_MAJOR	1

/*
 * Define this to support parsing of KIT bulk data transfer
 */
#define KIT_DUMP_MODE	1


/* --------------------------------------------------------------------------
 * V24/interface options
 * --------------------------------------------------------------------------
 */

/*
 * Use TERMIOS or TERMIO
 */
#define HAVE_TERMIOS
#undef  HAVE_TERMIO

/*
 * Where to write the lockfile
 */
#define LOCKPATH		"/var/lock"

/*
 * Define this if euracom has problems detecting CTS
 */
#undef	DONT_CHECK_CTS


/* --------------------------------------------------------------------------
 * Daemon options
 * --------------------------------------------------------------------------
 */

/*
 * Where to write PID file
 */
#define PIDFILE			"/var/run/euracom.pid"


/* --------------------------------------------------------------------------
 * Database options
 * --------------------------------------------------------------------------
 */

/*
 * Name of DB to connect 
 */
#define DEF_DB			"isdn"

/*
 * Name of recovery file (in case connection to DB is down)
 */
#define RECOVERY_FILE		"/tmp/euracom.recovery"

/*
 * Timeouts (defaults)
 */
#define SHUTDOWN_TIMEOUT	120    /* Drop connection after 2 minutes idle time */
#define RECOVERY_TIMEOUT	900    /* Retry after 15 mins */

#endif
