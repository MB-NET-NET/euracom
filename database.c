/* Database.c - Routines for phone-number lookups
   $Id: database.c,v 1.2 1996/11/02 15:19:57 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/Attic/database.c,v $
*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <gdbm.h>
#include <time.h>
#include "euracom.h"
#include "log.h"
#include "utils.h"
#include "fileio.h"

#define DB_BLOCK_SIZE	1024
#define DB_MODE		0644

static GDBM_FILE db_avon, db_wkn;
static BOOLEAN db_open = FALSE;


BOOLEAN openDB(char *db1, char *db2)
{
  if (db_open) {
    log_msg(ERR_WARNING, "Databases already opened. Ignoring request");
    return(TRUE);
  }

  db_avon = gdbm_open(db1, DB_BLOCK_SIZE, GDBM_READER, 0644, NULL);
  db_wkn  = gdbm_open(db2, DB_BLOCK_SIZE, GDBM_READER, 0644, NULL);

  if (!db_avon) {
    log_msg(ERR_ERROR, "Error opening %s", db1);
    return(FALSE);
  }
  
  if (!db_wkn) {
    log_msg(ERR_ERROR, "Error opeing %s", db2);
    return(FALSE);
  }

  db_open=TRUE;
  return(TRUE);
}

BOOLEAN closeDB()
{
  if (db_open) {
    gdbm_close(db_avon);
    gdbm_close(db_wkn);
    log_msg(ERR_DEBUG, "Database files closed");
    db_open=FALSE;
    return(TRUE);
  } else {
    log_msg(ERR_WARNING, "Database files already closed");
    return(FALSE);
  }
}


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


/*------------------------------------------------------*/
/* BOOLEAN split_text()                                 */
/* */
/* Splits up all_txt and writes key, value, rest        */
/* */
/* RetCode: TRUE: O.k; FALSE: Item not found            */
/*------------------------------------------------------*/
static BOOLEAN split_text(GDBM_FILE dbf, 
	       char *all_txt, 
	       char *key, char *value, char *rest)
{
  datum db_key;

  strcpy(key, ""); strcpy(value, ""); strcpy(rest, "");

  db_key.dptr=all_txt;
  for (db_key.dsize=strlen(all_txt); db_key.dsize>2; db_key.dsize--) {

    /* Check whether key exists in DB */
    if (gdbm_exists(dbf, db_key)) {
      datum content = gdbm_fetch(dbf, db_key);

      if (content.dptr) {
	/* Yup, it does. Now extract fields */
	strncpy(key, db_key.dptr, db_key.dsize); key[db_key.dsize]='\0';
	strncpy(value, content.dptr, content.dsize); 
	value[content.dsize]='\0';
	strcpy(rest, &all_txt[db_key.dsize]);
	free(content.dptr);
	return(TRUE);
      } else {
	log_msg(ERR_CRIT, "Key exists but cannot be fetched");
      }
    }	/* IF gdbm_exists */
  }	/* FOR i */
  return(FALSE);
}


/*------------------------------------------------------*/
/* void lookup_number()                                 */
/* */
/* Converts plain telephone-number in FQTN struct       */
/*------------------------------------------------------*/
void lookup_number(TelNo num, struct FQTN *fqtn)
{
  char t1[128], t2[128], t3[128];

  /* Teil 1: WKN */
  if (split_text(db_wkn, num, t1, t2, t3)) {
    strcpy(fqtn->telno, t1);
    strcpy(fqtn->wkn, t2);
    strcpy(fqtn->rest, t3);
  } else {
    strcpy(fqtn->telno, num);
    strcpy(fqtn->wkn, "");
    strcpy(fqtn->rest, "");
  }
      
  /* Teil 2. AVON */
  if (split_text(db_avon, fqtn->telno, t1, t2, t3)) {
    strcpy(fqtn->avon, t1); 
    strcpy(fqtn->avon_name, t2);
    strcpy(fqtn->telno, t3); 
  } else {
    strcpy(fqtn->avon, "");
    strcpy(fqtn->avon_name, "");
  }
}

void convert_telno(TelNo telno, struct FQTN *fqtn)
{
  strcpy(telno, "");
  if (strlen(fqtn->avon)) {
    strcat(telno, fqtn->avon);
    strcat(telno, " ");
  }
 
  if (strlen(fqtn->telno)) {
    strcat(telno, fqtn->telno);
  }

  if (strlen(fqtn->rest)) {
    strcat(telno, "-");
    strcat(telno, fqtn->rest);
  }
}
