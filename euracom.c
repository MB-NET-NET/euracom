/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.c -- Main programme
 *
 * Copyright (C) 1996-1998 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-09 17:31:56 GMT
 * Version:             $Revision: 1.28 $
 * Last modified:       $Date: 1998/02/15 12:06:09 $
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

static char rcsid[] = "$Id: euracom.c,v 1.28 1998/02/15 12:06:09 bus Exp $";

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <pwd.h>

#include "log.h"
#include "utils.h"
#include "fileio.h"
#include "privilege.h"

#include "euracom.h"

enum TVerbindung { GEHEND=1, KOMMEND};
typedef char TelNo[33];

/* Aufbau Gebühreninfo */
struct GebuehrInfo {
  int    teilnehmer;    /* Interner Teilnehmer */
  TelNo  nummer;        /* Remote # */
  time_t datum_vst;     /* Datum/Zeit Verbindungsaufbau (von OVSt bzw. Euracom) */
  time_t datum_sys;     /* Datum/Zeit Eintrag (approx. Verbindungsende) */
  int    einheiten;     /* Anzahl verbrauchter Einheiten */
  enum TVerbindung art;
  float  betrag_base;   /* Betrag für eine EH */
  float  betrag;        /* Gesamtbetrag */
  char   waehrung[4];   /* Währungsbezeichnung */
};

static const char pid_file[] = PIDFILE;
static struct SerialFile *euracom_port;

/*------------------------------------------------------*/
/* BOOLEAN gebuehr_sys_log()                            */
/* */
/* Schreibt Gebuehrinfo auf syslog                      */
/* */
/* RetCode: Success/No success                          */
/*------------------------------------------------------*/
BOOLEAN gebuehr_sys_log(const struct GebuehrInfo *geb)
{
  switch (geb->art) {
    case GEHEND: {
      syslog(LOG_NOTICE, "%d called %s. %d unit%s = %.2f %s",
              geb->teilnehmer, 
              geb->nummer,
              geb->einheiten,
              (geb->einheiten?"s":""),
              geb->betrag,
              geb->waehrung);
    }
      break;

    case KOMMEND:
      if (geb->teilnehmer) {
        /* Mit Verbindung */
        if (geb->nummer[0]) {
          syslog(LOG_NOTICE, "Incoming call from %s for %d",
                  geb->nummer,
                  geb->teilnehmer);
        } else {
          syslog(LOG_NOTICE, "Incoming call for %d", 
            geb->teilnehmer);
        }       /* IF (unkown_no) */
      } else {
        /* Ohne Verbindung */
        if (geb->nummer[0]) { 
          syslog(LOG_NOTICE, "Unresponded incoming call from %s",
            geb->nummer);
          } else {
          syslog(LOG_NOTICE, "Unresponded incoming call");
        }       /* IF unkown_no (ELSE) */
      } /* IF (geb->teilnehmer) (ELSE) */
      break;
  }     /* SWITCH */

  return(TRUE);
}


/*------------------------------------------------------*/
/* BOOLEAN gebuehr_db_log()                             */
/* */
/* Writes charge data into database                     */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
BOOLEAN gebuehr_db_log(const struct GebuehrInfo *geb)
{
  char statement[2048]; /* SQL Statement */
  char date_fmt[21];	/* yyyy-mm-dd hh:mm:ss */

  sprintf(statement, "INSERT INTO euracom (int_no, remote_no, einheiten, direction, factor, pay, currency, vst_date, sys_date) values ('%d','%s',",
    geb->teilnehmer, 
    geb->nummer);

  switch (geb->art) {
    case GEHEND:
      strcatf(statement, "'%d', 'O', '%.2f', '%.2f', '%s'",
        geb->einheiten,
        geb->betrag_base,
        geb->betrag,
        geb->waehrung);
      break;

    case KOMMEND:
      strcatf(statement, "'', 'I', '', '', ''");
      break;
  } 

  /* Convert time/date specs into ISO 8601 strings */
  strftime(date_fmt, sizeof(date_fmt), "%Y-%m-%d %H:%M:%S", gmtime(&geb->datum_vst));
  strcatf(statement, ",'%s +00'", date_fmt);

  strftime(date_fmt, sizeof(date_fmt), "%Y-%m-%d %H:%M:%S", gmtime(&geb->datum_sys));
  strcatf(statement, ",'%s +00')", date_fmt);

  return(database_log(statement));
}


