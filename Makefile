######################### -*- Mode: Makefile-Gmake -*- ########################
## Copyright (C) 2025, Mats Bergstrom
## $Id$
## 
## File name       : Makefile
## Description     : For mql.
## 
## Author          : Mats Bergstrom
## Created On      : Fri Jul  4 19:59:06 2025
## 
## Last Modified By: Mats Bergstrom
## Last Modified On: Sun Nov 16 15:29:12 2025
## Update Count    : 14
###############################################################################

CC		= gcc
CFLAGS		= -Wall -pedantic-errors -g -pthread
CPPFLAGS	= 
LDLIBS		= -lmosquitto -lmql -lpthread
LDFLAGS		= -L.

PREFIXDIR	= ..

BINDIR		= $(PREFIXDIR)/bin
LIBDIR		= $(PREFIXDIR)/lib
INCDIR		= $(PREFIXDIR)/include

BINFILES 	= mql
LIBFILES	= libmql.a
INCFILES	= mql.h

BINOBJ		= mql.o mql_listen.o

all: mql libmql.a

mql: $(BINOBJ) libmql.a

mql.o: mql.c mql.h
mql_listen.o: mql_listen.c mql.h

libmql.a: mqllib.o
	ar crv libmql.a mqllib.o

mqllib.o: mqllib.c mql.h


.PHONY: clean uninstall install



clean:
	rm -f *.o mql *~ *.log .*~ libmql.a

uninstall:
	cd $(BINDIR); rm $(BINFILES)

install:
	if [ ! -d $(BINDIR) ] ; then mkdir $(BINDIR); fi
	cp $(BINFILES) $(BINDIR)
	if [ ! -d $(LIBDIR) ] ; then mkdir $(LIBDIR); fi
	cp $(LIBFILES) $(LIBDIR)
	if [ ! -d $(INCDIR) ] ; then mkdir $(INCDIR); fi
	cp $(INCFILES) $(INCDIR)
