/*
 * prefixmatch.c -- Some stupid routines for PostgreSQL
 *
 * $Id$
 */

#include "postgres.h"
#include <string.h>
#include "9.1/server/fmgr.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

bool prefix_match(text *t1, text *t2)
{
  return (strncasecmp(VARDATA(t1), VARDATA(t2), VARSIZE(t2)-VARHDRSZ)==0);
}

int2 length(text *t1)
{
  return (VARSIZE(t1)-VARHDRSZ);
}
