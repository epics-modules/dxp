TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# Currently this application only works on vxWorks (CAMAC) and
# Linux (parallel port)

LIBRARY_IOC_Linux   += dxp
LIBRARY_IOC_vxWorks += dxp

DBDINC += dxpRecord
DBD += dxpSupport.dbd
DBD += dxp.dbd
DBD += dxpVX.dbd

USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The following are compiled and added to the Support library
dxp_SRCS += dxpRecord.c
dxp_SRCS += mcaDXPServer.cc
dxp_SRCS += devDXP.cc
dxp_SRCS += dxpRegister.c

dxp_LIBS += xerxes
dxp_LIBS += mca 
dxp_LIBS += calc sscan autosave
dxp_LIBS += mpf 
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

PROD_IOC_vxWorks += dxpApp
PROD_IOC_Linux += dxpApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
dxpApp_SRCS_Linux += dxp_registerRecordDeviceDriver.cpp
dxpApp_SRCS_vxWorks += dxpVX_registerRecordDeviceDriver.cpp
dxpApp_SRCS_Linux += dxpMain.cpp
dxpApp_SRCS_vxWorks += -nil-
#The following adds support from base/src/vxWorks
dxpApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

dxpApp_LIBS += dxp xerxes
dxpApp_LIBS += mca
dxpApp_LIBS += calc sscan autosave
dxpApp_LIBS_vxWorks += camac
dxpApp_LIBS += mpf 
dxpApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

