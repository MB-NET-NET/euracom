#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <signal.h>
#include <termios.h>
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

/* Globals */
int euracom_fd = 0;			/* RS232 port to Euracom */
char *gebuehr_filename = NULL;		/* Filename f�r Gebuehrendaten */



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

  /* Logfile �ffnen */
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
      sprintf(res, "%d called %s. %d units (%lu s), %6.2f DM",
	      geb->teilnehmer, geb->nummer,
	      geb->einheiten,
	      (unsigned long)((geb->doe)-(geb->datum)),
	      geb->betrag);
      break;
    case KOMMEND:
      sprintf(res, "Incoming call from %s. No connection (%lu s)",
	      geb->nummer,
	      (unsigned long)((geb->doe)-(geb->datum)));
      break;
    case VERBINDUNG:
      sprintf(res, "Incoming call from %s to %d for %lu s",
	      geb->nummer, geb->teilnehmer,
	      (unsigned long)((geb->doe)-(geb->datum)));
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
  struct tm tm;

  /* Art und Teilnehmer*/
  if (!(tok=strtok(buf, "|"))) { return(NULL); }
  sscanf(tok, "%s %d", art, &geb->teilnehmer);
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
  if (!(tok=strtok(NULL, "|"))) { log_msg(ERR_ERROR, "Field 2 invalid"); return(NULL); }
  strptime(tok, " %d.%m.%y, %H:%M", &tm);
  geb->datum=mktime(&tm);

  /* Telefonnummer */
  if (!(tok=strtok(NULL, "|"))) { log_msg(ERR_ERROR, "Field 3 invalid"); return(NULL); }
  stripblank(tok);
  strcpy(geb->nummer, tok);

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
/* int init_euracom_port()                              */
/* */
/* �ffnet RS232 device, setzt RTS                       */
/* */
/* RetCode: fd, -1: Error                               */
/*------------------------------------------------------*/
int init_euracom_port(const char *device)
{
  struct termios term;
  int fd;
  int flags;

  log_msg(ERR_DEBUG, "Initializing Euracom RS232 port (%s)", device);

  if ((fd=open(device, O_RDWR))<0) {
    log_msg(ERR_CRIT, "Error opening %s: %s", device, strerror(errno));
    return(-1);
  }

  if (tcgetattr(fd, &term) == -1 ) {
    log_msg(ERR_CRIT, "tcgetattr: %s", strerror(errno));
    return(-1);
  }
  memset(term.c_cc, 0, sizeof(term.c_cc));
  term.c_cc[VMIN]=1;
  term.c_cflag=B9600 | CS8 | CREAD | CRTSCTS;
  term.c_iflag=IGNBRK | IGNPAR;
  term.c_oflag=0;
  term.c_lflag=0;

  if (tcsetattr(fd, TCSANOW, &term)==-1) {
    log_msg(ERR_CRIT, "tcsetattr: %s", strerror(errno));
    return(-1);
  }

  sleep(1);
  ioctl(fd, TIOCMGET, &flags);
  if (!(flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS not set");
    close_euracom_port(fd);
    return(-1);
  }
  if (!(flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "Euracom did not raise CTS. Check connection");
    close_euracom_port(fd);
    return(-1);
  }

  return(fd);
}


/*------------------------------------------------------*/
/* int close_euracom_port()                             */
/* */
/* Schlie�t RS232 port, setzt RTS zur�ck                */
/* */
/* RetCode: 0: O.k, -1: Error                           */
/*------------------------------------------------------*/
int close_euracom_port(fd)
{
  int flags = 0;

  log_msg(ERR_DEBUG, "Shutting down Euracom RS232 port");

  ioctl(fd, TIOCMSET, &flags);

  sleep(1);
  ioctl(fd, TIOCMGET, &flags);
  if ((flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS still set");
    return(-1);
  }
  if ((flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "CTS still set");
    return(-1);
  }

  close(fd);
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
/* Signal handler f�r non-fatal signals                 */
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
/* Signal handler f�r fatal signals                     */
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
/* Signal handler f�r Programm-Beendigung               */
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
  printf("Usage: %s [-gjp] device\n", prg);
  printf("\t-g file\tGeb�hrenfile\n");
  printf("\t-j file\tJunkfile\n");
  printf("\t-p file\tProtokollfile\n");
  exit(0);
}


/*------------------------------------------------------*/
/* char *readln_rs232()                                 */
/* */
/* Daten liegen an fd an. Lesen bis 0A 0D 00 oder timeo */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
char *readln_rs232(int fd)
{
  static char buf[1024];
  char *cp=buf;
  fd_set fds;
  int retval;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  do {
    struct timeval tv;
    char inbuf;

    tv.tv_sec=5; tv.tv_usec=0;
    if ((retval=select(fd+1, &fds, NULL, NULL, &tv))>0) {
      /* Data from Euracom Port */
      if (read(fd, &inbuf, 1)!=1) {
	log_msg(ERR_ERROR, "read() failed in readln_rs232: %s", strerror(errno));
	break;
      }
      *cp=inbuf;
      if (*cp=='\0') {
	if ((*(cp-1)!='\r') || (*(cp-2)!='\n')) {
	  log_msg(ERR_ERROR, "Incomplete line from Euracom");
	}
	*(cp-2)='\0';
	break;
      } else {
	cp++;
      }
    }
  } while (retval>0);

  if (retval==0) {
    log_msg(ERR_WARNING, "Timeout during RS232 I/O");
    return(NULL);
  } else if (retval<0) {
    log_msg(ERR_ERROR, "select() failed: %s", strerror(errno));
    return(NULL);
  } else {
    return(buf);
  }
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

    tv.tv_sec=10; tv.tv_usec=0;
    retval=select(max_fd, &rfds, NULL, NULL, &tv);

    if (retval>0) {
      if (FD_ISSET(euracom_fd, &rfds)) {
	char *buf;

	/* Data from Euracom Port */
	if (!(buf=readln_rs232(euracom_fd))) {
	  log_msg(ERR_ERROR, "Euracom read failed. Ignoring line");
	} else {
	  parse_euracom_data(buf);
	}
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
  while ((opt = getopt(argc, argv, "g:")) != EOF) {
    switch (opt) {
      case 'g':
	gebuehr_filename=strdup(optarg);
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

  /* Initialize Euracom */
  if ((euracom_fd=init_euracom_port(devname))==-1) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    exit(1);
  }

  /* Re-open syslog facility */
  openlog("Euracom", 0, LOG_LOCAL0);

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
