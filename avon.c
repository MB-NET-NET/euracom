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
  check_createDB("./avon.dat", "./avondb");
  check_createDB("./wkn.dat", "./wkndb");

  openDB("./avondb", "./wkndb");
  while (cp=fgetline(stdin, NULL)) {
    lookup_number(cp, &fqtn);
    convert_telno(telno, &fqtn);
    printf("->%s\n", telno);
  }
  closeDB();
  close_log();
  return(0);
}
