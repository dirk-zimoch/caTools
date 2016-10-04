TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC application

ifeq ($(OS_CLASS),Linux)
PROD = caTools
#caTools_PROD_VERSION += 1
endif

# Build the main IOC entry point on workstation OSs.
caTools_SRCS += caToolsMain.c
caTools_SRCS += caToolsOutput.c
caTools_SRCS += caToolsInput.c
caTools_SRCS += caToolsUtils.c


# Finally link to the EPICS Base libraries
caTools_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CFLAGS_DEFAULT += -std=c99 -Wall

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifeq ($(OS_CLASS),Linux)
install: links
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
