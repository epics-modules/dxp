TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

# This application can be built on Linux (EPP, USB) WIN32 (EPP, USB, PXI), and vxWorks (CAMAC)
LIBRARY_IOC_Linux    += handel
LIBRARY_IOC_vxWorks  += handel
LIBRARY_IOC_cygwin32 += handel
LIBRARY_IOC_WIN32    += handel

ifeq ($(LINUX_USB_INSTALLED), YES)
USR_CFLAGS_Linux    += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C2X -DEXCLUDE_XMAP -DEXCLUDE_PLX -DLINUX
else
USR_CFLAGS_Linux    += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C2X -DEXCLUDE_USB -DEXCLUDE_USB2 -DEXCLUDE_XMAP -DEXCLUDE_PLX -DEXCLUDE_MERCURY -DLINUX
endif
USR_CFLAGS_cygwin32 += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C2X
USR_CFLAGS_WIN32    += -DEXCLUDE_CAMAC -DEXCLUDE_DXP4C2X
USR_CFLAGS_vxWorks  += -DEXCLUDE_SATURN -DEXCLUDE_USB -DEXCLUDE_XMAP -DEXCLUDE_MERCURY -DEXCLUDE_PLX
USR_CFLAGS +=          -DEXCLUDE_UDXP -DEXCLUDE_UDXPS
USR_CFLAGS +=          -DEXCLUDE_SERIAL -DEXCLUDE_VEGA
USR_CFLAGS_WIN32    += -DHANDEL_USE_DLL -DHANDEL_MAKE_DLL -DWIN32
ifeq ($(STATIC_BUILD), YES)
USR_CFLAGS          += -DXIA_STATIC_BUILD
endif


handel_SRCS_vxWorks  += dxp4c2x.c dxp4c2x_psl.c md_vxWorks.c

ifeq ($(LINUX_USB_INSTALLED), YES)
handel_SRCS_Linux    += xia_usb_linux.c
endif
handel_SRCS_Linux    += saturn.c saturn_psl.c 
handel_SRCS_cygwin32 += saturn.c saturn_psl.c 
handel_SRCS_WIN32    += saturn.c saturn_psl.c 
handel_SRCS_cygwin32 += xmap.c xmap_psl.c 
handel_SRCS_WIN32    += xmap.c xmap_psl.c 
handel_SRCS_Linux    += mercury.c mercury_psl.c 
handel_SRCS_cygwin32 += mercury.c mercury_psl.c 
handel_SRCS_WIN32    += mercury.c mercury_psl.c 
handel_SRCS_cygwin32 += xia_epp.c xia_plx.c xia_usb.c xia_usb2.c md_win32.c
handel_SRCS_WIN32    += xia_epp.c xia_plx.c xia_usb.c xia_usb2.c md_win32.c 
handel_SRCS_Linux    += xia_epp_linux.c md_linux.c

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
handel_SRCS += xia_assert.c
handel_SRCS += xia_file.c
handel_SRCS += xia_mem.c
handel_SRCS += xia_mem_point.c

handel_LIBS_WIN32        += DLPORTIO PlxApi
handel_LIBS_cygwin32     += DLPORTIO PlxApi
handel_SYS_LIBS_WIN32    += setupapi User32
handel_SYS_LIBS_cygwin32 += setupapi User32
handel_LIBS += $(EPICS_BASE_IOC_LIBS)
LIB_INSTALLS_WIN32    += ../DLPORTIO.lib ../PlxApi.lib
LIB_INSTALLS_cygwin32 += ../DLPORTIO.lib ../PlxApi.lib
BIN_INSTALLS_WIN32    += ../PlxApi631.dll ../dlportio.dll
BIN_INSTALLS_cygwin32 += ../PlxApi631.dll ../dlportio.dll 
BIN_INSTALLS_WIN32    += ../Plx9054.sys  ../xmap9054.inf ../port95nt.exe ../ezusb.sys ../xiausb.inf ../cyusb.sys ../cyusbme.sys ../xia_usb2.inf
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

