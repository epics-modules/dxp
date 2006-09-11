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
LIBRARY_IOC_cygwin32 += dxp
LIBRARY_IOC_WIN32 += dxp

DBDINC += dxpRecord
DBD += dxpSupport.dbd
DBD += dxp.dbd
DBD += dxpVX.dbd
DBD += dxpWin32.dbd

USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The following are compiled and added to the Support library
dxp_SRCS += epicsHandelLock.c
dxp_SRCS += dxpRecord.c
dxp_SRCS += devDxp.c
dxp_SRCS += drvDxp.c
dxp_SRCS += dxpMED.st

dxp_LIBS += handelSrc
dxp_LIBS += mca 
dxp_LIBS += calc sscan autosave
dxp_LIBS += asyn
dxp_LIBS += seq pv
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

PROD_IOC_vxWorks += dxpApp
PROD_IOC_Linux += dxpApp
PROD_IOC_cygwin32 += dxpApp
PROD_IOC_WIN32 += dxpApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
dxpApp_SRCS_Linux += dxp_registerRecordDeviceDriver.cpp
dxpApp_SRCS_cygwin32 += dxp_registerRecordDeviceDriver.cpp
dxpApp_SRCS_WIN32 += dxpWin32_registerRecordDeviceDriver.cpp
dxpApp_SRCS_vxWorks += dxpVX_registerRecordDeviceDriver.cpp
dxpApp_SRCS_Linux += dxpMain.cpp
dxpApp_SRCS_cygwin32 += dxpMain.cpp
dxpApp_SRCS_WIN32 += dxpMain.cpp
dxpApp_SRCS_vxWorks += -nil-
#The following adds support from base/src/vxWorks
dxpApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

dxpApp_LIBS += dxp handelSrc
dxpApp_LIBS += mca
dxpApp_LIBS += calc sscan autosave
dxpApp_LIBS_vxWorks += camac
dxpApp_LIBS += asyn
dxpApp_SYS_LIBS_cygwin32 += DLPORTIO
dxpApp_LIBS_WIN32 += DLPORTIO
dxpApp_SYS_LIBS_cygwin32 += rpc
dxpApp_LIBS += seq pv
dxpApp_LIBS += $(EPICS_BASE_IOC_LIBS)

# xMAP application for cygwin32 and win32

PROD_IOC_cygwin32 += xmapApp
PROD_IOC_WIN32    += xmapApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
xmapApp_SRCS_DEFAULT += dxp_registerRecordDeviceDriver.cpp
xmapApp_SRCS_WIN32   += dxpWin32_registerRecordDeviceDriver.cpp
xmapApp_SRCS += dxpMain.cpp

xmapApp_LIBS += dxp
xmapApp_LIBS += mca
xmapApp_LIBS += calc sscan autosave
xmapApp_LIBS += asyn
xmapApp_SYS_LIBS_cygwin32 += handel
xmapApp_LIBS_WIN32    += handel
xmapApp_SYS_LIBS_cygwin32 += DLPORTIO
xmapApp_LIBS_WIN32    += DLPORTIO
xmapApp_SYS_LIBS_cygwin32 += rpc
xmapApp_LIBS += seq pv
xmapApp_LIBS += $(EPICS_BASE_IOC_LIBS)


# Test applications
PROD_IOC_cygwin32 += test_sets1
test_sets1_SRCS += test_sets1.c
test_sets1_SYS_LIBS += handel
test_sets1_SYS_LIBS += DLPORTIO
test_sets1_LIBS += dxp
test_sets1_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================
#===========================
# Build application that lets other apps run with iopl(3)
PROD_IOC_Linux += startWithIopl3
startWithIopl3_SRCS += startWithIopl3.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

