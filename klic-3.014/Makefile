# Copyright 1993, 1994, 1995 Institute for New Generation Computer Technology
# Read COPYRIGHT for detailed information
#
# Top-level Makefile for KLIC system
# This file was created by KLIC configuration script.
# Date of Confuguration: Wed 02 Aug 2023 04:07:29 PM JST
# Configured by: ueda

########################################
# Shell
########################################

SHELL = /bin/sh

########################################
# Directory Names
########################################

# Include file directory
KLICINCLUDE = /usr/local/include

# Library directory
KLICLIB = /usr/local/lib

# Shared Library directory
KLICLIB_DLL = /usr/local/lib

# Executable directory
KLICBIN = /usr/local/bin

########################################
# Libraries to build
########################################

ARCHIVES = libklic.a libklict.a libklicm.a libklicd.a

########################################
# cygwin switches
########################################

CYGWIN = 

########################################
# System-dependent switches
########################################

# LN: link command to use for system compilation.
LN = ln -s

# KL1CMP: KL1 to C compiler used for compiling the self compiler.
# Only for system developers and not for normal users.
KL1CMP = false

# C compiler
CC = gcc -m32
LD = gcc -m32

# Optimization flags for compiling the runtime system by CC.
OPTLEVEL = 2
OPTFLAGS = -O2 -Wno-unused-result
KLICOPT = -O2 -g
DEBUGFLAGS = -g

# Additional CC flags for optimized compilation of KL1 programs.
UOPTFLAGS = -Wno-unused-result

# Library search flags
LIBSWITCHES = -L/usr/lib32   -lm -lnsl

# Library archive indexer
RANLIB = ranlib

# Shared Library
XLDFLAGS = 
EXTFLAGS = -fPIC
LDSHARED = gcc -m32 -shared
DLLEXT = .so
DLLIBS = libklic$(DLLEXT) libklict$(DLLEXT) libklicd$(DLLEXT)
DLLTARGET = runtimedir-dll
DLLINSTALLTARGET = install-runtime-dll

# Make program
MAKE = make

# Installer
INSTALL = /bin/cp -p
INSTDIR = /bin/mkdir -p
INSTALLHDR = /bin/cp -p

# Maximum parallelism in compilation
PARALLEL = 0


DISTSYSTEM = shm


NODES = 4


KL1SELFCOMP=kl1cmp

all:	self

install:	install-basics

# Copyright 1993, 1994, 1995 Institute for New Generation Computer Technology
# Read COPYRIGHT for detailed information
# (C)1996, 1997, 1998, 1999 Japan Information Processing Development Center
# (Read COPYRIGHT-JIPDEC for detailed information.)
#
# Common Part of the Makefile for KLIC
#
# Site-specific definitions will be prepended to this
# by the configuration script

CFLAGS=$(OPTFLAGS) $(DEBUGFLAGS) $(EXTFLAGS)
LDFLAGS=$(LIBSWITCHES) $(XLDFLAGS)

self:	version.sed compilerdir runtimedir $(DLLTARGET)

cross:	version.sed compilerdir.cross runtimedir $(DLLTARGET)

version.sed: version.sed.tmplt klic-version
	sed -e "s%##TODAY%`date -R`%;s%##KLICVERSION%`cat klic-version`%" \
		<version.sed.tmplt >version.sed

verify:	verify-compiler verify-runtime

dist:	klic klicdb runtimedist compilerdist documentdist

runtimedist:
	cd runtime; \
	$(MAKE) PARALLEL="$(PARALLEL)" KL1CMP="$(KL1CMP)" dist

runtimedir: klic klicdb
	cd runtime; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		LDFLAGS="$(LDFLAGS)" KL1CMP="$(KL1CMP)" \
		KLICOPT="$(KLICOPT)" \
		ARCHIVES="$(ARCHIVES)" LN="$(LN)" DISTTYPE="$(DISTTYPE)"\
		CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB=$(RANLIB) \
		all

