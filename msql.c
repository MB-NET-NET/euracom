/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * msql.c -- msql (miniSQL) database subsystem
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 *                      Michael Tepperis <michael.tepperis@fernuni-hagen.de>
 * Created:             1997-08-28 09:30:44 GMT
 * Version:             $Revision: 1.7 $
 * Last modified:       $Date: 1999/03/13 16:56:59 $
 * Keywords:            ISDN, Euracom, Ackermann, mSQL
 *
 * based on 'postgres.c' applied with changes needed by msql
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

static char rcsid[] = "$Id: msql.c,v 1.7 1999/03/13 16:56:59 bus Exp $";

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <string.h>
#include <stdlib.h>

#include "msql.h"

#include "log.h"
#include "utils.h"

#include "euracom.h"

/*
  Private data
 */
static char *msql_host = NULL;
static char *msql_port = NULL;
static char *msql_db = NULL;

static unsigned int shutdown_timeout = SHUTDOWN_TIMEOUT;
static unsigned int recovery_timeout = RECOVERY_TIMEOUT;

static time_t last_db_write = 0;
static time_t last_recovery = 0;

static int st;	/* mSQL database handle */
static enum DB_State { DB_OPEN, DB_CLOSED, RECOVERY } db_state = DB_CLOSED;

/*
  Function prototypes
 */
static BOOLEAN database_change_state( );
static BOOLEAN database_perform_recovery( );
static BOOLEAN database_msql_connect( );
static BOOLEAN database_msql_execute( const char *stc, BOOLEAN do_recovery );
static BOOLEAN database_write_recovery( const char *stc );

/*
  Some initialization routines
 */
void database_set_host( const char *str )
 {
  debug( 2, "database: Setting host to %s", str );
  if ( msql_host )
   {
    free( msql_host );
   }
  msql_host = strdup( str );
 }

void database_set_port( const char *str )
 {
  debug( 2, "database: Setting port to %s", str );
  if ( msql_port )
   {
    free( msql_port );
   }
  msql_port = strdup( str );
 }

void database_set_db( const char *str )
 {
  debug( 2, "database: Setting database to %s", str );
  if ( msql_db )
   {
    free( msql_db );
   }
  msql_db = strdup( str );
 }

void database_set_shutdown_timeout( int i )
 {
  debug( 2, "database: Setting shutdown timeout to %d s", i );
  shutdown_timeout = i;
 }

void database_set_recovery_timeout( int i)
 {
  debug( 2, "database: Setting recovery timeout to %d s", i );
  recovery_timeout = i;
 }

/*
   Initializes database variables
   Does not establish a connection
 */
BOOLEAN database_initialize( )
 {
  debug( 1, "Initializing database subsystem..." );

  unless ( msql_db )
   {
    msql_db = strdup( DEF_DB );
   }

  /* No reason to check internal variables */
  /* ...*/

  /* Initial state of statemachine */
  db_state = DB_CLOSED;
  debug( 1, "Will disconnect from DB after %ds idle time", shutdown_timeout) ;
  debug( 1, "Will retry to establish connection after %ds", recovery_timeout );
  return( TRUE );
 }

/*
   Shuts down database subsytem
 */
BOOLEAN database_shutdown( )
 {
  debug( 1, "Shutting down database subsystem" );
  
  /* Release backend */
  database_change_state( DB_CLOSED );

  /* Free internal variables */
  if ( msql_host )
   {
    free( msql_host); }
  if ( msql_port )
   {
    free( msql_port );
   }
  if ( msql_db )
   {
    free( msql_db );
   }
  
  return( TRUE );
 }

/*
   Gets usually called in idle times to check whether
   we can disconnect from db or if we should try to perform
   recovery
 */
void database_check_state( )
 {
  time_t now = time( NULL );

  switch ( db_state )
   {
    case DB_OPEN:
      /* If no write for SHUTDOWN_TIMEOUT secs, shut it down */
      if ( now-last_db_write > shutdown_timeout )
       {
        database_change_state( DB_CLOSED );
       }
      break;
    case DB_CLOSED:
      /* If it is closed, don't do anything */
      break;
    case RECOVERY:
      /* When recovery timeout has exeeded, try to re-establish connection */
      if ( now-last_recovery > recovery_timeout )
       {
        log_msg( ERR_INFO, "Trying to re-establish connection" );
        database_change_state( DB_OPEN );
       }
      break;
    default:
      log_msg( ERR_CRIT, "CASE missing in database_check_state" );
      break;
   }
 }

