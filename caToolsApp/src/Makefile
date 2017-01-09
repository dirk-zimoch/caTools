TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC application

ifeq ($(OS_CLASS),Linux)
PROD = caTools
USR_CFLAGS += -std=c99
endif

ifeq ($(OS_CLASS),WIN32)
PROD = caTools
SRC_DIRS += ../wingetopt/src
caTools_SRCS += getopt.c

# Disabling some warnings for Windows build system. Could use W4 instead of Wall too...
#  inline: this is an error fix, not warning fix. Windows alternative to inline is __inline
#  C4668: '__STDC_VERSION__' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'.
#  C4820: byte paddings in struct definitions
#  C4255: no function prototype given: converting '()' to '(void)'
#  C4711: function selected for automatic inline expansion
#  C4706: assignment within conditional expression

USR_CFLAGS += -Dinline=__inline /wd4668 /wd4820 /wd4255 /wd4711 /wd4706
endif


# Build the main IOC entry point on workstation OSs.
caTools_SRCS += caToolsMain.c
caTools_SRCS += caToolsOutput.c
caTools_SRCS += caToolsInput.c
caTools_SRCS += caToolsUtils.c

# Finally link to the EPICS Base libraries
caTools_LIBS += $(EPICS_BASE_IOC_LIBS)

#Generate version from GIT tag
GIT_VERSION := $(shell git describe --abbrev=2 --dirty --tags)


USR_CFLAGS += -Wall -DVERSION_STR=\"$(GIT_VERSION)\"


#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifeq ($(OS_CLASS),Linux)
install: links
endif

ifeq ($(OS_CLASS),WIN32)
install: linksWin
endif

#ifeq ($(EPICS_HOST_ARCH),$(T_A))
#install: archLinks
#endif

links: 
	ln -sf caTools $(INSTALL_BIN)/cado
	ln -sf caTools $(INSTALL_BIN)/caget
	ln -sf caTools $(INSTALL_BIN)/cagets
	ln -sf caTools $(INSTALL_BIN)/cainfo
	ln -sf caTools $(INSTALL_BIN)/camon
	ln -sf caTools $(INSTALL_BIN)/caput
	ln -sf caTools $(INSTALL_BIN)/caputq
	ln -sf caTools $(INSTALL_BIN)/cawait
	
linksWin: 
	del $(subst /,\,$(INSTALL_BIN))\cado.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\cado.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\caget.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\caget.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\cagets.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\cagets.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\cainfo.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\cainfo.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\camon.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\camon.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\caput.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\caput.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\caputq.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\caputq.exe caTools.exe
	del $(subst /,\,$(INSTALL_BIN))\cawait.exe >nul 2>&1
	cmd /c MKLINK $(subst /,\,$(INSTALL_BIN))\cawait.exe caTools.exe

archLinks:
	echo $(INSTALL_HOST_BIN)
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/cado
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/caget
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/cagets
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/cainfo
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/camon
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/caput
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/caputq
	ln -sf bin/$(EPICS_HOST_ARCH)/caTools $(TOP)/cawait
