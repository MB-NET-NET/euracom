#include <stdio.h>
#include "fileio.h"
#include "log.h"
#include "euracom.h"

int main()
{
  char *cp;
  struct FQTN fqtn;
  TelNo telno;

  init_log("avon", ERR_JUNK, USE_STDERR, NULL);

  if (!openDB(AVON_DB_NAME, WKN_DB_NAME)) {
    log_msg(ERR_FATAL, "Error opening database files");
    return(1);
  }
  while (cp=fgetline(stdin, NULL)) {
    lookup_number(cp, &fqtn);
    convert_telno(telno, &fqtn);
    printf("->%s\n", telno);
    printf("--> (%s) %s - %s; %s (%s)\n",
    	fqtn.avon, fqtn.telno, fqtn.rest,
    	fqtn.wkn, fqtn.avon_name);
  }
  closeDB();
  close_log();
  return(0);
}