/*
 */

BOOLEAN database_change_state( enum DB_State new_state )
 {
  BOOLEAN ok = FALSE;
  
  if ( db_state == new_state )
   return( TRUE );

  switch( db_state )
   {
    case DB_OPEN:
      switch ( new_state )
       {
        case DB_CLOSED:
          msqlClose( st );
          ok = TRUE;
          break;
        case RECOVERY:
          log_msg( ERR_INFO, "Entering recovery mode" );
          msqlClose( st );
          ok = TRUE;
          last_recovery = time( NULL );
          break;
        default:
          log_msg( ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state );
          break;
       }
      break;
    case DB_CLOSED:
      switch ( new_state )
       {
        case DB_OPEN:
          if ( database_msql_connect( ) )
	   {
            ok = TRUE;
           }
	  else
	   {
            log_msg( ERR_INFO, "Initial open failed.  Fall back to recover" );
            ok            = TRUE;
	    new_state     = RECOVERY;
            last_recovery = time( NULL );
           }
          break;
        case DB_CLOSED:
        case RECOVERY:
        default:
          log_msg( ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state );
          ok = TRUE;
          break;
       }
      break;
    case RECOVERY:
      switch ( new_state )
       {
        case DB_OPEN:
          if ( database_msql_connect( ) )
	   {
            log_msg( ERR_INFO, "Connection re-established.  Using recovery file" );
            ok       = TRUE;
	    db_state = DB_OPEN; /* Temporarily switch to DB_OPEN */
            if ( database_perform_recovery( ) )
	     {
              log_msg( ERR_INFO, "Recovery completed successfully" );
              ok = TRUE;
             }
	    else
	     {
              log_msg( ERR_WARNING, "Recovery failed!  Will stay in recovery mode" );
              last_recovery = time( NULL );
	      ok            = FALSE;
	      db_state      = RECOVERY;
             }
           }
	  else
	   {
            log_msg( ERR_INFO, "Database still inaccessible" );
            last_recovery = time( NULL );
	    ok            = FALSE;
	    db_state      = RECOVERY;
           }
          break;
        case DB_CLOSED:
          log_msg( ERR_WARNING, "DB shutdown while in recovery mode.  Check recovery file!" );
          break;
        case RECOVERY:
        default:
          log_msg( ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state );
          ok = TRUE;
          break;
       }
      break;
    default:
      log_msg( ERR_CRIT, "In unknown state %d", db_state );
      break;
   }
  if ( ok )
   {
    db_state = new_state;
   }
  return( ok );
 }


BOOLEAN database_perform_recovery( )
 {
  BOOLEAN reco_failed = FALSE;

  if ( check_file( RECOVERY_FILE ) )
   {
    FILE *fp;
    char *cp = ( char * )get_unique_tmpname( "EuRec", NULL );
    char *zeile;

    unless ( copy_file( RECOVERY_FILE, cp ) )
     {
      log_msg( ERR_CRIT, "Copying recovery file failed" );
      free( cp );
      return( FALSE );
     }
    unless ( delete_file( RECOVERY_FILE ) )
     {
      log_msg( ERR_CRIT, "Deletion of old recovery file failed" );
      delete_file( cp );
      free( cp );
      return( FALSE );
     }
    unless ( fp = fopen( cp, "r" ) )
     {
      log_msg( ERR_FATAL, "My own copy of the recovery file is gone.  I'm confused!" );
      free( cp );
      return( FALSE );
     }
    log_msg( ERR_INFO, "Starting recovering from file %s", cp );
    while ( ( zeile = fgetline( fp, NULL ) ) )
     {
      unless ( database_msql_execute( zeile, TRUE ) )
       {
        reco_failed = TRUE;
        database_change_state( RECOVERY );
       }
     }
    fclose( fp );
    unlink( cp );
    free( cp );
   }
  return( !reco_failed );
 }


/*
   Inserts geb->x values into table
 */
