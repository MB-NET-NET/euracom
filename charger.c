/* $Id: charger.c,v 1.4 1996/11/16 10:53:34 bus Exp $ */

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <string.h>
#include "euracom.h"
#include "log.h"
#include "utils.h"
#include "fileio.h"

#define GEBUEHR_FILE		"/var/lib/euracom/gebuehr.dat"
#define UNKNOWN_TEXT_EURA	"Rufnr.unbekannt"
#define	UNKNOWN_NO		"???"

/* Filter options */
struct Filter {
  int teilnehmer_num;
  int teilnehmer[5];
  time_t datum_von, datum_bis;
  enum TVerbindung art;
};

float base_charge = 18.0;

/*------------------------------------------------------*/
/* void usage()                                         */
/* */
/* Infos zur Programmbedienung                          */
/*------------------------------------------------------*/
void usage(const char *prg)
{
  printf("Usage: %s [-g]\n", prg);
  printf("\t-g file\tGebührenfile\n");
  printf("\t-t no\tOnly print internal # matching no\n");
  printf("\t-v time-from\tSets start-time\n");
  printf("\t-b time-to\tSets end-time\n");
  printf("\t-m Charge\tSets base charge (DM)\n");

  exit(0);
}


struct GebuehrInfo *file2geb(const char *stc, struct GebuehrInfo *geb)
{
  char *sbuf;
  char *items[15];
  int i;

  sbuf=(char *)malloc(strlen(stc)+5); strcpy(sbuf, stc);
  strcat(sbuf, ";");

  /* Eingabezeile aufsplitten */
  if ((i=get_args(sbuf, items, ';'))!=7) {
    log_msg(ERR_WARNING, "Input line scrambled. Args are %d",i );
    free(sbuf);
    return(NULL);
  }

  /* TVerbindung */
  switch(atoi(items[0])) {
    case 0:
      geb->art=FEHLER;
      log_msg(ERR_WARNING, "Invalid ConType logged");
      break;
    case 1:
      geb->art=GEHEND;
      break;
    case 2:
      geb->art=KOMMEND;
      break;
    case 3:
      geb->art=VERBINDUNG;
      break;
    default:
      geb->art=FEHLER;
      log_msg(ERR_CRIT, "ConType == %s", items[0]);
      break;
  }
      
  /* Teilnehmer */
  geb->teilnehmer=atoi(items[3]);

  /* Datum (Euracom und System) */
  geb->datum=atol(items[1]);
  geb->doe=atol(items[2]);

  if (abs(geb->datum-geb->doe)>3600) {
    log_msg(ERR_WARNING, "Timestamp invalid. Using system time");
    geb->datum=geb->doe;
  }
  /* Remote Number */
  strcpy(geb->nummer, items[4]);

  /* Einheiten, Betrag */
  geb->einheiten=atoi(items[5]);
  stripblank(items[6]);
  sscanf(items[6], "%f", &geb->betrag);

  free(sbuf);
  return(geb);
}

int print_line(const struct GebuehrInfo *geb)
{
  char buf[1024] = "", buf2[80];
  struct tm *tm = localtime(&geb->datum);

  /* Anschluss */
  sprintf(buf2, "%d & ", geb->teilnehmer);
  strcat(buf, buf2);

  /* Datum */
  strftime(buf2, 30, "%d.%m.%Y %H:%M & \0", tm);
  strcat(buf, buf2);

  /* Je nach Art */
  switch (geb->art) {
    case KOMMEND:
      if (strcmp(geb->nummer, "???")==0) {
        sprintf(buf2, "Vergeblicher Anruf & & " );
      } else {
	sprintf(buf2, "Vergeblicher Anruf von %s & & ", geb->nummer);
      }
      strcat(buf, buf2);
      break;

    case VERBINDUNG:
      if (strcmp(geb->nummer, "???")==0) {
        sprintf(buf2, "Eingehender Anruf & &");
      } else {
	sprintf(buf2, "Eingehender Anruf von %s & &", geb->nummer);
      }
      strcat(buf, buf2);
      break; 

    case GEHEND:
      {
	struct FQTN fqtn;
	TelNo telno;

	lookup_number(geb->nummer, &fqtn);
	convert_telno(telno, &fqtn);

	/* First line */
	sprintf(buf2, "%s & %d & %.2f DM", 
	      telno,
	      geb->einheiten,
	      geb->betrag);
	strcat(buf, buf2);

	/* Need a second line? */
	if (strlen(fqtn.avon_name) || strlen(fqtn.wkn)) {
	  sprintf(buf2, "\\\\\n& & \\footnotesize{%s (%s)} & &",
		  fqtn.wkn, fqtn.avon_name);
	  strcat(buf, buf2);
	}
      }
      break;

    default:
      sprintf(buf2, "Fehler in Eingabedatei");
      strcat(buf, buf2);
      break;
  }

  sprintf(buf2, "\\\\\n");
  strcat(buf, buf2);

  printf("%s", buf);

  return(0);
}

  
BOOLEAN check_filter(const struct Filter *filter, const struct GebuehrInfo *geb)
{
  int i;
  BOOLEAN found = FALSE;

  /* Check TEILNEHMER */
  for (i=0; i<filter->teilnehmer_num; i++) {
    if (geb->teilnehmer==filter->teilnehmer[i]) { found=TRUE; }
  }
  if ((filter->teilnehmer_num) && (!found)) { return(FALSE); }

  /* Datum von */
  if ((filter->datum_von) && (geb->datum<filter->datum_von)) { return(FALSE); }

  /* Datum bis */
  if ((filter->datum_bis) && (geb->datum>filter->datum_bis)) { return(FALSE); }

  /* Verbinungsart */
  if ((filter->art) && (geb->art!=filter->art)) { return(FALSE); }

  return(TRUE);
}


