#include <stdio.h>
#include <gdbm.h>
#include <sys/errno.h>
#include <strings.h>
#include "fileio.h"
#include "log.h"
#include "euracom.h"

#define DB_MODE		0644
#define DB_BLOCK_SIZE	1024
#define AVON_TXT_NAME	"/var/lib/euracom/avon"
#define WKN_TXT_NAME	"/var/lib/euracom/wkn.dat"

/*------------------------------------------------------*/
/* BOOLEAN createDB()                                   */
/* */
/* Creates database file                                */
/* */
/* RetCode: TRUE: O.k; FALSE: Error occured             */
/*------------------------------------------------------*/
static BOOLEAN createDB(char *txt, char *db)
{
  GDBM_FILE dbf;
  FILE *fp;
  char *cp;
  int num=0;

  log_msg(ERR_INFO, "Creating database %s from plain file %s",
	  db, txt);
  if ((dbf=gdbm_open(db, DB_BLOCK_SIZE, GDBM_NEWDB | GDBM_FAST, DB_MODE, NULL))==NULL) {
    log_msg(ERR_CRIT, "Error creating GDBM file");
    return(FALSE);
  }

  if ((fp=fopen(txt, "rt"))==NULL) {
    log_msg(ERR_CRIT, "Error reading %s: %s",
	    txt, strerror(errno));
    return(FALSE);
  }

  while (cp=fgetline(fp, NULL)) {
    char *cp2 = strchr(cp, ':');
    datum key, content;
    int ret;

    if (cp2) {
      *cp2++='\0';
      key.dptr=cp; key.dsize=strlen(cp);
      content.dptr=cp2; content.dsize=strlen(cp2);
      if ((ret=gdbm_store(dbf, key, content, GDBM_REPLACE))!=0) {
	log_msg(ERR_WARNING, "Could not insert %s in database. Return code is %d",
		cp, ret);
      } else {
        num++;
      }
    }
  }
  log_msg(ERR_DEBUG, "%d lines added to the database file", num);

  fclose(fp);
  gdbm_close(dbf);

  return(TRUE);
}

/*------------------------------------------------------*/
/* BOOLEAN check_createDB()                             */
/* */
/* Creates database file if neccessary                  */
/* */
/* RetCode: TRUE: O.k; FALSE: Error occured             */
/*------------------------------------------------------*/
BOOLEAN check_createDB(char *txt, char *db)
{
  if (last_modified(txt)>last_modified(db)) {
    log_msg(ERR_NOTICE, "%s is newer that %s. Re-creating database", txt, db);
    return(createDB(txt, db));
  }
  return(TRUE);
}


int main()
{
  char *cp;
  struct FQTN fqtn;

  init_log("mkdbs", ERR_JUNK, USE_STDERR, NULL);
  log_msg(ERR_INFO, "Checking database files...");
  createDB(AVON_TXT_NAME, AVON_DB_NAME);
  createDB(WKN_TXT_NAME, WKN_DB_NAME);
  close_log();

  return(0);
}
