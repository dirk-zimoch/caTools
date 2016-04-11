TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC application

PROD = caTools
#caTools_PROD_VERSION += 1

# Build the main IOC entry point on workstation OSs.
caTools_SRCS += caToolsMain.c
caTools_SRCS += caToolsOutput.c


# Finally link to the EPICS Base libraries
caTools_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CFLAGS_DEFAULT += -std=c99 -Wall

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


ifneq ($(T_A),)
install: links
endif

links: 
	ln -sf caTools $(INSTALL_BIN)/caget
	ln -sf caTools $(INSTALL_BIN)/cagets
	ln -sf caTools $(INSTALL_BIN)/cainfo
	ln -sf caTools $(INSTALL_BIN)/camon
	ln -sf caTools $(INSTALL_BIN)/caput
	ln -sf caTools $(INSTALL_BIN)/caputq
	ln -sf caTools $(INSTALL_BIN)/caputs
	ln -sf caTools $(INSTALL_BIN)/cawait