BOOLEAN database_log( const char *cp )
 {
  /* When db is closed, we have to re-establish a connection */
  if ( db_state == DB_CLOSED )
   database_change_state( DB_OPEN );

  /* Writing into DB failed? -> Switch to recovery mode */
  unless ( database_msql_execute( cp, TRUE ) )
   {
    database_change_state( RECOVERY );
   }
  return( TRUE );
 }


/* -----------------------------------------------------------------------
   Mid-level: mSQL routines
 */

/* Establish connection to database
   Will NOT call any state change routines!
 */
static BOOLEAN database_msql_connect( )
 {
  debug( 3, "Opening connection to database" );
  /* Datenbank-Server-Verbindung herstellen */
  st = msqlConnect( NULL );
  if( st < 0 )
   {
    log_msg( ERR_ERROR, "mSQL connect: ERROR" );
    msqlClose( st );
    return( FALSE );
   }
  /* Datenbank-Verbindung herstellen */
  if( msqlSelectDB( st, msql_db ) < 0 )
   {
    log_msg( ERR_ERROR, "Die Datenbank \"%s\" konnte nicht geöffnet werden.\n\n", msql_db );
   }
  return( TRUE );
 }


/*
   Executes SQL statement.
   RetCode: TRUE: Written into DB; FALSE: Recovery file
 */
static BOOLEAN database_msql_execute( const char *stc, BOOLEAN do_recovery )
{
  m_result *res;

  switch ( db_state )
   {
    case DB_OPEN:
      debug( 4, "Executing SQL: %s", stc );
      res=msqlQuery(st, stc);
      unless (res) {
        log_msg(ERR_ERROR, "mSQL connection failure:");
        if (do_recovery) { database_write_recovery(stc); }
        return( FALSE );
      }
      /* Write successful */
      last_db_write = time(NULL);
      return(TRUE);
      break;

    case DB_CLOSED:
      log_msg( ERR_WARNING, "User wants insert in closed db" );
      break;

    case RECOVERY:
      if ( do_recovery ) {
        database_write_recovery( stc );
      }
      break;
   }
  return( FALSE );
}
  
static BOOLEAN database_write_recovery(const char *stc)
 {
  FILE *fp = fopen(RECOVERY_FILE, "at");

  unless (fp) {
    log_msg( ERR_FATAL, "Writing into recovery file failed!" );
    log_msg( ERR_FATAL, "%s", stc );
    return(FALSE);
  }
  fputline(fp, stc);
  fclose(fp);
  return(TRUE);
}

/*--------------------------------------------------------------------------
 * BOOLEAN database_geb_log()
 *
 * Writes charge data into database
 *------------------------------------------------------------------------*/
BOOLEAN database_geb_log(const struct GebuehrInfo *geb)
 {
  char statement[ 4096 ]; /* SQL Statement */
  char date_fmt[ 15 ];    /* dd-mmm-yyyy; warum ich 15 statt 12 benoetige, ist mir unklar*/
  char time_fmt[ 9 ];     /* hh:mm:ss */

  sprintf(statement, "INSERT INTO euracom ( int_no, remote_no, einheiten, direction, pay, length, currency, vst_date, vst_time, sys_date, sys_time ) values ( %d, '%s', ",
           geb->teilnehmer,
           geb->nummer );  

  switch (geb->art) {
    case GEHEND:
      strcatf(statement, "%d, 'O', %f, %d, '%s'",
               geb->einheiten,
               geb->betrag,
               geb->length,
               LOCAL_CURRENCY);
      break;

    case KOMMEND:
      strcatf( statement, "0, 'I', 0, 0, ''" );
      break;
  }

  strftime(date_fmt,  sizeof(date_fmt),  "%d-%b-%Y", gmtime( &geb->datum_vst ) );
  strcatf(statement, ", '%s'", date_fmt );
  strftime(time_fmt, sizeof(time_fmt), "%H:%M:%S", gmtime( &geb->datum_vst ) );
  strcatf(statement, ", '%s'", time_fmt );

  strftime(date_fmt,  sizeof( date_fmt ),  "%d-%b-%Y", gmtime( &geb->datum_sys ) );
  strcatf(statement, ", '%s'", date_fmt );
  strftime(time_fmt, sizeof( time_fmt ), "%H:%M:%S", gmtime( &geb->datum_sys ) );
  strcatf(statement, ", '%s' )", time_fmt );

  return(database_log(statement));
}