void conv_phone(char *dst, const char *src)
{
  /* ^00: International call. Just strip 00 to get int'l phone number */
  if (strncmp(src, "00", 2)==0) {
    sprintf(dst,"+%s", &src[2]);
  /* ^0: National call: Strip 0, add my contrycode, that's it */
  } elsif (strncmp(src, "0", 1)==0) {
    sprintf(dst, "%s%s", COUNTRYCODE, &src[1]);
  /* anything else: Prepend countrycode and local areacode to get IPN */
  } else {
    sprintf(dst, "%s%s", AREACODE, src);
  }
}


/*--------------------------------------------------------------------------
 * struct GebuehrInfo *eura2geb()
 *
 * Converts string (from euracom) into struct GebuehrInfo
 *
 * Inputs: String
 * RetCode: Ptr to struct (param 1); NULL: Error
 *------------------------------------------------------------------------*/
struct GebuehrInfo *eura2geb(struct GebuehrInfo *geb, const char *str)
{
  #define MAX_ARGS 6
  char *buf=strdup(str);	/* Make a copy */
  char *argv[MAX_ARGS];		/* Arguments */
  struct tm tm;
  int num;

  /* Split up input line, I hope short-circuit evaluation is what it's used to be  */
  for (num=0; ((num<MAX_ARGS) && 
               (argv[num]=strtok(num?NULL:buf, "|")) && 
               (stripblank(argv[num]))); num++);

  if ((num<3) || (num>5)) {
    log_msg(ERR_ERROR, "Got %d fields in input line", num);
    safe_free(buf);
    return(NULL);
  }

  /* 0: Art und Teilnehmer*/
  switch (*argv[0]++) {
    case 'G':
      geb->art=GEHEND;
      if (num==4) { log_debug(1, "Call seems to be free of charge."); }
      break;
    case 'V':
    case 'K':
      if (num!=3) { log_msg(ERR_WARNING, "Invalid # of elements (%d) in %s", num, str); }
      geb->art=KOMMEND;
      break;
    default:
      log_msg(ERR_ERROR, "Field 1 does not contain any class descriptor");
      safe_free(buf);
      return(NULL);
      break;
  }
  geb->teilnehmer=atoi(argv[0]);

  /* 1: Datum/Zeit Verbindungsaufbau */
  memset(&tm, 0, sizeof(tm));
  strptime(argv[1], "%d.%m.%y, %H:%M", &tm);
  tm.tm_isdst=-1;
  if ((geb->datum_vst=mktime(&tm))==(time_t)-1) {
    log_msg(ERR_WARNING, "Invalid time spec \"%s\". Using system time instead", argv[1]);
    geb->datum_vst=time(NULL);
  }

  /* 2: Telephone-# */
  if (str_isdigit(argv[2])) {
    conv_phone(geb->nummer, argv[2]);
  } else {
    strcpy(geb->nummer, "");    /* Null string */
  }

  /* 3: Einheiten */
  geb->einheiten=(num>3)?atoi(argv[3]):0;

  /* 4: Gebuehr */
  if (num>4) {
    char *cp;

    if ((cp=strchr(argv[4], ','))) {*cp='.';}
    if ((cp=strchr(argv[4], ' '))) {*cp='\0';}
    geb->betrag=(float)atof(argv[4]);
  } else {
    geb->betrag=0;
  }

  /* Default-Infos */
  strcpy(geb->waehrung, LOCAL_CURRENCY);
  geb->betrag_base=PRICE_PER_UNIT;

  /* Don't touch geb->datum_sys */
  safe_free(buf);
  return(geb);
}

/*--------------------------------------------------------------------------
 * BOOLEAN daemon_remove_pid_file()
 *
 * Removes PID file
 *
 * Inputs: -
 * RetCode: TRUE: O.k; FALSE: Error
 *------------------------------------------------------------------------*/
BOOLEAN daemon_remove_pid_file()
{
  log_debug(1, "Removing PID file %s", pid_file);
  return(delete_file(pid_file));
}

