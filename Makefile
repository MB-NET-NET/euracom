#
# Makefile for 'Euracom Gebührenerfassung'
#
# Copyright (C) 1996-1998 by Michael Bussmann
#
# Released unter the terms of the GPL.  See LICENCE for details
#
# $Id: Makefile,v 1.1 1998/10/06 16:41:38 bus Exp $
#

# !! PLEASE CHECK config.h AS WELL !!

#
# Program version number
#
VERSION = 1.3.0

# Use debugging and profiling libs
# DEBUG=1

#
# Where executables/manpage should be installed
#
DSTBASE = /usr/local
BINDIR  = $(DSTBASE)/sbin
MANDIR  = $(DSTBASE)/man/man8

#
# Set this to the desired DB subsystem
#   a) postgres
#   b) msql (Michael Tepperis <101355.1561@compuserve.com>)
#   c) mysql (Stefan Schorsch <sschorsch@mail.rmc.de>)
#
DATABASE=postgres

ifeq ($(DATABASE), postgres)
  DATABASE_BASE    = /opt/postgreSQL

  DATABASE_INCLUDE = $(DATABASE_BASE)/include
  DATABASE_LIB     = $(DATABASE_BASE)/lib
  DATABASE_OBJ     = $(DATABASE_BASE)/obj

  # glibc2 users may need to use the second line
  DATABASE_LINK    = -lpq
  #DATABASE_LINK   = -lpq -lcrypt
endif

ifeq ($(DATABASE), msql)
  DATABASE_BASE    = /usr/local/Hughes

  DATABASE_INCLUDE = $(DATABASE_BASE)/include
  DATABASE_LIB     = $(DATABASE_BASE)/lib
  DATABASE_OBJ     = /tmp

  DATABASE_LINK    = -lmsql
endif

ifeq ($(DATABASE), mysql)
  DATABASE_INCLUDE = /usr/include/mysql
  DATABASE_LIB     = /usr/lib
  DATABASE_OBJ     = /tmp

  DATABASE_LINK    = -lmysqlclient
endif

#
# Standard compiler settings
#
CC=gcc
CFLAGS=-m486 -pipe -Wall

#
# End of user serviceable parts.  From here on anything is my fault
#

# Debugging/profiling version?
ifdef DEBUG
  # Flags for developer's version
  CFLAGS+=-g -pg
  LDFLAGS+=-pg
else
  # Flags for production version
  CFLAGS+=-O
  LDFLAGS+=-s
endif

# Sets name of database interface file
DATABASE_SUBSYS=$(DATABASE).c

export CC
export CPPFLAGS
export LDFLAGS
export CFLAGS

all: euracom prefixmatch.so

euracom: database.o euracom.o serial.o utils.o privilege.o log.o
	$(LINK.o) -o $@ $^ -L$(DATABASE_LIB) $(DATABASE_LINK)

database.o: $(DATABASE_SUBSYS)
	$(CC) $(CFLAGS) -I$(DATABASE_INCLUDE) -o $@ -c $^

.c.o:
	$(CC) $(CFLAGS) -DVERSION="\"$(VERSION)\"" -o $@ -c $^

prefixmatch.so: prefixmatch.c
ifeq ($(DATABASE), postgres)
	$(CC) -o prefix.o -I$(DATABASE_INCLUDE) -O -fPIC -c $^
	$(LD) -s -Bshareable -o $@ prefix.o
	@$(RM) prefix.o
else
	@echo "prefixmatch.so will only work with postgreSQL DBMS"
endif

install: euracom prefixmatch.so
ifeq ($(DATABASE), postgres)
	install -o postgres -m 755 -d $(DATABASE_OBJ)
	install -o postgres -m 755 prefixmatch.so $(DATABASE_OBJ)
endif
	install -o root -m 755 -d $(BINDIR)
	install -o root -m 755 euracom $(BINDIR)

	install -o man -g man -m 755 -d $(MANDIR)
	install -o man -g man -m 644 euracom.8 $(MANDIR)

database:
ifeq ($(DATABASE), postgres)
	psql -eqf generate_tables.sql isdn
endif
ifeq ($(DATABASE), msql)
	msql isdn < generate_msql_tables.sql
endif
ifeq ($(DATABASE), mysql)
	mysql isdn < generate_mysql_tables.sql
endif

clean:
	-rm -f *.o euracom prefixmatch.so euracom-man.txt ChangeLog gmon.out

euracom-man.txt: euracom.8
	nroff -Tascii -man $^ | col -b >$@

ChangeLog: CVS/Entries
# Using NIS does have some disadvantages...
	rcs2log -u 'bus	Michael Bussmann	bus@fgan.de' >$@

release: ChangeLog euracom-man.txt
	@echo Generating release $(VERSION)...
	@cd /tmp && cvs export -D 'now' euracom
	@cp ChangeLog /tmp/euracom/
	@cd /tmp && mv euracom euracom-$(VERSION)
	@cd /tmp && tar -czf euracom.tar.gz euracom-$(VERSION)
	@-mkdir release
	@mv /tmp/euracom.tar.gz release/euracom-$(VERSION).tar.gz
	@rm -r /tmp/euracom-$(VERSION)
	@cp euracom-man.txt release
	@cp README.html release/euracom-README.html
	@cd release && pgps -ba -u 0xAA320021 +clearsig=on euracom-$(VERSION).tar.gz -o euracom-$(VERSION).pgpsig
	@echo "--- NEW RELEASE: $(VERSION) ---"
	@ls -l release/
