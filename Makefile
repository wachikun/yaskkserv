# -*- Makefile -*-

include Makefile.config

ifdef DEBUG
export VAR_PATH		= $(PROJECT_ROOT)/var/$(ARCHITECTURE_LOWER_CASE)/debug
else				# DEBUG
export VAR_PATH		= $(PROJECT_ROOT)/var/$(ARCHITECTURE_LOWER_CASE)/release
endif				# DEBUG

export MAKEFILE		= Makefile.$(ARCHITECTURE_LOWER_CASE)
export MAKEDEPEND	= $(PERL) -w $(PROJECT_ROOT)/tools/build/makedepend.$(ARCHITECTURE_LOWER_CASE)

ifeq ($(ARCHITECTURE),BSD_CYGWIN_LINUX_GCC)
SOURCE_PATH		= ./source
PRE_COMMAND		=
else				# ($(ARCHITECTURE),BSD_CYGWIN_LINUX_GCC)
include UNKNOWN_ARCHITECTURE_or_Makefile.config_not_found__please_execute_configure__dummy_include_file
endif				# ($(ARCHITECTURE),BSD_CYGWIN_LINUX_GCC)

.PHONY			: all clean run makerun break makebreak kill makekill debugger vlist vhist vreport tags depend cleandepend install package test setup

all			: setup
	$(PRE_COMMAND)
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) all

run			: setup
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) run

makerun			: setup
	$(PRE_COMMAND)
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) makerun

break			: setup
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) break

makebreak		: setup
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) makebreak

clean			: setup
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) clean

realclean		: setup
	$(RM) -rf ./var
	$(RM) -rf Makefile.config

distclean		: realclean

depend			: setup
	$(PRE_COMMAND)
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) depend

cleandepend		: setup
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) cleandepend

test			:
	$(MAKE) --no-print-directory -C $(SOURCE_PATH) -f $(MAKEFILE) test

install_common_		:
	$(MKDIR) -p $(PREFIX)/bin
	$(INSTALL) $(VAR_PATH)/yaskkserv_make_dictionary/yaskkserv_make_dictionary $(PREFIX)/bin/yaskkserv_make_dictionary

install			: install_normal

install_all		: install_common_
	$(MKDIR) -p $(PREFIX)/sbin
	$(INSTALL) $(VAR_PATH)/yaskkserv_simple/yaskkserv_simple $(PREFIX)/sbin/yaskkserv_simple
	$(INSTALL) $(VAR_PATH)/yaskkserv_normal/yaskkserv_normal $(PREFIX)/sbin/yaskkserv_normal
	$(INSTALL) $(VAR_PATH)/yaskkserv_hairy/yaskkserv_hairy $(PREFIX)/sbin/yaskkserv_hairy

install_simple		: install_common_
	$(MKDIR) -p $(PREFIX)/sbin
	$(INSTALL) $(VAR_PATH)/yaskkserv_simple/yaskkserv_simple $(PREFIX)/sbin/yaskkserv

install_normal		: install_common_
	$(MKDIR) -p $(PREFIX)/sbin
	$(INSTALL) $(VAR_PATH)/yaskkserv_normal/yaskkserv_normal $(PREFIX)/sbin/yaskkserv

install_hairy		: install_common_
	$(MKDIR) -p $(PREFIX)/sbin
	$(INSTALL) $(VAR_PATH)/yaskkserv_hairy/yaskkserv_hairy $(PREFIX)/sbin/yaskkserv

package			:
	$(MKDIR) -p var &&\
	$(MKDIR) -p var/package &&\
	$(RM) -rf var/package/yaskkserv-$(PROJECT_VERSION) &&\
	hg archive var/package/yaskkserv-$(PROJECT_VERSION) &&\
	cd var/package &&\
	$(RM) -f yaskkserv-$(PROJECT_VERSION)/.hg_archival.txt &&\
	$(RM) -f yaskkserv-$(PROJECT_VERSION)/.hgignore &&\
	$(RM) -rf yaskkserv-$(PROJECT_VERSION)/local &&\
	tar czf yaskkserv-$(PROJECT_VERSION).tar.gz yaskkserv-$(PROJECT_VERSION) &&\
	tar cJf yaskkserv-$(PROJECT_VERSION).tar.xz yaskkserv-$(PROJECT_VERSION) &&\
	sha1sum yaskkserv-$(PROJECT_VERSION).tar.gz yaskkserv-$(PROJECT_VERSION).tar.xz

setup			:
	mkdir -p $(VAR_PATH)/skk/architecture/$(ARCHITECTURE_LOWER_CASE)
	mkdir -p $(VAR_PATH)/yaskkserv_hairy
	mkdir -p $(VAR_PATH)/yaskkserv_make_dictionary
	mkdir -p $(VAR_PATH)/yaskkserv_normal
	mkdir -p $(VAR_PATH)/yaskkserv_simple
