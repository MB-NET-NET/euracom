/* Database.c - Routines for phone-number lookups
   $Id: database.c,v 1.6 1997/08/28 09:30:43 bus Exp $
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
    db_open=FALSE;
    return(TRUE);
  } else {
    log_msg(ERR_WARNING, "Database files already closed");
    return(FALSE);
  }
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

  if (!db_open) {
    log_msg(ERR_WARNING, "Database files not open");
    return(FALSE);
  }
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
