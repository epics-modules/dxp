TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += dxp

DBDINC += dxpRecord
DBD += dxpSupport.dbd

USR_CFLAGS += -DEXCLUDE_DXPX10P
USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The following are compiled and added to the Support library
dxp_SRCS += dxpRecord.c
dxp_SRCS += mcaDXPServer.cc
dxp_SRCS += devDXP.cc

dxp_LIBS += xerxes
dxp_LIBS += mca 
dxp_LIBS += mpf mpfEpics
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

