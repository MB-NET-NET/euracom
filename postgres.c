/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * postgres.c -- PostgreSQL database subsystem
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1997-08-28 09:30:44 GMT
 * Version:             $Revision: 1.7 $
 * Last modified:       $Date: 1998/01/08 12:32:18 $
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

static char rcsid[] = "$Id: postgres.c,v 1.7 1998/01/08 12:32:18 bus Exp $";

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <string.h>
#include <stdlib.h>
#include <libpq-fe.h>

#include "log.h"
#include "utils.h"
#include "fileio.h"

#include "euracom.h"

/* Private data */
static char *pg_host = NULL;
static char *pg_port = NULL;
static char *pg_db = NULL;
static unsigned int shutdown_timeout = SHUTDOWN_TIMEOUT;
static unsigned int recovery_timeout = RECOVERY_TIMEOUT;

static time_t last_db_write = 0;
static time_t last_recovery = 0;

static PGconn *db_handle = NULL;
static enum DB_State { DB_OPEN, DB_CLOSED, RECOVERY} db_state = DB_CLOSED;

/*
  Function prototypes
*/
void database_set_host(const char *str);
void database_set_port(const char *str);
void database_set_db(const char *str);
void database_set_shutdown_timeout(int i);
void database_set_recovery_timeout(int i);

BOOLEAN database_initialize(void);
BOOLEAN database_log(const char *cp);
BOOLEAN database_shutdown(void);
void database_check_state();

static BOOLEAN database_change_state();
static BOOLEAN database_perform_recovery();
static BOOLEAN database_pg_connect();
static BOOLEAN database_pg_execute(const char *stc, BOOLEAN do_recovery);
static BOOLEAN database_write_recovery(const char *stc);

/* Code */

/* Some initialization routines */
void database_set_host(const char *str)
{
  log_msg(ERR_JUNK, "database: Setting host to %s", str);
  if (pg_host) { free(pg_host); }
  pg_host=strdup(str);
}

void database_set_port(const char *str)
{
  log_msg(ERR_JUNK, "database: Setting port to %s", str);
  if (pg_port) { free(pg_port); }
  pg_port=strdup(str);
}

void database_set_db(const char *str)
{
  log_msg(ERR_JUNK, "database: Setting database to %s", str);
  if (pg_db) { free(pg_db); }
  pg_db=strdup(str);
}

void database_set_shutdown_timeout(int i)
{
  log_msg(ERR_JUNK, "database: Setting shutdown timeout to %s s", i);
  shutdown_timeout=i;
}

void database_set_recovery_timeout(int i)
{
  log_msg(ERR_JUNK, "database: Setting recovery timeout to %s s", i);
  shutdown_timeout=i;
}

/*
   Initializes database variables

   Does not establish a connection
*/
BOOLEAN database_initialize()
{
  log_msg(ERR_INFO, "Initializing database subsystem...");

  if (!pg_db) { pg_db=strdup(DEF_DB); }

  /* No reason to check internal variables */
  /* ...*/

  /* Initial state of statemachine */
  db_state=DB_CLOSED;

  log_msg(ERR_JUNK, "Will disconnect from DB after %ds idle time", shutdown_timeout);
  log_msg(ERR_JUNK, "Will retry to establish connection after %ds", recovery_timeout);
  return(TRUE);
}

/*
   Shuts down database subsytem
*/
BOOLEAN database_shutdown()
{
  log_msg(ERR_INFO, "Shutting down database subsystem");
  
  /* Release backend */
  database_change_state(DB_CLOSED);

  /* Free internal variables */
  if (pg_host) { free(pg_host); }
  if (pg_port) { free(pg_port); }
  if (pg_db) { free(pg_db); }
  
  return(TRUE);
}


/*
   Gets usually called in idle times to check whether
   we can disconnect from db or if we should try to perform
   recovery
*/
void database_check_state()
{
  time_t now = time(NULL);

  switch (db_state) {
    case DB_OPEN:
      /* If no write for SHUTDOWN_TIMEOUT secs, shut it down */
      if (now-last_db_write>shutdown_timeout) {
        database_change_state(DB_CLOSED);
      }
      break;

    case DB_CLOSED:
      /* If it is closed, don't do anything */
      break;

    case RECOVERY:
      /* When recovery timeout has exeeded, try to re-establish connection */
      if (now-last_recovery>recovery_timeout) {
        log_msg(ERR_INFO, "Trying to re-establish connection");
        database_change_state(DB_OPEN);
      }
      break;

    default:
      log_msg(ERR_CRIT, "CASE missing in database_check_state");
      break;
  }
}


