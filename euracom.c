/* Euracom 18x, Gebührenerfassung
   Copyright (C) 1996-1997 MB Computrex

   Released under GPL

   Michael Bussmann <bus@fgan.de>

   $Id: euracom.c,v 1.11 1997/07/26 07:35:39 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/euracom.c,v $
 */

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include "euracom.h"
#include "log.h"
#include "utils.h"
#include "fileio.h"


#define DEFAULT_DEVICE		"/dev/ttyS0"
#define GEBUEHR_FILE		"/var/lib/euracom/gebuehr.dat"
#define PROTOCOL_FILE		"/var/lib/euracom/protocol.dat"
#define DEF_LOGFAC		LOG_LOCAL0

#define UNKNOWN_TEXT_EURA	"Rufnr.unbekannt"
#define	UNKNOWN_NO		"???"


/* Globals */
int euracom_fd = 0;			/* RS232 port to Euracom */
char *gebuehr_filename = NULL;		/* Filename für Gebuehrendaten */
char *proto_filename = NULL;		/* Filename für Euracom-backup file */
extern char *readln_rs232();


/*------------------------------------------------------*/
/* BOOLEAN gebuehr_log()                                */
/* */
/* Schreibt Gebuehrinfo auf disk/syslog                 */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
BOOLEAN gebuehr_log(const struct GebuehrInfo *geb)
{
  char res[1024];
  struct FQTN fqtn;
  BOOLEAN unknown_no = FALSE;

  /* Logfile öffnen */
  if (gebuehr_filename) {
    FILE *fp;

    if (fp=fopen(gebuehr_filename, "a")) {
      struct tm *tm_sys = localtime(&geb->datum_sys);
      struct tm *tm_vst = localtime(&geb->datum_vst);
      char t_1[30], t_2[30];

      strftime(t_1, 15, "%b %d %X %Y\0", tm_sys);
      strftime(t_2, 15, "%H:%M\0", tm_vst);
      
      fprintf(fp, "%s | %2d |%.16s | %ld | %s | %4d |%c|%.2f|%s| %.2f|\n",
	      t_1,
	      geb->teilnehmer,
	      geb->nummer,
	      (unsigned long)geb->datum_sys,
	      t_2,
	      geb->einheiten,
	      (geb->art==GEHEND?'O':'I'),
	      (geb->betrag_base),
	      (geb->waehrung),
	      geb->betrag);
      fclose(fp);
    } else {
      log_msg(ERR_CRIT, "Could not open logfile: %s", strerror(errno));
    }
  }

  /* Open databases */
  if (!openDB(AVON_DB_NAME, WKN_DB_NAME)) {
    log_msg(ERR_CRIT, "Could not open databases");
  }

  /* Syslog meldung */
  strcpy(res, "");
  unknown_no=(strcmp(geb->nummer, UNKNOWN_NO)==0);

  if (!unknown_no) {
    lookup_number(geb->nummer, &fqtn);
  }
  switch (geb->art) {
    case GEHEND: {
      TelNo telno;

      convert_telno(telno, &fqtn);
      sprintf(res, "%d called %s. %d units = %.2f DM",
	      geb->teilnehmer, telno,
	      geb->einheiten,
	      geb->betrag);
    }
      break;
    case KOMMEND:
      if (unknown_no) {
	if (geb->teilnehmer) {
	  sprintf(res, "Incoming call for %d",
		  geb->teilnehmer);
	} else {
	  sprintf(res, "Incoming call but no connection");
	}
      } else {
	TelNo telno;

	convert_telno(telno, &fqtn);
	if (geb->teilnehmer) {
       	  sprintf(res, "Incoming call from %s for %d",
	      telno, geb->teilnehmer);
	} else {
          sprintf(res, "Incoming call from %s. No connection",
		  telno);
	}
      }
      break;

    default:
      log_msg(ERR_WARNING, "CASE missing in geb->art");
      break;
  }	/* SWITCH */

  syslog(LOG_NOTICE, "%s", res);
  
  /* Close DB */
  closeDB();

  return(TRUE);
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
  char teiln[25];

  /* Art und Teilnehmer*/
  if (!(tok=strtok(buf, "|"))) { 
    log_msg(ERR_ERROR, "Error extracting first token");
    return(NULL);
  }
  if (!(cp=strpbrk(tok, "GVK"))) { 
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
  if (!(tok=strtok(NULL, "|"))) { log_msg(ERR_ERROR, "Field 3 invalid"); return(NULL); }
  stripblank(tok);
  if (strcasecmp(tok, UNKNOWN_TEXT_EURA)==0) {
    strcpy(geb->nummer, UNKNOWN_NO);
  } else {
    strcpy(geb->nummer, tok);
  }

  if (geb->art==KOMMEND) {
    geb->einheiten=0;
    geb->betrag=0;
  } else {
    /* Einheiten */
    if (!(tok=strtok(NULL, "|"))) {log_msg(ERR_ERROR, "Field 4 invalid");  return(NULL); }
    geb->einheiten=atoi(tok);

    /* Gebuehr */
    if (!(tok=strtok(NULL, "|"))) { log_msg(ERR_ERROR, "Field 5 invalid"); return(NULL); }
    stripblank(tok);
    { char *cp;
      if (cp=strchr(tok, ',')) {*cp='.';}
      if (cp=strchr(tok, ' ')) {*cp='\0';}
    }
    sscanf(tok, "%f", &geb->betrag);
  }

  free(buf);

  /* Default-Infos */
  strcpy(geb->waehrung, "DM");
  geb->betrag_base=0.12;

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
  close_euracom_port();
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
  printf("Usage: %s [-gp] [device]\n", prg);
  printf("\t-g file\tGebührenfile\n");
  printf("\t-p file\tProtokollfile\n");
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
  if (proto_filename) {
    FILE *fp = fopen(proto_filename, "a");
    
    if (fp) { 
      fprintf(fp, "%s\n", buf); 
      fclose(fp); 
    }
  }

  /* Check message type */
  if (!strchr(buf, '|')) {
    /* Junk daten */
    syslog(LOG_INFO, "%s", buf);
    return(TRUE);
  } else {
    struct GebuehrInfo gebuehr;
    FILE *fp;

    if (!eura2geb(&gebuehr, buf)) {
      syslog(LOG_ERR, "Invalid charge data: %s", buf);
      return(FALSE);
    }
    
    /* Aktuelle Zeit einsetzen */
    gebuehr.datum_sys=time(NULL);

    /* Logging */
    gebuehr_log(&gebuehr);
  }    
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
  int retval;
 
  FD_ZERO(&rfds);
  FD_SET(euracom_fd, &rfds);

  if (euracom_fd>max_fd) { max_fd=euracom_fd; }

  max_fd++;
  do {
    struct timeval tv;
 
    FD_ZERO(&rfds);
    FD_SET(euracom_fd, &rfds);
    tv.tv_sec=10; tv.tv_usec=0;
    retval=select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

    if (retval>0) {
      if (FD_ISSET(euracom_fd, &rfds)) {
	char *buf;

	/* Data from Euracom Port */
	if (!(buf=readln_rs232(euracom_fd))) {
	  log_msg(ERR_ERROR, "Euracom read failed. Ignoring line");
	} else {
	  parse_euracom_data(buf);
	}
      } else {
	log_msg(ERR_FATAL, "Data on non-used socket");
      }
    }	/* IF retval */
  } while (retval!=-1);

  log_msg(ERR_ERROR, "Select failed: %s", strerror(errno));
  return(0);
}
	

int main(argc, argv)
  int argc;
  char **argv;
{
  extern char *optarg;
  extern int optind;
  int opt;

  char devname[1024];

  /* Logging */
  init_log("Euracom", ERR_JUNK, USE_STDERR | TIMESTAMP, NULL);

  /* Parse command line options */
  while ((opt = getopt(argc, argv, "g:p:")) != EOF) {
    switch (opt) {
      case 'g':
	gebuehr_filename=strdup(optarg);
        break;
      case 'p':
	proto_filename=strdup(optarg);
        break;
      default:
        usage(argv[0]);
        exit(0);
        break;
    }     /* SWITCH */
  }	/* WHILE */

  /* Check command line arguments */
  if (optind==argc-1) {
    strcpy(devname, argv[optind]);
  } else {
    strcpy(devname, DEFAULT_DEVICE);
  }

  /* Check logfiles */
  if (!gebuehr_filename) { gebuehr_filename=strdup(GEBUEHR_FILE); }
  if (!proto_filename)	{ proto_filename=strdup(PROTOCOL_FILE); }

  /* Initialize Euracom */
  if ((euracom_fd=init_euracom_port(devname))==-1) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    exit(1);
  }

  /* Re-open syslog facility */
  openlog("Euracom", 0, DEF_LOGFAC);

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
  signal(SIGCONT, SIG_IGN);
  signal(SIGSTOP, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  select_loop();

  /* Shutdown gracefully */
  shutdown_program(0);
  return(0);
  
}
