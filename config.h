/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * config.h -- Local configuration
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-10-04 16:51:00 GMT
 * Version:             $Revision: 1.15 $
 * Last modified:       $Date: 1999/03/13 18:09:22 $
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

#if !defined(_CONFIG_H)
#define _CONFIG_H

#include "autoconf.h"

/* 
   !! PLEASE _DO_ CHECK THE FOLLOWING LINES AND CHANGE THEM ACCORDING TO
      YOUR LOCAL REQUIREMENTS
   !!
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
#undef  HAVE_TERMIO

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
