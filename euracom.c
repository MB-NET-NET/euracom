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
#include "log.h"
#include "utils.h"
#include "fileio.h"

enum TVerbindung { FEHLER=0, GEHEND, KOMMEND, VERBINDUNG};
typedef char TelNo[32];
struct GebuehrInfo {
  enum TVerbindung art;
  int teilnehmer;
  time_t datum;	/* Datum/Zeit Verbindungsaufbau */
  time_t doe;	/* Datum/Zeit Eintrag (approx. Verbindungsende) */
  TelNo nummer;
  int einheiten;
  float betrag;
};

#define DEFAULT_DEVICE		"/dev/cua0"
#define GEBUEHR_FILE		"/var/log/euracom.gebuehr"
#define PROTOCOL_FILE		"/var/log/euracom.protocol"
#define DEF_LOGFAC		LOG_LOCAL0

#define UNKNOWN_TEXT_EURA	"Rufnr.unbekannt"
#define	UNKNOWN_NO		"???"


/* Globals */
int euracom_fd = 0;			/* RS232 port to Euracom */
char *gebuehr_filename = NULL;		/* Filename für Gebuehrendaten */
char *proto_filename = NULL;		/* Filename für Euracom-backup file */

extern char *readln_rs232();


/*------------------------------------------------------*/
/* unsigned int guess_duration()                        */
/* */
/* RetCode: Estimated duration in secs                  */
/*------------------------------------------------------*/
unsigned int guess_duration(const struct GebuehrInfo *geb)
{
  unsigned int sec = (unsigned int)((geb->doe)-(geb->datum));

  return(sec);
}


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
  char art;

  /* Logfile öffnen */
  if (gebuehr_filename) {
    FILE *fp;

    if (fp=fopen(gebuehr_filename, "a")) {
      strcpy(res, "");
      fprintf(fp, "%d;%lu;%lu;%d;%s;%d;%7.2f\n",
	      geb->art,
	      (unsigned long)geb->datum, (unsigned long)geb->doe,
	      geb->teilnehmer,
	      geb->nummer,
	      geb->einheiten,
	      geb->betrag);
      fclose(fp);
    } else {
      log_msg(ERR_CRIT, "Could not open logfile: %s", strerror(errno));
    }
  }
      
  /* Syslog meldung */
  strcpy(res, "");
  switch (geb->art) {
    case FEHLER:
      log_msg(ERR_WARNING, "Invalid geb->art");
      return(FALSE);
      break;
    case GEHEND:
      sprintf(res, "%d called %s. %d units = %6.2f DM (%u s)",
	      geb->teilnehmer, geb->nummer,
	      geb->einheiten,
	      geb->betrag,
	      guess_duration(geb));
      break;
    case KOMMEND:
      if (strcmp(geb->nummer, UNKNOWN_NO)==0) {
        sprintf(res, "Incoming call but no connection (%u s)",
	      guess_duration(geb));
      } else {
        sprintf(res, "Incoming call from %s. No connection (%u s)",
	      geb->nummer,
	      guess_duration(geb));
      }
      break;
    case VERBINDUNG:
      if (strcmp(geb->nummer, UNKNOWN_NO)==0) {
      	sprintf(res, "Incoming call for %d (%u s)",
      		geb->teilnehmer,
      		guess_duration(geb));
      } else {
      	sprintf(res, "Incoming call from %s for %d (%u s)",
	      geb->nummer, geb->teilnehmer,
	      guess_duration(geb));
      }
      break;
    default:
      log_msg(ERR_WARNING, "CASE missing in geb->art");
      break;
  }	/* SWITCH */

  syslog(LOG_NOTICE, "%s", res);
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
  char *tok;
  char *buf=strdup(str);
  char art[3];
  char teiln[25];

  /* Art und Teilnehmer*/
  if (!(tok=strtok(buf, "|"))) { return(NULL); }
  geb->teilnehmer=0;
  sscanf(tok, "%s %s", art, teiln);
  geb->teilnehmer=atoi(teiln);
  switch (art[0]) {
    case 'G':
      geb->art=GEHEND;
      break;
    case 'V':
      geb->art=VERBINDUNG;
      break;
    case 'K':
      geb->art=KOMMEND;
      break;
    default:
      geb->art=FEHLER;
      log_msg(ERR_ERROR, "Field 1 invalid");
      return(NULL);
      break;
  }

  /* Datum/Zeit Verbindungsaufbau */
  if ((tok=strtok(NULL, "|"))) {
    struct tm tm;

    stripblank(tok);
    strptime(tok, "%d.%m.%y, %H:%M", &tm);
    tm.tm_sec=tm.tm_yday=tm.tm_wday=0;
    tm.tm_isdst=-1;
    geb->datum=mktime(&tm);
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

  if ((geb->art==KOMMEND) || (geb->art==VERBINDUNG)) {
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
  /* geb->doe wird *nicht* gesetzt! */
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
  log_msg(ERR_NOTICE, "Signal %d received", sig);
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
    gebuehr.doe=time(NULL);

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
