/***************************************************************************
 * euracom -- Euracom 18x Gebührenerfassung
 *
 * euracom.c -- Main programme
 *
 * Copyright (C) 1996-1997 by Michael Bussmann
 *
 * Authors:             Michael Bussmann <bus@fgan.de>
 * Created:             1996-10-09 17:31:56 GMT
 * Version:             $Revision: 1.17 $
 * Last modified:       $Date: 1997/10/09 13:49:04 $
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

static char rcsid[] = "$Id: euracom.c,v 1.17 1997/10/09 13:49:04 bus Exp $";

#include "config.h"

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <assert.h>

#include "log.h"
#include "utils.h"
#include "fileio.h"

#include "euracom.h"

enum TVerbindung { FEHLER, GEHEND, KOMMEND};
typedef char TelNo[33];

/* Aufbau Gebühreninfo */
struct GebuehrInfo {
  int    teilnehmer;    /* Interner Teilnehmer */
  TelNo  nummer;        /* Remote # */
  time_t datum_vst;     /* Datum/Zeit Verbindungsaufbau (von VSt) */
  time_t datum_sys;     /* Datum/Zeit Eintrag (approx. Verbindungsende) */
  int    einheiten;     /* Anzahl verbrauchter Einheiten */
  enum TVerbindung art;
  float  betrag_base;   /* Betrag für eine EH */
  float  betrag;        /* Gesamtbetrag */
  char   waehrung[4];   /* Währungsbezeichnung */
};

/*------------------------------------------------------*/
/* BOOLEAN gebuehr_sys_log()                            */
/* */
/* Schreibt Gebuehrinfo auf syslog                      */
/* */
/* RetCode: Success/No success                          */
/*------------------------------------------------------*/
BOOLEAN gebuehr_sys_log(const struct GebuehrInfo *geb)
{
  char res[1024] = "";

  switch (geb->art) {
    case GEHEND: {
      sprintf(res, "%d called %s. %d units = %.2f %s",
	      geb->teilnehmer, 
	      geb->nummer,
	      geb->einheiten,
	      geb->betrag,
	      geb->waehrung);
    }
      break;

    case KOMMEND:
      if (geb->teilnehmer) {
        /* Mit Verbindung */
        if (geb->nummer[0]) {
	  sprintf(res, "Incoming call from %s for %d",
	          geb->nummer,
		  geb->teilnehmer);
	} else {
	  sprintf(res, "Incoming call for %d", 
	    geb->teilnehmer);
        }	/* IF (unkown_no) */
      } else {
      	/* Ohne Verbindung */
        if (geb->nummer[0]) { 
       	  sprintf(res, "Unresponded incoming call from %s",
	    geb->nummer);
	  } else {
          sprintf(res, "Unresponded incoming call");
	}	/* IF unkown_no (ELSE) */
      }	/* IF (geb->teilnehmer) (ELSE) */
      break;

    default:
      log_msg(ERR_WARNING, "CASE missing in geb->art");
      break;
  }	/* SWITCH */

  syslog(LOG_NOTICE, "%s", res);
  
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
  char buf1[4096]; /* Attributes */
  char buf2[4096]; /* Final statement */
  char vst_date[35], sys_date[35];
  struct tm *tm;

  /* Convert time/date specs */
  tm=localtime(&geb->datum_vst);
  strftime(vst_date, 30, "%d %b %Y %H:%M:%S", tm);

  tm=localtime(&geb->datum_sys);
  strftime(sys_date, 30, "%d %b %Y %H:%M:%S", tm);

  switch (geb->art) {
    case GEHEND:
      sprintf(buf1, "'%d', 'O', '%.2f', '%.2f', '%s'",
	geb->einheiten,
	geb->betrag_base,
	geb->betrag,
	geb->waehrung);
      break;

    case KOMMEND:
      strcpy(buf1, "'', 'I', '', '', ''");
      break;

    case FEHLER:
    default:
      log_msg(ERR_CRIT, "CASE fehler not handled!");
      break;
  } 

  sprintf(buf2, "INSERT into euracom (int_no, remote_no, vst_date, sys_date, einheiten, direction, factor, pay, currency) values ('%d','%s','%s','%s', %s);", 
    geb->teilnehmer,
    geb->nummer,
    vst_date, sys_date,
    buf1);

  return (database_log(buf2));
}