/*--------------------------------------------------------------------------
 * BOOLEAN daemon_create_pid_file()
 *
 * Creates PID file
 *
 * Inputs: -
 * RetCode: TRUE: O.k; FALSE: Error
 *------------------------------------------------------------------------*/
BOOLEAN daemon_create_pid_file()
{
  BOOLEAN may_create = TRUE;
  FILE *fp;

  log_debug(1, "Checking PID file status (%s)", pid_file);

  /* Is there already a PID file? */
  if ((fp=fopen(pid_file, "r"))) {
    pid_t pid;

    fscanf(fp, "%d", &pid);
    if (kill(pid, 0)) {
      log_msg(ERR_WARNING, "Stale PID file found in %s (pid %d)", pid_file, pid);
    } else {
      log_msg(ERR_CRIT, "Another euracom process seems to be active (pid %d)", pid);
      may_create=FALSE;
    }	/* IF kill */
    fclose(fp);
  }

  if (may_create) {
    FILE *fp;

    /* Create PID file */
    if ((fp=fopen(pid_file, "w"))) {
      pid_t pid = getpid();

      log_debug(1, "Creating PID file %s (%d)", pid_file, pid);
      fprintf(fp, "%d\n", pid);
      fclose(fp);
      return(TRUE);
    } else {
      log_msg(ERR_CRIT, "Cannot create %s: %s", pid_file, strerror(errno));
    }
  }
  return(FALSE);
}

/*------------------------------------------------------*/
/* int shutdown_program()                               */
/* */
/* Beendet programm                                     */
/* */
/* RetCode: No return                                   */
/*------------------------------------------------------*/
int shutdown_program(int force)
{
  if (privilege_active()) {
    privilege_enter_priv();
  }
  serial_close_device(euracom_port);	/* Close port */
  serial_deallocate_file(euracom_port);	/* Free structure */

  database_shutdown();			/* Disconnect gracefully */
  daemon_remove_pid_file();		/* Remove PID file */

  logger_shutdown();			/* Close log file/syslog */

  if (privilege_active()) {
    privilege_drop_priv();
  }
  exit(force);				/* Have a nice day */
}

/*------------------------------------------------------*/
/* int hangup()                                         */
/* */
/* Signal handler für non-fatal signals                 */
/* */
/* RetCode: 0                                           */
/*------------------------------------------------------*/
int hangup(int sig)
{
  signal(sig, (void *)hangup);	/* Restart signal handler */
  log_msg(ERR_INFO, "Signal %d received. Ignoring", sig);
  return(0);
}

/*------------------------------------------------------*/
/* int fatal()                                          */
/* */
/* Signal handler für fatal signals                     */
/* */
/* RetCode: 0                                           */
/*------------------------------------------------------*/
int fatal(int sig)
{
  log_msg(ERR_FATAL, "Signal %d received. Giving up", sig);
  shutdown_program(3);
  return(0);    /* Make compiler happy */
}

/*------------------------------------------------------*/
/* int terminate()                                      */
/* */
/* Signal handler für Programm-Beendigung               */
/* */
/* RetCode: 0                                           */
/*------------------------------------------------------*/
int terminate(int sig)
{
  log_msg(ERR_NOTICE, "Euracom exiting on signal %d", sig);
  shutdown_program(0);
  return(0);
}

/*------------------------------------------------------*/
/* int parse_euracom_data()                             */
/* */
/* Zeile von Euracom-Druckerport verarbeiten            */
/* */
/* RetCode: FALSE: Fehler, TRUE: O.k                    */
/*------------------------------------------------------*/
BOOLEAN parse_euracom_data(const char *buf)
{
  /* Check message type */
  unless (strchr(buf, '|')) {
    /* Send informational messages directly to syslog() */
    syslog(LOG_INFO, "%s", buf);
  } else {
    struct GebuehrInfo gebuehr;

    unless (eura2geb(&gebuehr, buf)) {
      syslog(LOG_ERR, "Invalid charge data: %s", buf);
      return(FALSE);
    }
    
    /* Aktuelle Zeit (GMT) einsetzen */
    gebuehr.datum_sys=time(NULL);

    /* Logging */
    gebuehr_sys_log(&gebuehr);
    gebuehr_db_log(&gebuehr);
  }    
  return(TRUE);
}

