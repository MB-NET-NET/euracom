/*********************************************************************/
/* utils.c - Generic utilities                                       */
/*                                                                   */
/* Copyright (C) 1996-2004 MB Computrex           Released under GPL */
/*********************************************************************/

static char rcsid[] = "$Id$";

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pwd.h>
#include <errno.h>

#include "log.h"
#include "utils.h"


/*---------------------------------------------------------------------*/
/* char *fgetline()                                                    */
/* */
/* Liest einen String aus der Datei ein (ohne CR).                     */
/* \CR wird ueberlesen                                                 */
/* */
/* RetCode: NULL = Error, sonst Ptr auf string                         */
/* : len!=NULL : Enthaelt Laenge des gelesenen Strings                 */
/*---------------------------------------------------------------------*/
char   *fgetline(fp, len)
  FILE   *fp;
  int    *len;
{
  static char *cp = NULL;
  int     i, k, l;

  /* Beim Ersten Aufruf mallocen */
  if (!cp) {
    cp = (char *) malloc(FGETLINE_BUFLEN);
  }
  l = FGETLINE_BUFLEN - 1;
  cp[0] = '\0';

rep:
  i = strlen(cp);
  l = l - i;
  if (!fgets(&cp[i], l, fp)) {
    return((i==0)?NULL:cp);
  }

  /* Remove trailing CR */
  k = strlen(cp);
  if (cp[k - 1] == '\n') {
    cp[--k] = '\0';
    /* Remove trailing \ */
    if ((k > 0) && (cp[k - 1] == '\\')) {
      cp[--k] = '\0';
      goto rep;
    }
  }
  if (len) {
    *len = strlen(cp);
  }
  return (cp);
}

/*---------------------------------------------------------------------*/
/* void    fputline(FILE *fp, char *zeile)                             */
/* */
/* Schreibt einen String mit anschliessendem CR in file		       */
/*---------------------------------------------------------------------*/
void    fputline(FILE *fp, const char *zeile)
{
  fprintf(fp, "%s\n", zeile);
}

/*---------------------------------------------------------------------*/
/* BOOLEAN check_file()                                                */
/* */
/* Ueberprueft, ob filename vorhanden ist                              */
/* RetCode: FALSE: Fehler, TRUE: O.k                                   */
/*---------------------------------------------------------------------*/
BOOLEAN check_file(name)
  const char *name;
{
  struct stat sbuf;

  if (strlen(name) == 0) {
    return (TRUE);
  }
  if (stat(name, &sbuf) == 0) {
    if (S_ISREG(sbuf.st_mode)) {
      return(TRUE);
    }
  }
  return(FALSE);
}

/*---------------------------------------------------------------------*/
/* char *get_unique_tmpname()	                      		       */
/* */
/* Erzeugt einen temporaeren Dateinamen				       */
/* Erzeugt bei jedem Aufruf einen neuen Namen			       */
/* Dateiname wird in outstring geschrieben; falls NULL: malloc!	       */
/* */
/* RetCode: Ptr auf Dateinamen. NULL: Error			       */
/*---------------------------------------------------------------------*/
char   *get_unique_tmpname(pfx, outstring)
  const char *pfx;
  char   *outstring;
{
  char   *ptr;

  if ((ptr=tempnam(NULL, pfx))==NULL) {
    log_msg(ERR_CRIT, "get_unique_tmpname: Unable to create tmpname");
    return (NULL);
  }
  if (outstring == NULL) {
    /* Kein Zielstring: Ptr auf malloced-Bereich vom tempnam zurueckgeben */
    return (ptr);
  } else {
    /* Zielstring: Ptr kopieren, dann freigeben */
    strcpy(outstring, ptr);
    free(ptr);
    return (outstring);
  }
}

/*---------------------------------------------------------------------*/
/* BOOLEAN delete_file()                                               */
/* */
/* Loescht angegebenes file				               */
/* */
/* RetCode: TRUE: O.k. ; FALSE: Fehler				       */
/*---------------------------------------------------------------------*/
BOOLEAN delete_file(filename)
  const char *filename;
{
  if (!unlink(filename)) {
    debug(4, "delete_file: Removing %s", filename);
    return (TRUE);
  } else {
    debug(1, "delete_file: %s could not be deleted", filename);
    return (FALSE);
  }
}

/*---------------------------------------------------------------------*/
/* BOOLEAN copy_file()                                                 */
/* */
/* Kopiert file src nach dst				               */
/* */
/* RetCode: TRUE: O.k. ; FALSE: Fehler				       */
/*---------------------------------------------------------------------*/
BOOLEAN copy_file(const char *src, const char *dst)
{
  char *buf;
  int fd_r, fd_w;
  ssize_t r, len = 0;

  debug(2, "copy_file: Copying \"%s\" +==> \"%s\"", src, dst);

  if ((fd_r=open(src, O_RDONLY))<0) {
    log_msg(ERR_ERROR, "Could not open %s for reading: %s", src, get_strerror);
    return(FALSE);
  }
  if ((fd_w=open(dst, O_WRONLY | O_CREAT, 0600))<0) {
    log_msg(ERR_ERROR, "Could not open %s for writing: %s", dst, get_strerror);
    close(fd_r);
    return(FALSE);
  }

  buf=malloc(COPY_BUFSIZE+1);
  while ((r=read(fd_r, buf, COPY_BUFSIZE))>0) {
    len+=write(fd_w, buf, r);
  }
  debug( 3, "%ld bytes copied", len);
  close(fd_r);
  free(buf);

  if (close(fd_w)<0) {
    log_msg(ERR_ERROR, "Error closing %s: %s", dst, get_strerror);
    return(FALSE);
  }
  return (TRUE);
}