void conv_phone(char *dst, char *src)
{
  /* ^00: International call. Just strip 00 to get int'l phone number */
  if (strncmp(src, "00", 2)==0) {
    strcpy(dst, "+"); strcat(dst, &src[2]);
  /* ^0: National call: Strip 0, add my contrycode, that's it */
  } elsif (strncmp(src, "0", 1)==0) {
    strcpy(dst, COUNTRYCODE); strcat(dst, &src[1]);
  /* anything else: Prepend countrycode and local areacode to get IPN */
  } else {
    strcpy(dst, AREACODE); strcat(dst, src);
  }
}


/*------------------------------------------------------*/
/* struct GebuehrInfo *eura2geb()                       */
/* */
/* Wandelt String von Euracom in GebuehrInfo struct um  */
/* */
/* RetCode: Ptr to param 1, NULL: Error                 */
/*------------------------------------------------------*/
struct GebuehrInfo *eura2geb(struct GebuehrInfo *geb, const char *str)
{
  char *tok, *cp;
  char *buf=strdup(str);

  /* Art und Teilnehmer*/
  unless (tok=strtok(buf, "|")) { 
    log_msg(ERR_ERROR, "Error extracting first token");
    return(NULL);
  }
  unless (cp=strpbrk(tok, "GVK")) { 
    log_msg(ERR_ERROR, "Field 1 does not contain any char of G,V,K");
    return(NULL);
  }

  switch (*cp++) {
    case 'G':
      geb->art=GEHEND;
      break;
    case 'V':
    case 'K':
      geb->art=KOMMEND;
      break;
    default:
      geb->art=-1;
      log_msg(ERR_CRIT, "Field 1 descriptor missing. Can't happen");
      return(NULL);
      break;
  }
  geb->teilnehmer=atoi(cp);

  /* Datum/Zeit Verbindungsaufbau */
  if ((tok=strtok(NULL, "|"))) {
    struct tm tm;

    stripblank(tok);
    strptime(tok, "%d.%m.%y, %H:%M", &tm);
    tm.tm_sec=tm.tm_yday=tm.tm_wday=0;
    tm.tm_isdst=-1;
    geb->datum_vst=mktime(&tm);
  } else {
    log_msg(ERR_ERROR, "Field 2 invalid"); 
    return(NULL); 
  }

  /* Telefonnummer */
  unless (tok=strtok(NULL, "|")) { log_msg(ERR_ERROR, "Field 3 invalid"); return(NULL); }
  stripblank(tok);
  if (strcasecmp(tok, UNKNOWN_TEXT_EURA)==0) {
    strcpy(geb->nummer, "");	/* Null string */
  } else {
    conv_phone(geb->nummer, tok);
  }

  if (geb->art==KOMMEND) {
    geb->einheiten=0;
    geb->betrag=0;
  } else {
    /* Einheiten */
    unless (tok=strtok(NULL, "|")) {log_msg(ERR_ERROR, "Field 4 invalid");  return(NULL); }
    geb->einheiten=atoi(tok);

    /* Gebuehr */
    unless (tok=strtok(NULL, "|")) { log_msg(ERR_ERROR, "Field 5 invalid"); return(NULL); }
    stripblank(tok);
    { char *cp;
      if ((cp=strchr(tok, ','))) {*cp='.';}
      if ((cp=strchr(tok, ' '))) {*cp='\0';}
    }
    sscanf(tok, "%f", &geb->betrag);
  }

  free(buf);

  /* Default-Infos */
  strcpy(geb->waehrung, LOCAL_CURRENCY);
  geb->betrag_base=PRICE_PER_UNIT;

  /* geb->datum_sys wird *nicht* gesetzt! */
  return(geb);
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
  serial_shutdown();
  database_shutdown();
  close_log();
  exit(force);
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
  signal(sig, (void *)hangup);
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
  shutdown_program(1);
  return(0);
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
  log_msg(ERR_INFO, "Signal %d received. Program terminating", sig);
  shutdown_program(0);
  return(0);
}

