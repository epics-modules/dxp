TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += xerxes

USR_CFLAGS_DEFAULT += -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_vxWorks += -DEXCLUDE_DXPX10P
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

xerxes_SRCS_vxWorks += dxp4c.c
xerxes_SRCS_vxWorks += dxp4c2x.c
xerxes_SRCS_DEFAULT += x10p.c
xerxes_SRCS += xerxes.c
xerxes_SRCS += xerxes_log.c
xerxes_SRCS += md_epics.c
xerxes_SRCS += md_log_epics.c

xerxes_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += xerxes.h
INC += xerxes_structures.h
INC += xerxes_generic.h
INC += xia_xerxes_structures.h
INC += xerxesdef.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

