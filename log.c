/*********************************************************************/
/* log.c - Logging utilities                                         */
/*                                                                   */
/* Copyright (C) 1996-1998 MB Computrex           Released under GPL */
/*********************************************************************/

/*---------------------------------------------------------------------
 * Version:	$Id: log.c,v 1.4 2000/12/17 17:14:48 bus Exp $
 * File:	$Source: /home/bus/Y/CVS/euracom/log.c,v $
 *-------------------------------------------------------------------*/

static char rcsid[] = "$Id: log.c,v 1.4 2000/12/17 17:14:48 bus Exp $";

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>

#include "log.h"

static int logger_debuglevel = 0;
static char logger_prefix[64] = "unknown";
static char *logger_logfile = NULL;
static int logger_options = USE_STDERR;
static FILE *logger_file = NULL;

static void logger_write_line(const char *extra, const char *stc)
{
  char timestr[20] = "";

  if (logger_options & TIMESTAMP) {
    time_t tval = time(NULL);
    struct tm *zeit = localtime(&tval);

    strftime(timestr, sizeof(timestr), "[%d.%m %H:%M:%S] \0", zeit);
  }

  if (logger_options & USE_STDERR) {
    fprintf(stderr, "%s%s%s: %s\n", timestr, logger_prefix, extra, stc);
  }

  if (logger_file) {
    fprintf(logger_file, "%s%s%s: %s\n", timestr, logger_prefix, extra, stc);
    fflush(logger_file);
  }
}
  
/*----------------------------------------------------------------------*/
/* void log_msg()                                                       */
/*                                                                      */
/* Logs message to stdout or syslog                                     */
/*----------------------------------------------------------------------*/
void log_msg(enum ErrorLevel level, const char *fmt, ...)
{ 
  char buf[1024];

  va_list ap;
  static char *descr[] = {
    " fatal error", " critical error", " error", " warning", " notice", ""
  };

  va_start(ap, fmt);
  vsnprintf(buf, 1023, fmt, ap); buf[1023]='\0';
  va_end(ap);

  /* When in SYSLOG mode, map program log level into less offensive syslog levels */
  if (logger_options & USE_SYSLOG) {
    int syslog_prio;

    switch(level) {
      case ERR_FATAL:
        syslog_prio=LOG_ERR;
        break;
      case ERR_CRIT:
      case ERR_ERROR:
        syslog_prio=LOG_WARNING;
        break;
      case ERR_WARNING:
        syslog_prio=LOG_NOTICE;
        break;
      case ERR_NOTICE:
        syslog_prio=LOG_INFO;
        break;
      default:
        syslog_prio=LOG_DEBUG;
        break;
    }
    syslog(syslog_prio, "%s", buf);
  }	/* IF & SYSLOG */

  /* Print to file and/or stderr if requested */
  logger_write_line(descr[level], buf);
}

/*----------------------------------------------------------------------*/
/* void log_debug()                                                     */
/*                                                                      */
/* Logs debug message                                                   */
/*----------------------------------------------------------------------*/
void log_debug(int level, const char *fmt, ...)
{ 
  if (level<=logger_debuglevel) {
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, 1023, fmt, ap); buf[1023]='\0';
    va_end(ap);

    logger_write_line("", buf);
  }
}

void logger_set_prefix(const char *stc)
{
  strcpy(logger_prefix, stc);

  /* Print ridiculous debug message */
  debug(2, "logger: Prefix has been set to %s", stc);
}

void logger_set_level(int level)
{
  logger_debuglevel=level;
  debug(2, "logger: Debuglevel has been set to %d", level);
}

void logger_set_options(int flags)
{
  logger_options=flags;
}

void logger_set_logfile(const char *stc)
{
  if (logger_logfile) {
    free(logger_logfile);
  }
  if (stc) {
    logger_logfile=strdup(stc);
    debug(2, "logger: Logfile has been set to %s", stc);
  } else {
    logger_logfile=NULL;
  }
}

/*----------------------------------------------------------------------*/
/* void logger_initialize()                                             */
/*                                                                      */
/* Initializes logging. Opens syslog if neccessary                      */
/* Supported flags: USE_SYSLOG, USE_STDERR, TIMESTAMP                   */
/*----------------------------------------------------------------------*/
void logger_initialize()
{
  /* Do logfile handling */
  if (logger_logfile) {

    /* If a logfile is already open, close it */
    if (logger_file) {
      debug(1, "Logfile already active. Closing %s...", logger_logfile);
      fclose(logger_file);
      logger_file=NULL;
    }

    if ((logger_file=fopen(logger_logfile, "a"))) {
      debug(1, "Opened logfile %s", logger_logfile);
    } else {
      log_msg(ERR_ERROR, "Error creating logfile %s: %s", logger_logfile, strerror(errno));
      logger_file=NULL;
    }
  }

  /* Syslog mode? */
  if (logger_options & USE_SYSLOG) {
    openlog(logger_prefix, LOG_PID, LOG_USER);
    debug(3, "Opened syslog facility");
  }
}

/*----------------------------------------------------------------------*/
/* void logger_shutdown()                                               */
/*                                                                      */
/* Closes logging. Closes syslog if neccessary                          */
/*----------------------------------------------------------------------*/
void logger_shutdown()
{
  debug(2, "Closing logging services");
  if (logger_file) {
    fclose(logger_file);
    logger_file=NULL;
  }

  if (logger_options & USE_SYSLOG) {
    closelog();
  }
}
