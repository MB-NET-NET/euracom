#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <signal.h>
#include <termios.h>
#include "log.h"
#include "utils.h"
#include "fileio.h"

enum TVerbindung { FEHLER=0, GEHEND, KOMMEND, VERBINDUNG};
typedef char TelNo[32];
struct GebuehrInfo {
  enum TVerbindung art;
  int teilnehmer;
  time_t datum;
  TelNo nummer;
  int einheiten;
  float betrag;
};

#define DEFAULT_DEVICE	"/dev/cua0"

int euracom_fd = 0;	/* RS232 port to Euracom */


/*------------------------------------------------------*/
/* char *geb2str()                                      */
/* */
/* Wandelt GebuehrInfo struct in can. string um         */
/* */
/* RetCode: Ptr to static string, NULL: Error           */
/*------------------------------------------------------*/
char *geb2str(struct GebuehrInfo *geb)
{
  static char res[1024];
  char art;

  switch (geb->art) {
    case FEHLER:
      log_msg(ERR_WARNING, "Invalid geb->art");
      return(NULL);
      break;
    case GEHEND:
      art='G';
      break;
    case KOMMEND:
      art='K';
      break;
    case VERBINDUNG:
      art='V';
      break;
    default:
      log_msg(ERR_WARNING, "CASE missing in geb->art");
      art='G';
      break;
  }	/* SWITCH */

  sprintf(res, "%2d;%c;%d;%s;%d;%6.3f",
	  geb->teilnehmer,
	  art,
	  geb->datum,
	  geb->nummer,
	  geb->einheiten,
	  geb->betrag);
  
  return(res);
}

/*------------------------------------------------------*/
/* struct GebuehrInfo *str2geb()                        */
/* */
/* Wandelt can. string in GebuehrInfo struct um         */
/* */
/* RetCode: Ptr to param 1, NULL: Error                 */
/*------------------------------------------------------*/
struct GebuehrInfo *str2geb(struct GebuehrInfo *geb, const char *str)
{
  geb->art=KOMMEND;
  geb->teilnehmer=12;
  geb->datum=1;
  strcpy(geb->nummer, "123412");
  geb->einheiten=0;
  geb->betrag=0.0;

  return(geb);
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
  geb->art=KOMMEND;
  geb->teilnehmer=12;
  geb->datum=1;
  strcpy(geb->nummer, "123412");
  geb->einheiten=0;
  geb->betrag=0.0;

  return(geb);
}


int init_euracom_port(const char *device)
{
  struct termios term;
  int fd;
  int flags;

  log_msg(ERR_DEBUG, "Initializing Euracom RS232 port");

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

  euracom_fd=fd;

  sleep(1);
  ioctl(euracom_fd, TIOCMGET, &flags);
  if (!(flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS not set");
    close_euracom_port();
    return(-1);
  }
  if (!(flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "Euracom did not raise CTS. Check connection");
    close_euracom_port();
    return(-1);
  }

  return(fd);
}


int close_euracom_port()
{
  int flags = 0;

  log_msg(ERR_DEBUG, "Shutting down Euracom RS232 port");

  ioctl(euracom_fd, TIOCMSET, &flags);

  sleep(1);
  ioctl(euracom_fd, TIOCMGET, &flags);
  if ((flags & TIOCM_RTS)) {
    log_msg(ERR_CRIT, "RTS still set");
    return(-1);
  }
  if ((flags & TIOCM_CTS)) {
    log_msg(ERR_CRIT, "CTS still set");
    return(-1);
  }

  close(euracom_fd);
}

int shutdown_program(int force)
{
  close_euracom_port();
  close_log();
  exit(force);
}

int hangup(int sig)
{
  log_msg(ERR_NOTICE, "Signal %d received", sig);
  return(0);
}

int fatal(int sig)
{
  log_msg(ERR_FATAL, "Signal %d received. Giving up", sig);
  shutdown_program(1);
  return(0);
}

int terminate(int sig)
{
  log_msg(ERR_INFO, "Signal %d received. Program terminating", sig);
  shutdown_program(0);
  return(0);
}

void usage(const char *prg)
{
  printf("Usage: %s [-f] device\n", prg);
  printf("\t-f file\tName of output file\n\n");
  exit(0);
}


int parse_euracom_data(const char *buf)
{
  printf("I just got: %s\n", buf);
}

int select_loop()
{
  fd_set rfds;
  int max_fd=0;
  int retval;
  char ebuf[1024], *eb=ebuf;

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
	char buf;
	size_t t;

	/* Data from Euracom Port */
	if (read(euracom_fd, &buf, 1)==1) {
  	  *eb=buf;
	  if (buf=='\0') { 
	    parse_euracom_data(ebuf);
	    eb=ebuf;
	  } else {
	    eb++;
	  }
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
  while ((opt = getopt(argc, argv, "f")) != EOF) {
    switch (opt) {
      case 'f':
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

  /* Initialize Euracom */
  if ((euracom_fd=init_euracom_port(devname))==-1) {
    log_msg(ERR_FATAL, "Error initializing serial interface");
    exit(1);
  }

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
