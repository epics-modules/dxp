TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# This application can be built on Linux (EPP, USB) WIN32 (EPP, USB, PXI)
LIBRARY_IOC_Linux    += handel
LIBRARY_IOC_WIN32    += handel

USR_CFLAGS       += -DEXCLUDE_XUP -DEXCLUDE_SERIAL -DEXCLUDE_VEGA -DEXCLUDE_UDXPS
USR_CFLAGS_Linux += -DEXCLUDE_XMAP -DEXCLUDE_PLX -DLINUX
# For debugging USB problems
#USR_CFLAGS       += -DXERXES_TRACE_IO

# The very old gcc42 compiler does not recognize this flag
ifneq (gcc42, $(findstring gcc42, $(T_A)))
  USR_CFLAGS_Linux    += -Wno-unused-but-set-variable
endif

ifneq ($(LINUX_USB_INSTALLED), YES)
USR_CFLAGS_Linux    += -DEXCLUDE_USB -DEXCLUDE_USB2 -DEXCLUDE_XMAP -DEXCLUDE_PLX -DEXCLUDE_MERCURY
endif
USR_CFLAGS_WIN32    += -DEXCLUDE_EPP
USR_CFLAGS_WIN32    += -DHANDEL_USE_DLL -DHANDEL_MAKE_DLL -DWIN32
ifeq ($(STATIC_BUILD), YES)
USR_CFLAGS          += -DXIA_STATIC_BUILD
endif

ifeq ($(LINUX_USB_INSTALLED), YES)
handel_SRCS_Linux    += xia_usb_linux.c
endif
handel_SRCS += saturn.c saturn_psl.c 
handel_SRCS += mercury.c mercury_psl.c 
handel_SRCS += udxp.c udxp_common.c udxp_psl.c 
handel_SRCS += stj.c stj_psl.c 
handel_SRCS_WIN32 += xmap.c xmap_psl.c 
handel_SRCS_WIN32 += xia_plx.c xia_usb.c xia_usb2.c md_win32.c 
handel_SRCS_Linux += xia_epp_linux.c md_linux.c

handel_SRCS += xerxes.c
handel_SRCS += xerxes_io.c
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
handel_SRCS += handel_run_control.c
handel_SRCS += handel_run_params.c
handel_SRCS += handel_sort.c
handel_SRCS += handel_system.c
handel_SRCS += handel_xerxes.c
handel_SRCS += xia_assert.c
handel_SRCS += xia_file.c

handel_LIBS_WIN32     += PlxApi
handel_SYS_LIBS_WIN32 += setupapi User32
handel_LIBS += $(EPICS_BASE_IOC_LIBS)

ifeq (win32-x86, $(findstring win32-x86, $(T_A)))
  BIN_INSTALLS   += ../plxapi720.dll
  LIB_INSTALLS   += ../PlxApi.lib
endif
ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  BIN_INSTALLS   += ../x64/plxapi720_x64.dll
  LIB_INSTALLS   += ../x64/PlxApi.lib
endif

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
INC += md_generic.h
INC += handel_errors.h

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

