/* Database.c - Routines for phone-number lookups
   $Id: database.c,v 1.1 1996/11/02 13:11:55 bus Exp $
   $Source: /home/bus/Y/CVS/euracom/Attic/database.c,v $
*/

#include <stdlib.h>
#include <strings.h>
#include <gdbm.h>
#include "euracom.h"
#include "log.h"
#include "utils.h"
#include "fileio.h"

#define DB_BLOCK_SIZE	1024

struct FQTN {
  char avon[15];
  char telno[40];
  char rest[15];
  char avon_name[40];
  char wkn[40];
};

int createDB(char *txt, char *db)
{
  GDBM_FILE dbf;
  FILE *fp;
  char *cp;
  int num=0;

  printf("Creating database files...\n");
  if ((dbf=gdbm_open(db, DB_BLOCK_SIZE, GDBM_NEWDB | GDBM_FAST, 0644, NULL))==NULL) {
    fprintf(stderr, "Error creating database files\n");
    return(0);
  }

  if ((fp=fopen(txt, "rt"))==NULL) {
    fprintf(stderr, "Error reading source file\n");
    return(0);
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
        fprintf(stderr, "Inserting %s: Return code is %d\n", cp, ret);
      } else {
        num++;
      }
    }
  }
  printf("%d lines added to database\n", num);

  fclose(fp);
  gdbm_close(dbf);

  return(num);
}


int split_text(GDBM_FILE dbf, 
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
	return(1);
      } else {
	fprintf(stderr, "Yikes! Key exists but cannot be fetched\n");
      }
    }	/* IF gdbm_exists */
  }	/* FOR i */
  return(0);
}


int print_fqtn(struct FQTN *fqtn)
{
  printf("Telefonnummer:\tAVON %s TELNO %s REST %s\n\tWKN %s (AVONN %s)\n",
	 fqtn->avon, fqtn->telno, fqtn->rest,
	 fqtn->wkn, fqtn->avon_name);
  return(0);
}

int read_loop()
{
  GDBM_FILE dbf1 = gdbm_open("./avondb", DB_BLOCK_SIZE, GDBM_READER, 0644, NULL);
  GDBM_FILE dbf2 = gdbm_open("./wkndb", DB_BLOCK_SIZE, GDBM_READER, 0644, NULL);
  char *cp;

  if (!dbf1) {
    fprintf(stderr, "Error opening dbf\n");
    return(0);
  }
  if (!dbf2) {
    fprintf(stderr, "Error opening dbf\n");
    return(0);
  }
  while (cp=fgetline(stdin, NULL)) {
    struct FQTN fqtn;
    char t1[128], t2[128], t3[128];

    /* Teil 1: WKN */
    if (split_text(dbf2, cp, t1, t2, t3)) {
      strcpy(fqtn.telno, t1);
      strcpy(fqtn.wkn, t2);
      strcpy(fqtn.rest, t3);
    } else {
      strcpy(fqtn.telno, cp);
      strcpy(fqtn.wkn, "WKN");
      strcpy(fqtn.rest, "REST");
    }
      
    /* Teil 2. AVON */
    if (split_text(dbf1, fqtn.telno, t1, t2, t3)) {
      strcpy(fqtn.avon, t1); 
      strcpy(fqtn.avon_name, t2);
      strcpy(fqtn.telno, t3); 
    } else {
      strcpy(fqtn.avon, "AVON");
      strcpy(fqtn.avon_name, "AVON NAME");
    }

    print_fqtn(&fqtn);
  }


  gdbm_close(dbf1);
  gdbm_close(dbf2);
}


int main()
{
  /* createDB("./avon.dat", "./avondb");
  createDB("./wkn.dat", "./wkndb"); */

  read_loop();
  return(0);
}

