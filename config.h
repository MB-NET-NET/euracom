/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * config.h -- Local configuration
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-04 16:51:00 GMT
 * Version:             $Revision: 1.3 $
 * Last modified:       $Date: 1998/01/17 13:27:38 $
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
 * Price for 1 unit.  Should be consistent with the settings in Euracom's
 * PROM or things will be, umm, inconsistent :-)
 */
#define PRICE_PER_UNIT		0.12

/*
 * Facility for syslog()
 */
#define DEF_LOGFAC		LOG_LOCAL0


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
 * Where to write PID file
 */
#define PIDFILE			"/var/run/euracom.pid"


/* --------------------------------------------------------------------------
 * PostgreSQL database options
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


/* --------------------------------------------------------------------------
 * Euracom settings.  You will most probably not need to change these!
 * --------------------------------------------------------------------------
 */

/*
 * String Euracom sends as telephone number when remote number could not be
 * determined
 */
#define UNKNOWN_TEXT_EURA	"Rufnr.unbekannt"


/* --------------------------------------------------------------------------
 * DO NOT CHANGE ANYTHING OF THESE OR TERRIBLE THINGS WILL HAPPEN!
 * --------------------------------------------------------------------------
 */
#define elsif else if
#define unless(s) if (!(s))

#endif
