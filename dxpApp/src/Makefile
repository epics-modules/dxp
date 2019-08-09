TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_Linux += dxp
LIBRARY_IOC_WIN32 += dxp

DBD += dxpSupport.dbd
DBD += dxp.dbd

USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The Handel library uses "char *" rather than "const char *" in many places, so we use the following
# flag to turn off compiler warnings when we pass string constants to those functions.
USR_CPPFLAGS_Linux += -Wno-write-strings

# The following are compiled and added to the Support library
dxp_SRCS += NDDxp.cpp
dxp_SRCS += dxpMED.st

dxp_LIBS += handel
dxp_LIBS += asyn
dxp_LIBS += mca
dxp_LIBS += ADBase
dxp_LIBS += seq pv
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

ifeq ($(LINUX_USB_INSTALLED), YES)
PROD_IOC_Linux += dxpApp
endif
PROD_IOC_WIN32 += dxpApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
dxpApp_SRCS_DEFAULT += dxpMain.cpp dxp_registerRecordDeviceDriver.cpp

dxpApp_LIBS += dxp handel
dxpApp_LIBS += mca
dxpApp_LIBS += seq pv

include $(ADCORE)/ADApp/commonDriverMakefile

dxpApp_LIBS_WIN32 += PlxApi
ifeq ($(LINUX_USB_INSTALLED), YES)
dxpApp_SYS_LIBS_Linux += usb
endif
dxpApp_SYS_LIBS_WIN32    += setupapi

# Test applications
#PROD_IOC_Linux += test_scas3
#PROD_IOC_WIN32 += test_scas3
#PROD_IOC_cygwin32 += test_scas3
#test_scas3_SRCS += test_scas3.c
#test_scas3_LIBS += dxp
#test_scas3_LIBS += handel asyn
#test_scas3_SYS_LIBS_Linux += usb
#test_scas3_LIBS += $(EPICS_BASE_IOC_LIBS)
#test_scas3_LIBS_WIN32 += PlxApi
#test_scas3_SYS_LIBS_cygwin32 += PlxApi
#test_scas3_SYS_LIBS_cygwin32 += setupapi
#test_scas3_SYS_LIBS_cygwin32 += $(CYGWIN_RPC_LIB)
#test_scas3_SYS_LIBS_WIN32 += setupapi
#PROD_IOC += test_sscanf
#test_sscanf_SRCS += test_sscanf.c

#===========================
# Build application that lets other apps run with iopl(3)
PROD_IOC_Linux += startWithIopl3
startWithIopl3_SRCS += startWithIopl3.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

