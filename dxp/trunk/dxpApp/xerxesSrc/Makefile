TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# This application can only be built on Linux (parallel port) and vxWorks (CAMAC)
LIBRARY_IOC_Linux += xerxes
LIBRARY_IOC_vxWorks += xerxes

USR_CFLAGS_Linux += -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_vxWorks += -DEXCLUDE_DXPX10P
USR_CFLAGS += -DEXCLUDE_UDXP -DEXCLUDE_UDXPS -DEXCLUDE_USB -DEXCLUDE_DGFG200

# The following lines eliminate the -ansi flag when compiling md_epics.c, because it
# does inline assembly that does not work with -ansi.
ifeq ($(GNU),YES)
ANSI = -ansi
md_epicsANSI= -std=gnu89
CONFORM_CFLAGS = $($(findstring md_epics,$*)ANSI)
endif

xerxes_SRCS_vxWorks += dxp4c.c
xerxes_SRCS_vxWorks += dxp4c2x.c
xerxes_SRCS_Linux += x10p.c
xerxes_SRCS += xerxes.c
xerxes_SRCS += xerxes_log.c
xerxes_SRCS += md_epics.c
xerxes_SRCS += md_log.c

xerxes_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += xerxes.h
INC += xerxes_structures.h
INC += xerxes_generic.h
INC += xia_xerxes_structures.h
INC += xerxesdef.h
INC += xia_common.h
INC += xia_md.h
INC += xia_mddef.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