/*----------------------------------------------------------------------*/
/* int detach()                                                         */
/*                                                                      */
/* Detaches itself from controlling tty, forks itself in the bg and     */
/* returns "success" to the environment                                 */
/*                                                                      */
/* RetCode: 0 : Error detaching, else PID of new process                */
/*----------------------------------------------------------------------*/
int detach()
{
  pid_t child;

  debug(2, "Entering daemon mode...");

  switch (child=fork()) {
    case -1: 
      log_msg(ERR_FATAL, "fork() failed: %s", strerror(errno));
      break;

    case 0: 	/* Child */
      {
        int devnull;
        int fd_max = sysconf(_SC_OPEN_MAX);
        int fd;

        debug(3, "fork() successful");

        /* Ignore standard stop signals */
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        /* Close any open files */
        for (fd=((fd_max<1)?256:fd_max)-1; fd>=0; fd--) { close(fd); }

        /* Move to root directory to make sure we aren't on a mounted fs anymore */
        chdir("/");

        /* Clear any inherited file mode creation mask */
        umask(0);

        /* Redirect std[in|out|err] to /dev/null */
        if ((devnull=open(_PATH_DEVNULL, O_RDWR | O_NOCTTY, 0))!=-1) {
          dup2(devnull, STDIN_FILENO);
          dup2(devnull, STDOUT_FILENO);
          dup2(devnull, STDERR_FILENO);
          if (devnull>2) { close(devnull); }
        }

        /* Put ourself in a new process group */
        setsid();

        /* Detach from controlling tty */
        if ((fd=open(_PATH_TTY, O_WRONLY)>=0)) {
          ioctl(fd, TIOCNOTTY, NULL);
          close(fd);
        }
      }
      return(getpid());
      break;

    default:	/* Parent */
      _exit(0);
      break;
  }     /* SWITCH */

  return(0);
}

/*---------------------------------------------------------------------*/
/* int stripblank(char *string)                                        */
/* */
/* Loescht fuehrende und nachfolgende Blanks aus einem String          */
/* RetCode: Neue Laenge von string				       */
/*---------------------------------------------------------------------*/
int     stripblank(string)
  char   *string;
{
  char   *cp;
  int     len;

  /* Nachfolgende Blanks entfernen */
  len = strlen(string)-1;
  while ((len >= 0) && ((string[len] == ' ') || (string[len] == '\n'))) {
    len--;
  }
  string[++len] = '\0';

  /* Leerzeichen am Anfang entfernen */
  for (cp = string; ((*cp) && (*cp == ' ')); cp++);
  strcpy(string, cp);

  return (strlen(string));
}

/*--------------------------------------------------------------------------
 * BOOLEAN str_isdigit()
 *
 * Checks whether string consists entirely of digits (0-9)
 *
 * Inputs: String
 * RetCode: TRUE/FALSE
 *------------------------------------------------------------------------*/
BOOLEAN str_isdigit(char *str)
{
  char *cp;
  
  for (cp=str; *cp; cp++) { if (!isdigit(*cp)) return(FALSE); }
  return(TRUE);
}

/*--------------------------------------------------------------------------
 * char *strcatf()
 *
 * Combined sprintf and strcat
 *
 * Inputs: Destination-string, format, arguments
 * RetCode: Ptr to destination string
 *------------------------------------------------------------------------*/
char *strcatf(char *dst, const char *fmt, ...)
{
  char buf[1024]; /* FIXME */

  va_list ap;

  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);

  return(strcat(dst, buf));
}

/*--------------------------------------------------------------------------
 * char *my_strncpy()
 *
 * A version of strncpy that \0 terminates the string
 *
 * Inputs: Destination-string, source string, max bytes
 * RetCode: Ptr to destination string
 *
 * FIXME: Inherits the mis-feature of null-padding the string (see strncpy)
 *------------------------------------------------------------------------*/
char *my_strncpy(char *dst, const char *src, size_t n)
{
  strncpy(dst, src, n); dst[n]='\0';
  return(dst);
}

/*--------------------------------------------------------------------------
 * void safe_free()
 *
 * free(3) wrapper
 *
 * Inputs: Ptr
 *------------------------------------------------------------------------*/
void safe_free(void *ptr)
{
  if (ptr) {
    free(ptr); ptr=NULL;
  } else {
    debug(1, "Freeing NULL pointer");
  }
}

/*--------------------------------------------------------------------------
 * void safe_malloc()
 *
 * malloc(3) wrapper. Never returns NULL
 *
 * Inputs: Requested bytes
 *------------------------------------------------------------------------*/
void *safe_malloc(size_t len)
{
  void *ptr = malloc(len);

  if (!ptr) {
    log_msg(ERR_FATAL, "Virtual memory exhausted. (%ld bytes requested)", len);
    exit(3);
  } else {
    debug(5, "Allocated %ld bytes", len);
  }
  return(ptr);
}
