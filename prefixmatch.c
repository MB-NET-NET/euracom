/*
 * prefixmatch.c -- Some stupid routines for PostgreSQL
 *
 * $Id$
 */
#include <string.h>
#include "postgres_ext.h"
#include "libpq-fe.h"
#include "server/c.h"

bool prefix_match(text *t1, text *t2)
{
  return (strncasecmp(t1->vl_dat, t2->vl_dat, t2->vl_len-4)==0);
}

int2 length(text *t1)
{
  return (t1->vl_len-4);
}
