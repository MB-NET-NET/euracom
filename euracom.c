/* Euracom 18x, Gebührenerfassung
   Copyright (C) 1996-1997 MB Computrex

   Released under GPL

   Michael Bussmann <bus@fgan.de>

   $Id: euracom.c,v 1.12 1997/08/28 09:30:43 bus Exp $
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

#ifdef TEST_ONLY
#define DEFAULT_DEVICE		"/dev/stdin"
#define PROTOCOL_FILE		"/tmp/protocol.dat"
#define DEF_LOGFAC		LOG_LOCAL0
#else
#define DEFAULT_DEVICE		"/dev/ttyS0"
#define PROTOCOL_FILE		"/var/lib/euracom/protocol.dat"
#define DEF_LOGFAC		LOG_LOCAL0
#endif

#define UNKNOWN_TEXT_EURA	"Rufnr.unbekannt"
#define	UNKNOWN_NO		"???"

/* Globals */
int euracom_fd = 0;			/* RS232 port to Euracom */
char *proto_filename = NULL;		/* Filename für Euracom-backup file */
extern char *readln_rs232();


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
  BOOLEAN unknown_no = (strcmp(geb->nummer, UNKNOWN_NO)==0);

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
        if (unknown_no) {
	  sprintf(res, "Incoming call for %d", 
	    geb->teilnehmer);
	} else {
	  sprintf(res, "Incoming call from %s for %d",
	          geb->nummer,
		  geb->teilnehmer);
        }
      } else {
        /* Ohne Verbindung */
	if (unknown_no) {
          sprintf(res, "Unresponded incoming call");
	} else {
       	  sprintf(res, "Unresponded incoming call from %s",
	    geb->nummer);
	}
      }
      break;

    default:
      log_msg(ERR_WARNING, "CASE missing in geb->art");
      break;
  }	/* SWITCH */

#ifndef TEST_ONLY
  syslog(LOG_NOTICE, "%s", res);
#endif
  
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
  char vst_date[20], vst_time[20], sys_date[20], sys_time[20];
  struct tm *tm;

  /* Convert time/date specs */
  tm=localtime(&geb->datum_vst);
  strftime(vst_date, 15, "%d %b %Y", tm);
  strftime(vst_time, 15, "%H:%M:%S", tm);

  tm=localtime(&geb->datum_sys);
  strftime(sys_date, 15, "%d %b %Y", tm);
  strftime(sys_time, 15, "%H:%M:%S", tm);

  switch (geb->art) {
    case GEHEND:
      sprintf(buf1,"'%d', '%s', '%s', '%s', '%s', '%s', '%d', '%c', '%.3f', '%.3f', '%s'",
        geb->teilnehmer,
	geb->nummer,
	vst_date, vst_time, sys_date, sys_time,
	geb->einheiten,
	'G',
	geb->betrag_base,
	geb->betrag,
	geb->waehrung);
      break;

    case KOMMEND:
      sprintf(buf1,"'%d', '%s', '%s', '%s', '%s', '%s', '', '%c', '', '', ''",
        geb->teilnehmer,
	geb->nummer,
	vst_date, vst_time, sys_date, sys_time,
	(geb->teilnehmer?'V':'K'));
      break;
  } 

  sprintf(buf2, "INSERT into test (int_no, remote_no, vst_date, vst_time, sys_date, sys_time, einheiten, geb_art, factor, pay, currency) values (%s);", buf1);

  return (database_log(buf2));
}


void conv_phone(char *dst, char *src)
{
  if (strncmp(src, "00", 2)==0) {
    strcpy(dst, "+"); strcat(dst, &src[2]);
  } elsif (strncmp(src, "0", 1)==0) {
    strcpy(dst, "+49"); strcat(dst, &src[1]);
  } else {
    strcpy(dst, "+492364"); strcat(dst, src);
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
  char teiln[25];

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
    strcpy(geb->nummer, UNKNOWN_NO);
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
  /* Make a copy of everything into procol file */
  if (proto_filename) {
    FILE *fp = fopen(proto_filename, "a");
    
    if (fp) { 
      fprintf(fp, "%s\n", buf); 
      fclose(fp); 
    }
  }

  /* Check message type */
  unless (strchr(buf, '|')) {
    /* Junk daten */
#ifndef TEST_ONLY
    syslog(LOG_INFO, "%s", buf);
#endif
    return(TRUE);
  } else {
    struct GebuehrInfo gebuehr;
    FILE *fp;

    unless (eura2geb(&gebuehr, buf)) {
#ifndef TEST_ONLY
      syslog(LOG_ERR, "Invalid charge data: %s", buf);
#endif
      return(FALSE);
    }
    
    /* Aktuelle Zeit einsetzen */
    gebuehr.datum_sys=time(NULL);

    /* Logging */
    gebuehr_sys_log(&gebuehr);
    gebuehr_db_log(&gebuehr);
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

    if (retval==0) {	/* Timeout */
      database_check_state();
    } elsif (retval>0) {
      if (FD_ISSET(euracom_fd, &rfds)) {
	char *buf;

#ifndef TEST_ONLY
	/* Data from Euracom Port */
	unless (buf=readln_rs232(euracom_fd)) {
#else
	unless (buf=fgetline(stdin, NULL)) {
#endif
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
  extern char *optarg;
  extern int optind;
  int opt;

  char devname[1024] = DEFAULT_DEVICE;

  /* Logging */
  init_log("Euracom", ERR_JUNK, USE_STDERR | TIMESTAMP, NULL);

  /* Parse command line options */
  while ((opt = getopt(argc, argv, "p:")) != EOF) {
    switch (opt) {
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
  }

  /* Check logfiles */
  unless (proto_filename) { proto_filename=strdup(PROTOCOL_FILE); }

#ifndef TEST_ONLY
  /* Initialize Euracom */
  if ((euracom_fd=init_euracom_port(devname))==-1) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    exit(1);
  }
#else
  euracom_fd=0;
#endif

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

  /* Initialize database */
  database_initialize(NULL, NULL, NULL, NULL);

  /* Read data */
  select_loop();

  /* Shutdown gracefully */
  shutdown_program(0);
  return(0);
  
}
