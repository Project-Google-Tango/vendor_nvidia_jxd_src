#Copyright (C) 2007 L. Donnie Smith

PACKAGE_NAME = CWiid
PACKAGE_TARNAME = cwiid
PACKAGE_VERSION = 0.6.00
PACKAGE_STRING = CWiid 0.6.00
PACKAGE_BUGREPORT = cwiid@abstrakraft.org

prefix = /home/vsalve/wii/wiican/wmgui/cwiid-0.6.00+svn201/install_dir
exec_prefix = ${prefix}

sysconfdir = ${prefix}/etc
libdir = ${exec_prefix}/lib

datarootdir = ${prefix}/share
mandir = ${datarootdir}/man
docdir = ${datarootdir}/doc/${PACKAGE_TARNAME}

CC = gcc
AWK = gawk
LEX = flex
YACC = bison -y
PYTHON = python

COMMON = /home/vsalve/wii/wiican/wmgui/cwiid-0.6.00+svn201/common

ifdef DESTDIR
	ROOTDIR = $(DESTDIR:%/=%)
endif

CWIID_CONFIG_DIR = $(ROOTDIR)${sysconfdir}/cwiid
CWIID_PLUGINS_DIR = $(ROOTDIR)${libdir}/cwiid/plugins

DEBUGFLAGS = -g
WARNFLAGS = -Wall -W
CFLAGS = $(DEBUGFLAGS) $(WARNFLAGS) -DHAVE_CONFIG_H -I$(COMMON)/include