/*------------------------------------------------------*/
/* void usage()                                         */
/* */
/* Infos zur Programmbedienung                          */
/*------------------------------------------------------*/
void usage(const char *prg)
{
  printf("Usage: %s [options] device\n", prg);
  printf("\t-H, --db-host = host           \tSets database host\n" \
         "\t-P, --db-port = port           \tSets database port number\n" \
	 "\t-N, --db-name = name           \tDatabase to connect\n" \
	 "\t-R, --db-recovery-timeout = sec\tTime to stay in recovery mode\n" \
	 "\t-S, --db-shutdown-timeout = sec\tDisconnect after sec idle seconds\n" \
	 "\t-p, --protocol-file = file     \tEnable logging all rs232 messages\n" \
	 "\t-v, --verbose [= level]        \tSets verbosity level\n" \
	 "\t-h, --help                     \tYou currently look at it\n");

  exit(0);
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
    return(TRUE);
  } else {
    struct GebuehrInfo gebuehr;

    unless (eura2geb(&gebuehr, buf)) {
      syslog(LOG_ERR, "Invalid charge data: %s", buf);
      return(FALSE);
    }
    
    /* Aktuelle Zeit einsetzen */
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
  int max_fd=0;
  int euracom_fd = serial_query_fd();
  int retval;
 
  FD_ZERO(&rfds);
  FD_SET(euracom_fd, &rfds);

  assert(euracom_fd!=0);
  if (euracom_fd>max_fd) { max_fd=euracom_fd; }

  max_fd++;
  do {
    struct timeval tv;
 
    FD_ZERO(&rfds);
    FD_SET(euracom_fd, &rfds);
    tv.tv_sec=10; tv.tv_usec=0;
    retval=select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

    if (retval==0) {	/* Timeout */
      database_check_state();
    } elsif (retval>0) {
      if (FD_ISSET(euracom_fd, &rfds)) {
	char *buf;

	/* Data from Euracom Port */
	unless (buf=readln_rs232(euracom_fd)) {
	  log_msg(ERR_ERROR, "Euracom read failed. Ignoring line");
	} else {
	  parse_euracom_data(buf);
	}
      } else {
	log_msg(ERR_FATAL, "Data on non-used socket");
      }
    }	/* IF retval */
  } while (retval>=0);

  log_msg(ERR_ERROR, "Select failed: %s", strerror(errno));
  return(0);
}
	

int main(argc, argv)
  int argc;
  char **argv;
{
  int opt;
  int option_index = 0;
  struct option long_options[] = {
    {"db-host", 1, NULL, 'H'},
    {"db-port", 1, NULL, 'P'},
    {"db-name", 1, NULL, 'N'},
    {"db-recovery-timeout", 1, NULL, 'R'},
    {"db-shutdown-timeout", 1, NULL, 'S'},
    {"protocol-file", 1, NULL, 'p'},
    {"verbose", 2, NULL, 'v'},
    {NULL, 0, NULL, 0}
  };

  /* Logging */
  init_log("Euracom", ERR_JUNK, USE_STDERR | TIMESTAMP, NULL);

  /* Parse command line options */
  while ((opt = getopt_long_only(argc, argv, "hH:P:N:R:S:p:v::", long_options, &option_index)) != EOF) {
    switch (opt) {
      /* Database subsystem */
      case 'H':
        database_set_host(optarg);
	break;
      case 'P':
        database_set_port(optarg);
	break;
      case 'N':
        database_set_db(optarg);
	break;
      case 'R':
        database_set_recovery_timeout(atoi(optarg));
	break;
      case 'S':
        database_set_shutdown_timeout(atoi(optarg));
	break;

      /* Standard main options */
      case 'v':
        break;
      case 'p':
	serial_set_protocol_name(optarg);
        break;

      /* Fallback */
      default:
        usage(argv[0]);
        exit(0);
        break;
    }     /* SWITCH */
  }	/* WHILE */

  /* Check command line arguments */
  if (optind==argc-1) {
    serial_set_device(argv[optind]);
  } else {
    usage(argv[0]);
    exit(1);
  }

  /* Now that variables should be set, initialize every subsystem */
  unless(serial_initialize()) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    exit(1);
  }

  unless (database_initialize()) {
    log_msg(ERR_FATAL, "Error initializing database subsystem");
    exit(1);
  }

  /* Open syslog facility */
  openlog("euracom", 0, DEF_LOGFAC);

  /* Set up signal handlers */
  signal(SIGHUP,(void *)hangup);
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

  /* Read data */
  select_loop();

  /* Shutdown gracefully */
  shutdown_program(0);
  return(0);
}