/*------------------------------------------------------*/
/* int select_loop()                                    */
/* */
/* Auf Daten von RS232 warten                           */
/* */
/* RetCode: Fehler                                      */
/*------------------------------------------------------*/
int select_loop()
{
  fd_set rfds;
  int euracom_fd = serial_query_fd(euracom_port);
  int retval;
 
  do {
    struct timeval tv;
 
    FD_ZERO(&rfds); FD_SET(euracom_fd, &rfds);
    tv.tv_sec=10; tv.tv_usec=0;
    retval=select(euracom_fd+1, &rfds, NULL, NULL, &tv);

    if (retval==0) {    /* Timeout */
      database_check_state();
    } elsif (retval>0) {
      if (FD_ISSET(euracom_fd, &rfds)) {
        char *buf;

        /* Data from Euracom Port */
        unless (buf=readln_rs232(euracom_port)) {
          log_msg(ERR_ERROR, "Euracom read failed. Ignoring line");
        } else {
          parse_euracom_data(buf);
        }
      } else {
        log_msg(ERR_FATAL, "Data on non-used socket");
      }
    }   /* IF retval */
  } while (retval>=0);

  log_msg(ERR_ERROR, "Select failed: %s", strerror(errno));
  return(0);
}

/*--------------------------------------------------------------------------
 * void usage()
 *
 * Prints some more or less useful information on how to start the programme
 *
 * Inputs: Name of executable
 *------------------------------------------------------------------------*/
void usage(const char *prg)
{
  printf("Usage: %s [options] device\n", prg);
  printf("\n  Database subsystem option:\n"\
         "  -H, --db-host=host           \tSets database host\n" \
         "  -D, --db-name=name           \tDatabase name to connect to\n" \
         "  -P, --db-port=port           \tSets database port number\n" \
         "  -R, --db-recovery-timeout=sec\tTime to stay in recovery mode\n" \
         "  -S, --db-shutdown-timeout=sec\tDisconnect after sec idle seconds\n" \
         "\n  Logging options:\n" \
         "  -l, --log-file=file          \tFile where to write log and debug messages into\n" \
         "  -p, --protocol-file=file     \tEnable logging all RS232 messages\n" \
         "  -d, --debug[=level]          \tSets debugging level\n" \
         "\n  Runtime options:\n" \
         "  -f, --no-daemon              \tDon't detach from tty and run in background\n" \
         "  -u, --run-as-user=#uid | name\tRun as user uid (or username, resp.)\n" \
         "\n  Misc:\n" \
         "  -h, --help                   \tYou currently look at it\n");
#if defined (DONT_CHECK_CTS)
  printf("\nCTS check has been disabled\n");
#endif
  exit(0);
}