runtimedir-dll: klic klicdb
	cd runtime; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		LDFLAGS="$(LDFLAGS)" KL1CMP="$(KL1CMP)" \
		KLICOPT="$(KLICOPT)" \
		ARCHIVES="$(ARCHIVES)" LN="$(LN)" DISTTYPE="$(DISTTYPE)"\
		CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" DLLIBS="$(DLLIBS)" \
		LDSHARED="$(LDSHARED)" RANLIB=$(RANLIB) \
		dll

verify-runtime: klic klicdb
	cd runtime; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" \
		CC="$(CC)" LD="$(LD)" CYGWIN=$(CYGWIN) \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB=$(RANLIB) \
		verify

compilerdist: runtimedist
	cd compiler; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CC="$(CC)" LD="$(LD)" \
		KL1CMP="$(KL1CMP)" dist

compilerdir: runtimedir
	cd compiler; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	KLIC_LD_OPTIONS="-L../runtime $(LDFLAGS)"; \
	export KLIC_LD_OPTIONS; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		KLICOPT="$(KLICOPT)" \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" \
		all

compilerdir.cross:
	cd compiler; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	KLIC_LD_OPTIONS="$(LDFLAGS)"; \
	export KLIC_LD_OPTIONS; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		KLICOPT="$(KLICOPT)" \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" \
		klicdb klic

verify-compiler:
	cd compiler; \
	LD_LIBRARY_PATH="../runtime:$(LD_LIBRARY_PATH)"; \
	export LD_LIBRARY_PATH; \
	KLIC_LD_OPTIONS="$(LDFLAGS)"; \
	export KLIC_LD_OPTIONS; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		KLICOPT="$(KLICOPT)" CYGWIN=$(CYGWIN) \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" \
		verify

#klic:
#	cd compiler; \
#	$(MAKE) CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
#		KL1CMP="$(KL1CMP)" \
#		PVMINC="$(PVMINC)" PVMLIBDIR="$(PVMLIBDIR)" \
#		PVMLIB="$(PVMLIB)" \
#		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" klic

klic:
	cd compiler; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	KLIC_LD_OPTIONS="$(LDFLAGS)"; \
	export KLIC_LD_OPTIONS; \
	$(MAKE) CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" klic

klicdb:
	cd compiler; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	KLIC_LD_OPTIONS="$(LDFLAGS)"; \
	export KLIC_LD_OPTIONS; \
	$(MAKE) CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" klicdb

rmonitor:
	cd rmon; \
	$(MAKE) KLICLIB="$(KLICLIB)" clean all

kl1pp:
	cd compiler; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	$(MAKE) CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		LDFLAGS="$(LDFLAGS)" CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" kl1pp

documentdist:
	cd documents; $(MAKE) dist

tests:	compilerdir runtimedir $(DLLTARGET)
	echo 'Seq. impl.: NG' >tests.log
	cd test; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" \
		distclean tests;
	echo 'Seq. impl.: OK' >tests.log

disttests:	compilerdir runtimedir $(DLLTARGET)
	echo 'Distributed mem. imp.: NG' >disttests.log
	cd disttest; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" KLICOPT="$(KLICOPT)" NODES="$(NODES)" \
		distclean all-test;
	echo 'Distributed mem. imp.: OK' >disttests.log

shmtests:	compilerdir runtimedir $(DLLTARGET)
	cd shmtest; \
	LD_LIBRARY_PATH=../runtime:${LD_LIBRARY_PATH}; \
	export LD_LIBRARY_PATH; \
	$(MAKE) PARALLEL="$(PARALLEL)" KLICOPT="$(KLICOPT)" NODES="$(NODES)" \
		distclean all-test

install-basics: all install-compiler install-runtime $(DLLINSTALLTARGET) install-include

install-compiler: compilerdir
	cd compiler; \
	$(MAKE) install INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		KL1CMP="$(KL1CMP)" KL1SELFCOMP="$(KL1SELFCOMP)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)" \
		KLICINCLUDE="$(KLICINCLUDE)" DLLEXT="$(DLLEXT)" \
		CYGWIN=$(CYGWIN)

install-runtime: runtimedir
	cd runtime; \
	$(MAKE) install INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		KL1CMP="$(KL1CMP)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)" \
		ARCHIVES="$(ARCHIVES)" \
		KLICINCLUDE="$(KLICINCLUDE)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB="$(RANLIB)" \
		CYGWIN=$(CYGWIN)

