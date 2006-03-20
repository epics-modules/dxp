TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# This application can only be built on Linux (parallel port) and vxWorks (CAMAC)
LIBRARY_IOC_Linux += handelSrc
LIBRARY_IOC_vxWorks += handelSrc
LIBRARY_IOC_cygwin32 += handelSrc
LIBRARY_IOC_WIN32 += handelSrc

USR_CFLAGS_Linux    += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_cygwin32 += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_WIN32    += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_vxWorks  += -DEXCLUDE_DXPX10P
USR_CFLAGS +=          -DEXCLUDE_UDXP -DEXCLUDE_UDXPS -DEXCLUDE_DGF200 -DEXCLUDE_POLARIS 
USR_CFLAGS +=          -DEXCLUDE_USB -DEXCLUDE_SERIAL -DEXCLUDE_ARCNET
USR_CFLAGS_WIN32    += -DHANDEL_USE_DLL -DHANDEL_MAKE_DLL -DWIN32

# The following lines eliminate the -ansi flag when compiling md_epics.c, because it
# does inline assembly that does not work with -ansi.
ifeq ($(GNU),YES)
ANSI = -ansi
md_epicsANSI= -std=gnu89
CONFORM_CFLAGS = $($(findstring md_epics,$*)ANSI)
endif


#handelSrc_SRCS_vxWorks += dxp4c.c
handelSrc_SRCS_vxWorks  += dxp4c2x.c dxp4c2x_psl.c
handelSrc_SRCS_vxWorks  += dxp4c.c
handelSrc_SRCS_Linux    += x10p.c x10p_psl.c
handelSrc_SRCS_cygwin32 += x10p.c x10p_psl.c
handelSrc_SRCS_WIN32    += x10p.c x10p_psl.c
handelSrc_SRCS_Linux    += md_epics.c
handelSrc_SRCS_vxWorks  += md_epics.c
handelSrc_SRCS_cygwin32 += md_win95.c epplib.c
handelSrc_SRCS_WIN32    += md_win95.c epplib.c
handelSrc_SRCS += xerxes.c
handelSrc_SRCS += xerxes_log.c
handelSrc_SRCS += md_log.c
handelSrc_SRCS += handel.c
handelSrc_SRCS += fdd.c
handelSrc_SRCS += psl.c
handelSrc_SRCS += handel_dbg.c
handelSrc_SRCS += handel_detchan.c
handelSrc_SRCS += handel_dyn_default.c
handelSrc_SRCS += handel_dyn_detector.c
handelSrc_SRCS += handel_dyn_firmware.c
handelSrc_SRCS += handel_dyn_module.c
handelSrc_SRCS += handel_file.c
handelSrc_SRCS += handel_log.c
handelSrc_SRCS += handel_memdbg_win32.c
handelSrc_SRCS += handel_run_control.c
handelSrc_SRCS += handel_run_params.c
handelSrc_SRCS += handel_sort.c
handelSrc_SRCS += handel_system.c
handelSrc_SRCS += handel_xerxes.c

handelSrc_LIBS += $(EPICS_BASE_IOC_LIBS)
handelSrc_LIBS += $(EPICS_BASE_IOC_LIBS)
handelSrc_LIBS += $(EPICS_BASE_IOC_LIBS)
handelSrc_LIBS_WIN32 += DLPORTIO
LIB_INSTALLS_cygwin32 += ../DLPORTIO.lib ../handel.lib
LIB_INSTALLS_WIN32    += ../DLPORTIO.lib ../handel.lib
BIN_INSTALLS_cygwin32 += ../fdd.dll ../handel.dll ../handel_vba.dll ../md.dll 
BIN_INSTALLS_WIN32    += ../fdd.dll ../handel.dll ../handel_vba.dll ../md.dll 
BIN_INSTALLS_cygwin32 += ../plxlib.dll ../xerxes.dll ../xmap.dll ../xmap_psl.dll
BIN_INSTALLS_WIN32    += ../plxlib.dll ../xerxes.dll ../xmap.dll ../xmap_psl.dll
INC += xia_common.h
INC += xia_xerxes.h
INC += xerxes_generic.h
INC += xia_xerxes_structures.h
INC += xerxes_structures.h
INC += xerxesdef.h
INC += handel.h
INC += handel_constants.h
INC += handeldef.h
INC += handel_generic.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

