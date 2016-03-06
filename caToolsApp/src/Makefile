TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=============================
# Build the IOC application

PROD = caTools
#caTools_PROD_VERSION += 1

# Build the main IOC entry point on workstation OSs.
#caTools_SRCS += caToolsMain.cpp
caTools_SRCS += caToolsMain.c


# Finally link to the EPICS Base libraries
caTools_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CFLAGS_DEFAULT += -std=c99

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