BOOLEAN database_change_state(enum DB_State new_state)
{
  BOOLEAN ok = FALSE;
  
  if (db_state==new_state) return(TRUE);

  switch(db_state) {
    case DB_OPEN:
      switch (new_state) {
        case DB_CLOSED:
          PQfinish(db_handle);
          ok=TRUE;
          break;

        case RECOVERY:
          log_msg(ERR_INFO, "Entering recovery mode");
          PQfinish(db_handle);
          ok=TRUE;
          last_recovery=time(NULL);
          break;

        default:
          log_msg(ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state);
          break;
      }
      break;

    case DB_CLOSED:
      switch (new_state) {
        case DB_OPEN:
          if (database_pg_connect()) {
            ok=TRUE;
          } else {
            log_msg(ERR_INFO, "Initial open failed.  Fall back to recover");
            ok=TRUE; new_state=RECOVERY;
            last_recovery=time(NULL);
          }
          break;
        case DB_CLOSED:
        case RECOVERY:
        default:
          log_msg(ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state);
          ok=TRUE;
          break;
      }
      break;

    case RECOVERY:
      switch (new_state) {
        case DB_OPEN:
          if (database_pg_connect()) {
            log_msg(ERR_INFO, "Connection re-established.  Using recovery file");
            ok=TRUE; db_state=DB_OPEN; /* Temporarily switch to DB_OPEN */
            if (database_perform_recovery()) {
              log_msg(ERR_INFO, "Recovery completed successfully");
              ok=TRUE;
            } else {
              log_msg(ERR_WARNING, "Recovery failed!  Will stay in recovery mode");
              last_recovery=time(NULL); ok=FALSE; db_state=RECOVERY;
            }
          } else {
            log_msg(ERR_INFO, "Database still inaccessible");
            last_recovery=time(NULL); ok=FALSE; db_state=RECOVERY;
          }
          break;
        case DB_CLOSED:
          log_msg(ERR_WARNING, "DB shutdown while in recovery mode.  Check recovery file!");
          break;
        case RECOVERY:
        default:
          log_msg(ERR_CRIT, "State change %d -> %d not defined (CASE)", db_state, new_state);
          ok=TRUE;
          break;
      }
      break;

    default:
      log_msg(ERR_CRIT, "In unknown state %d", db_state);
      break;
  }

  if (ok) { db_state=new_state; }
  return(ok);
}


BOOLEAN database_perform_recovery()
{
  BOOLEAN reco_failed = FALSE;

  if (check_file(RECOVERY_FILE)) {
    FILE *fp;
    char *cp = (char *)get_unique_tmpname("EuRec", NULL);
    char *zeile;

    unless (copy_file(RECOVERY_FILE, cp)) {
      log_msg(ERR_CRIT, "Copying recovery file failed");
      free(cp);
      return(FALSE);
    }

    unless (delete_file(RECOVERY_FILE)) {
      log_msg(ERR_CRIT, "Deletion of old recovery file failed");
      delete_file(cp);
      free(cp);
      return(FALSE);
    }

    unless (fp=fopen(cp, "r")) {
      log_msg(ERR_FATAL, "My own copy of the recovery file is gone.  I'm confused!");
      free(cp);
      return(FALSE);
    }

    log_msg(ERR_INFO, "Starting recovering from file %s", cp);
    while ((zeile=fgetline(fp, NULL))) {
      unless (database_pg_execute(zeile, TRUE)) {
        reco_failed=TRUE;
        database_change_state(RECOVERY);
      }
    }

    fclose(fp);
    unlink(cp);
    free(cp);
  }
  return(!reco_failed);
}


/*
   Inserts geb->x values into table
*/
BOOLEAN database_log(const char *cp)
{
  /* When db is closed, we have to re-establish a connection */
  if (db_state==DB_CLOSED) database_change_state(DB_OPEN);

  /* Writing into DB failed? -> Switch to recovery mode */
  unless (database_pg_execute(cp, TRUE)) {
    database_change_state(RECOVERY);
  }
  return(TRUE);
}


/* -----------------------------------------------------------------------
   Mid-level: Postgres routines
*/

/* Establish connection to database
   Will NOT call any state change routines!
*/
BOOLEAN database_pg_connect()
{
  db_handle=PQsetdb(pg_host, pg_port, NULL, NULL, pg_db);

  if (PQstatus(db_handle) == CONNECTION_BAD) {
    log_msg(ERR_ERROR, "PostgreSQL connect: %s", PQerrorMessage(db_handle));
    PQfinish(db_handle);
    return(FALSE);
  }
  return(TRUE);
}


/*
   Executes SQL statement.

   RetCode: TRUE: Written into DB; FALSE: Recovery file
*/
BOOLEAN database_pg_execute(const char *stc, BOOLEAN do_recovery)
{
  PGresult *pgres;

  switch (db_state) {
    case DB_OPEN:
      pgres=PQexec(db_handle, stc);
      if (PQresultStatus(pgres)!=PGRES_COMMAND_OK) {
        log_msg(ERR_ERROR, "PostgreSQL error: %s", PQcmdStatus(pgres));
        PQclear(pgres);
        if (do_recovery) { database_write_recovery(stc); }
        return(FALSE);
      } else {
        /* Write successful */
        last_db_write=time(NULL);
      }
      PQclear(pgres);
      return(TRUE);
      break;

    case DB_CLOSED:
      log_msg(ERR_WARNING, "User wants insert in closed db");
      break;

    case RECOVERY:
      if (do_recovery) { database_write_recovery(stc); }
      break;
  }
  return(FALSE);
}
  

BOOLEAN database_write_recovery(const char *stc)
{
  FILE *fp = fopen(RECOVERY_FILE, "at");

  unless (fp) {
    log_msg(ERR_FATAL, "Writing into recovery file failed!");
    log_msg(ERR_FATAL, "%s", stc);
    return(FALSE);
  }

  fputline(fp, stc);

  fclose(fp);
  return(TRUE);
}