BOOLEAN eval_chargefile(const char *name, struct Filter *filter)
{
  FILE *fp = fopen(name, "rt");
  char *cp;
  struct GebuehrInfo geb;
  long lineno = 0;
  int no_calls = 0, total_e = 0;
  float total_g = 0.0;

  if (!fp) {
    log_msg(ERR_FATAL, "Could not open %s: %s", name, strerror(errno));
    return(FALSE);
  }

  printf("\\documentstyle[11pt,german,a4,isolatin1]{article}\n");
  printf("\\pagestyle{empty}\n\\nofiles\n\\begin{document}\n");

  printf("\\begin{tabular}{|c|l|l|r|r|}\n\\hline\n");
  printf("\\small{Anschluß} & \\small{Datum} & \\small{Nummer} & \\small{Einheiten} & \\small{Betrag} \\\\\n");
  printf("\\hline\n");

  while (cp=fgetline(fp, NULL)) {
    lineno++;
    if (!file2geb(cp, &geb)) {
      log_msg(ERR_ERROR, "%s l.%ld: parse error", name, lineno);
    } else {
      if (check_filter(filter, &geb)) {
        print_line(&geb);
        if (geb.betrag) {
	  no_calls++;
	  total_g+=geb.betrag; total_e+=geb.einheiten;
        }
      }
    }
  }

  fclose(fp);

  printf("\\hline\n");
  printf(" &         & %d Gespräche & %d & %.2f DM \\\\\n",
	 no_calls, total_e, total_g);
  printf(" &         & Grundgebühr  &    & %.2f DM \\\\\n\\hline\n", base_charge);
  printf(" & GESAMT: &              &    & %.2f DM \\\\\n",
	 total_g+base_charge);
  printf("\\hline\n\\end{tabular}\n\\end{document}\n");
  return(TRUE);
}

    
int main(argc, argv)
  int argc;
  char **argv;
{
  extern char *optarg;
  extern int optind;
  int opt;
  char *gebuehr_filename = NULL;
  struct Filter filter;

  /* Clear filter */
  filter.teilnehmer_num=0;
  filter.datum_von=filter.datum_bis=0;
  filter.art=0;

  /* Logging */
  init_log("charger", ERR_JUNK, USE_STDERR, NULL);

  /* Parse command line options */
  while ((opt = getopt(argc, argv, "m:g:t:v:b:a:")) != EOF) {
    switch (opt) {
      case 'a':
	log_msg(ERR_DEBUG, "Filter \"art\" set to %s", optarg);
	filter.art=atoi(optarg);
	break;
      case 'b':
	filter.datum_bis=atol(optarg);
	log_msg(ERR_DEBUG, "Filter \"date_to\" set to %lu", filter.datum_bis);
	break;
      case 'g':
	gebuehr_filename=strdup(optarg);
        break;
      case 'm:
      	base_charge=(float)atof(optarg);
      	break;
      case 't':
	filter.teilnehmer[filter.teilnehmer_num++]=atoi(optarg);
	log_msg(ERR_DEBUG, "Filter \"teilnehmer\" (%d) set to %d", 
		filter.teilnehmer_num,
		filter.teilnehmer[filter.teilnehmer_num-1]);
	break;
      case 'v':
	filter.datum_von=atol(optarg);
	log_msg(ERR_DEBUG, "Filter \"date_from\" set to %lu", filter.datum_von);
	break;
      default:
        usage(argv[0]);
        exit(0);
        break;
    }     /* SWITCH */
  }	/* WHILE */

  /* Check logfiles */
  if (!gebuehr_filename) { gebuehr_filename=strdup(GEBUEHR_FILE); }

  /* Open database files */
  if (!openDB(AVON_DB_NAME, WKN_DB_NAME)) {
    log_msg(ERR_FATAL, "Could not open database files");
    exit(1);
  }

  /* Open charge file. Do evaluation */
  eval_chargefile(gebuehr_filename, &filter);

  closeDB();
  close_log();

  return(0);  
}