install-runtime-dll: runtimedir-dll
	cd runtime; \
	$(MAKE) install-dll INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		KL1CMP="$(KL1CMP)" KLICLIB="$(KLICLIB)" \
		KLICLIB_DLL="$(KLICLIB_DLL)" KLICBIN="$(KLICBIN)" \
		ARCHIVES="$(ARCHIVES)" \
		KLICINCLUDE="$(KLICINCLUDE)" \
		DLLEXT="$(DLLEXT)" DLLIBS="$(DLLIBS)" \
		LDSHARED="$(LDSHARED)" RANLIB="$(RANLIB)" \
		install-dll

install-include:
	cd include; \
	$(MAKE) install INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		INSTALLHDR="$(INSTALLHDR)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)" \
		KLICINCLUDE="$(KLICINCLUDE)"

install-rmon_server:
	cd runtime; \
	$(MAKE) install-rmon_server INSTALL="$(INSTALL)" \
		INSTDIR="$(INSTDIR)" KL1CMP="$(KL1CMP)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)" \
		ARCHIVES="$(ARCHIVES)" \
		KLICINCLUDE="$(KLICINCLUDE)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB="$(RANLIB)"

install-rmonitor: install-rmon_server rmonitor
	cd rmon; \
	$(MAKE) install INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)"

depend:
	cd runtime; $(MAKE) depend INSTALL="$(INSTALL)" INSTDIR="$(INSTDIR)" \
		KLICOPT="$(KLICOPT)" PVMDIR="$(PVM_ROOT)"\
		ARCHIVES="$(ARCHIVES)" LN="$(LN)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)" \
		KLICINCLUDE="$(KLICINCLUDE)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB="$(RANLIB)"
	cd compiler; $(MAKE) depend CFLAGS="$(CFLAGS)" \
		CC="$(CC)" LD="$(LD)" KLICINCLUDE="$(KLICINCLUDE)" \
		KLICLIB="$(KLICLIB)" KLICBIN="$(KLICBIN)"
	cd rmon; $(MAKE) depend \
		CFLAGS="$(CFLAGS)" OPTLEVEL="$(OPTLEVEL)" \
		PVMDIR="$(PVM_ROOT)" PVMLIBDIR="$(PVMLIBDIR)" \
		PVMLIB="$(PVMLIB)" LDFLAGS="$(LDFLAGS)"\
		CC="$(CC)" LD="$(LD)" \
		DLLEXT="$(DLLEXT)" LDSHARED="$(LDSHARED)" RANLIB=$(RANLIB)

clean:
	cd compiler; $(MAKE) clean
	cd include; $(MAKE) clean
	cd runtime; $(MAKE) clean LN="$(LN)" DLLIBS="$(DLLIBS)"
	cd test; $(MAKE) clean
	cd disttest; $(MAKE) clean
	cd shmtest; $(MAKE) clean
#	cd documents; $(MAKE) clean
	cd rmon; $(MAKE) clean
	- /bin/rm -f core Makefile.bak *~ \#*\#

distclean:
	cd compiler; $(MAKE) distclean
	cd include; $(MAKE) distclean
	cd runtime; $(MAKE) distclean LN="$(LN)"
	cd test; $(MAKE) distclean
	cd disttest; $(MAKE) distclean
	cd shmtest; $(MAKE) distclean
#	cd documents; $(MAKE) distclean
	cd rmon; $(MAKE) distclean
	- /bin/rm -f Makefile core Makefile.bak config.tmp config.sh \
		tests.log disttests.log mail.log \
		*~ \#*\#

realclean:
	cd compiler; $(MAKE) realclean
	cd include; $(MAKE) realclean
	cd runtime; $(MAKE) realclean LN="$(LN)"
	cd test; $(MAKE) realclean
	cd disttest; $(MAKE) realclean
	cd shmtest; $(MAKE) realclean
#	cd documents; $(MAKE) realclean
	cd rmon; $(MAKE) realclean
	- /bin/rm -f Makefile core *.bak *.orig version.sed \
		tests.log disttests.log mail.log \
		config.tmp config.sh *~ \#*\#

mail:
	./sendreport.sh
