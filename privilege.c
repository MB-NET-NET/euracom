/*********************************************************************/
/* privilege.c - Utils for changing UID/EUID                         */
/*                                                                   */
/* Copyright (C) 1998-1998 MB Computrex           Released under GPL */
/*********************************************************************/

/*---------------------------------------------------------------------
 * Version:	$Id: privilege.c,v 1.3 1999/11/02 09:55:13 bus Exp $
 * File:	$Source: /home/bus/Y/CVS/euracom/privilege.c,v $
 *-------------------------------------------------------------------*/

static char rcsid[] = "$Id: privilege.c,v 1.3 1999/11/02 09:55:13 bus Exp $";

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "log.h"
#include "privilege.h"


static uid_t alt_uid = 65534;
static gid_t alt_gid = 65534;
static BOOLEAN active = FALSE;

/*--------------------------------------------------------------------------
 * int privilege_set_alternate_uid()
 *
 * Sets non-privileged UID.  Does NOT change current UID
 *
 * Inputs: UID
 * RetCode: -
 *------------------------------------------------------------------------*/
int privilege_set_alternate_uid(uid_t uid)
{
  debug(2, "privilege: Setting alternate UID to %d", uid);
  alt_uid=uid;
  return(TRUE);
}

/*--------------------------------------------------------------------------
 * int privilege_set_alternate_gid()
 *
 * Sets non-privileged GID.  Does NOT change current GID
 *
 * Inputs: GID
 * RetCode: -
 *------------------------------------------------------------------------*/
int privilege_set_alternate_gid(gid_t gid)
{
  debug(2, "privilege: Setting alternate GID to %d", gid);
  alt_gid=gid;
  return(TRUE);
}

/*--------------------------------------------------------------------------
 * int privilege_initialize()
 *
 * Activates privilege subsystem
 *
 * Inputs: -
 * RetCode: -
 *------------------------------------------------------------------------*/
int privilege_initialize()
{
  active=TRUE;
  return(1);
}

/*--------------------------------------------------------------------------
 * BOOLEAN privilege_active()
 *
 * Checks whether privilege subsystem is active
 *
 * Inputs: -
 * RetCode: TRUE: Active; FALSE: Inactive
 *------------------------------------------------------------------------*/
BOOLEAN privilege_active()
{
  return(active);
}

/*--------------------------------------------------------------------------
 * int privilege_leave_priv()
 *
 * Temporarily switches to alternate uid/gid
 *
 * Inputs: -
 * RetCode: 0: Error; 1: O.k
 *------------------------------------------------------------------------*/
int privilege_leave_priv()
{
  debug(2, "privilege: Dropping effective %d/%d -> %d/%d (Real %d/%d)",
    geteuid(), getegid(), alt_uid, alt_gid, getuid(), getgid());
  if (setegid(alt_gid)!=0) {
    log_msg(ERR_WARNING, "Could not switch from %d to EGID %d", getegid(), alt_gid);
    return(0);
  }
  if (seteuid(alt_uid)!=0) {
    log_msg(ERR_WARNING, "Could not switch from %d to EUID %d", geteuid(), alt_uid);
    return(0);
  }
  return(1);
}

/*--------------------------------------------------------------------------
 * int privilege_enter_priv()
 *
 * Temporarily regain old privileges
 *
 * Inputs: -
 * RetCode: 0: Error; 1: O.k.
 *------------------------------------------------------------------------*/
int privilege_enter_priv()
{
  debug(2, "privilege: Regaining effective %d/%d -> %d/%d",
    geteuid(), getegid(), getuid(), getgid());
  if (setegid(getgid())!=0) {
    log_msg(ERR_WARNING, "Could not switch from %d to EGID %d", getegid(), getgid());
    return(0);
  }
  if (seteuid(getuid())!=0) {
    log_msg(ERR_WARNING, "Could not switch from %d to EUID %d", geteuid(), getuid());
    return(0);
  }
  return(1);
}

/*--------------------------------------------------------------------------
 * int privilege_drop_priv()
 *
 * Drops root privileges FOREVER
 *
 * Inputs: -
 * RetCode: 0: Error; 1: O.k.
 *------------------------------------------------------------------------*/
int privilege_drop_priv()
{
  debug(1, "privilege: Dropping privileges FOREVER (%d/%d -> %d/%d) (effective %d/%d)",
    getuid(), getgid(), alt_uid, alt_gid, geteuid(), getegid());
  if (setgid(alt_gid)!=0) {
    log_msg(ERR_WARNING, "Could not set GID from %d to %d", getgid(), alt_gid);
    return(0);
  }
  if (setuid(alt_uid)!=0) {
    log_msg(ERR_WARNING, "Could not set UID from %d to %d", getuid(), alt_uid);
    return(0);
  }
  return(1);
}