/*--------------------------------------------------------------------------
 * int main()
 *
 * Main programme
 *
 * Inputs: argv, argc
 * RetCode: 0: Successful termination;
 *          1: Error in command line arguments
 *          2: Error during initialization
 *          3: Fatal errors
 *------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
  int opt;
  BOOLEAN no_daemon = FALSE;

  static const struct option long_options[] = {
    {"db-host", 1, NULL, 'H'},
    {"db-port", 1, NULL, 'P'},
    {"db-name", 1, NULL, 'D'},
    {"db-recovery-timeout", 1, NULL, 'R'},
    {"db-shutdown-timeout", 1, NULL, 'S'},

    {"log-file", 1, NULL, 'l'},
    {"protocol-file", 1, NULL, 'p'},
    {"debug", 2, NULL, 'd'},

    {"foreground", 0, NULL, 'f'},
    {"no-daemon", 0, NULL, 'f'},
    {"help", 0, NULL, 'h'},
    {"run-as-user", 1, NULL, 'u'},

    {NULL, 0, NULL, 0}
  };

  /* First time logging */
  logger_set_options(USE_STDERR | TIMESTAMP);
  logger_set_prefix("euracom");
  logger_set_level(0);	/* Debug level */
  logger_set_logfile(NULL);
  logger_initialize();

  euracom_port=serial_allocate_file();

  /* Parse command line options */
  while ((opt=getopt_long_only(argc, argv, "fhH:P:D:R:S:l:p:u:d::", long_options, NULL))!=EOF) {
    switch (opt) {
      /* Database subsystem */
      case 'H':
        database_set_host(optarg);
        break;
      case 'P':
        database_set_port(optarg);
        break;
      case 'D':
        database_set_db(optarg);
        break;
      case 'R':
        database_set_recovery_timeout(atoi(optarg));
        break;
      case 'S':
        database_set_shutdown_timeout(atoi(optarg));
        break;

      /* Logging subsystem */
      case 'l':
        logger_set_logfile(optarg);
        break;
      case 'd':
        logger_set_level(optarg?atoi(optarg):5);
        break;

      /* Serial subsystem */
      case 'p':
        serial_set_protocol_name(euracom_port, optarg);
        break;

      /* Main programme */
      case 'f':
        log_debug(2, "Program will not detach itself.");
        no_daemon=TRUE;
        break;
      case 'u':
        {
          if (geteuid()!=0) { 
            log_msg(ERR_WARNING, "Change of UID will not work unless the program is started by root");
          } else {
            struct passwd *pwd;
            
            /* If argument starts with # it's a numerical id */
            unless ((pwd=((optarg[0]=='#')?getpwuid(atoi(&optarg[1])):getpwnam(optarg)))) {
              log_msg(ERR_ERROR, "UID or username %s not found.", optarg);
              exit(1);
            }
            privilege_set_alternate_uid(pwd->pw_uid);
            privilege_set_alternate_gid(pwd->pw_gid);
            privilege_initialize();
          }
        }
        break;

      /* Fallback */
      case 'h':
      default:
        usage(argv[0]);
        exit(0);
        break;
    }     /* SWITCH */
  }     /* WHILE */

  /* Check command line arguments */
  if (optind==argc-1) {
    serial_set_device(euracom_port, argv[optind]);
  } else {
    log_msg(ERR_ERROR, "You must specify a device where to read from");
    usage(argv[0]);
    exit(1);
  }
  
  /* Set up signal handlers */
  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, (void *)terminate);
  signal(SIGQUIT, (void *)terminate);
  signal(SIGILL, (void *)fatal);
  signal(SIGTRAP, (void *)terminate);
  signal(SIGABRT, (void *)terminate);
  signal(SIGBUS, (void *)fatal);
  signal(SIGFPE, (void *)fatal);
  signal(SIGUSR1, SIG_IGN);
  signal(SIGSEGV, (void *)fatal);
  signal(SIGUSR2, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGALRM, SIG_IGN);
  signal(SIGTERM, (void *)terminate);
  signal(SIGSTKFLT, (void *)fatal);
  signal(SIGCHLD, SIG_IGN);

  /* Don't use exit() from here on.  shutdown_program() will do */
  unless (no_daemon) {
    logger_set_options(TIMESTAMP | USE_SYSLOG);
    unless (detach()) {
      shutdown_program(2);
    }
  }

  /* Files have permissions 644 */
  umask(022);

  /* Re-initialze logger to open logfile */
  logger_initialize();

  /* Write pid file. */
  unless (daemon_create_pid_file()) {
    log_msg(ERR_FATAL, "Error writing PID file");
    shutdown_program(2);
  }

  /* Open euracom port */
  unless (serial_open_device(euracom_port)) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    shutdown_program(2);
  }
 
  /* Since all required files are open (or created), we can safely drop privileges */ 
  if (privilege_active()) {
    unless (privilege_leave_priv()) {
      log_msg(ERR_FATAL, "Could not leave privileged section");
      shutdown_program(2);
    }
  }

  unless (database_initialize()) {
    log_msg(ERR_FATAL, "Error initializing database subsystem");
    shutdown_program(2);
  }

  /* Open syslog facility */
  closelog();	/* Clear logger syslog settings */
  openlog("euracom", 0, DEF_LOGFAC);

  /* Read data */
  log_msg(ERR_INFO, "Euracom %s listening on %s", VERSION, euracom_port->fd_device);
  select_loop();

  /* Shutdown gracefully */
  shutdown_program(0);
  return(0);
}
