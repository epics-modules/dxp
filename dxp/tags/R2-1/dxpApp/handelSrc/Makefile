TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# This application can only be built on Linux (parallel port) and vxWorks (CAMAC)
LIBRARY_IOC_Linux += handel
LIBRARY_IOC_vxWorks += handel
LIBRARY_IOC_cygwin32 += handel
LIBRARY_IOC_WIN32 += handel

USR_CFLAGS_Linux +=    -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_cygwin32 += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_WIN32 += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C -DEXCLUDE_DXP4C2X
USR_CFLAGS_vxWorks +=  -DEXCLUDE_DXPX10P
USR_CFLAGS +=          -DEXCLUDE_UDXP -DEXCLUDE_UDXPS -DEXCLUDE_DGF200 -DEXCLUDE_POLARIS 
USR_CFLAGS +=          -DEXCLUDE_USB -DEXCLUDE_SERIAL -DEXCLUDE_ARCNET

# The following lines eliminate the -ansi flag when compiling md_epics.c, because it
# does inline assembly that does not work with -ansi.
ifeq ($(GNU),YES)
ANSI = -ansi
md_epicsANSI= -std=gnu89
CONFORM_CFLAGS = $($(findstring md_epics,$*)ANSI)
endif


#handel_SRCS_vxWorks += dxp4c.c
handel_SRCS_vxWorks  += dxp4c2x.c dxp4c2x_psl.c
handel_SRCS_vxWorks  += dxp4c.c
handel_SRCS_Linux    += x10p.c x10p_psl.c
handel_SRCS_cygwin32 += x10p.c x10p_psl.c
handel_SRCS_WIN32    += x10p.c x10p_psl.c
handel_SRCS_Linux    += md_epics.c
handel_SRCS_vxWorks  += md_epics.c
handel_SRCS_cygwin32 += md_win95.c epplib.c
handel_SRCS_WIN32    += md_win95.c epplib.c
handel_SRCS += xerxes.c
handel_SRCS += xerxes_log.c
handel_SRCS += md_log.c
handel_SRCS += handel.c
handel_SRCS += fdd.c
handel_SRCS += psl.c
handel_SRCS += handel_dbg.c
handel_SRCS += handel_detchan.c
handel_SRCS += handel_dyn_default.c
handel_SRCS += handel_dyn_detector.c
handel_SRCS += handel_dyn_firmware.c
handel_SRCS += handel_dyn_module.c
handel_SRCS += handel_file.c
handel_SRCS += handel_log.c
handel_SRCS += handel_memdbg_win32.c
handel_SRCS += handel_run_control.c
handel_SRCS += handel_run_params.c
handel_SRCS += handel_sort.c
handel_SRCS += handel_system.c
handel_SRCS += handel_xerxes.c

handel_LIBS_Linux += $(EPICS_BASE_IOC_LIBS)
handel_LIBS_cygwin32 += $(EPICS_BASE_IOC_LIBS)
handel_LIBS_vxWorks += $(EPICS_BASE_IOC_LIBS)
handel_LIBS_WIN32     += DLPORTIO
LIB_INSTALLS_WIN32    += ../DLPORTIO.lib
LIB_INSTALLS_cygwin32 += ../DLPORTIO.lib
INC += xia_common.h
INC += xia_xerxes.h
INC += xerxes_generic.h
INC += xia_xerxes_structures.h
INC += xerxes_structures.h
INC += xerxesdef.h
INC += handel.h
INC += handeldef.h
INC += handel_generic.h

#===========================
ifneq ($(strip $(LIB_INSTALLS_$(OS_CLASS))),)
LIB_INSTALLS+=$(subst -nil-,,$(LIB_INSTALLS_$(OS_CLASS)))
else
ifdef LIB_INSTALLS_DEFAULT
LIB_INSTALLS+=$(LIB_INSTALLS_DEFAULT)
endif
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef LIB_INSTALLS
buildInstall : libInstalls
endif

libInstalls:  $(LIB_INSTALLS)
	@echo "Installing $(^F)"
	@$(INSTALL) -d -m 555 $^ $(INSTALL_LIB)


