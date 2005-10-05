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

USR_CFLAGS += -DXIA_SPECIAL_MEM
USR_CFLAGS += -DEXCLUDE_DGFG200
USR_CFLAGS += -DEXCLUDE_UDXPS

# The following are compiled and added to the Support library
dxp_SRCS += dxpRecord.c
dxp_SRCS += devDxp.c
dxp_SRCS += drvDxp.c
dxp_SRCS += dxpMED.st

dxp_LIBS += handel
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
dxpApp_SRCS_WIN32 += dxp_registerRecordDeviceDriver.cpp
dxpApp_SRCS_vxWorks += dxpVX_registerRecordDeviceDriver.cpp
dxpApp_SRCS_Linux += dxpMain.cpp
dxpApp_SRCS_cygwin32 += dxpMain.cpp
dxpApp_SRCS_WIN32 += dxpMain.cpp
dxpApp_SRCS_vxWorks += -nil-
#The following adds support from base/src/vxWorks
dxpApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

dxpApp_LIBS += dxp handel
dxpApp_LIBS += mca
dxpApp_LIBS += calc sscan autosave
dxpApp_LIBS_vxWorks += camac
dxpApp_LIBS += asyn
dxpApp_SYS_LIBS_cygwin32 += DLPORTIO
dxpApp_SYS_LIBS_cygwin32 += rpc
dxpApp_LIBS += seq pv
dxpApp_LIBS += $(EPICS_BASE_IOC_LIBS)

# xMAP application for cygwin

PROD_IOC_cygwin32 += xmapApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
xmapApp_SRCS_cygwin32 += dxp_registerRecordDeviceDriver.cpp
xmapApp_SRCS_cygwin32 += dxpMain.cpp
xmapApp_SRCS_vxWorks += -nil-

xmapApp_LIBS += dxp
xmapApp_LIBS += mca
xmapApp_LIBS += calc sscan autosave
xmapApp_LIBS += asyn
xmapApp_SYS_LIBS_cygwin32 += xmap_handel
xmapApp_SYS_LIBS_cygwin32 += DLPORTIO
xmapApp_SYS_LIBS_cygwin32 += rpc
xmapApp_LIBS += seq pv
xmapApp_LIBS += $(EPICS_BASE_IOC_LIBS)


# Test applications
PROD_IOC_Linux += test_basethresh
PROD_IOC_cygwin32 += test_basethresh
test_basethresh_SRCS += test_basethresh.c
test_basethresh_LIBS += handel
test_basethresh_SYS_LIBS_cygwin32 += DLPORTIO
test_basethresh_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_set_acquisition_values
test_set_acquisition_values_SRCS += test_set_acquisition_values.c
test_set_acquisition_values_SYS_LIBS += xmap_handel
test_set_acquisition_values_SYS_LIBS += DLPORTIO
test_set_acquisition_values_LIBS += dxp
test_set_acquisition_values_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_scas
test_scas_SRCS += test_scas.c
test_scas_SYS_LIBS += xmap_handel
test_scas_SYS_LIBS += DLPORTIO
test_scas_LIBS += dxp
test_scas_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_read_baseline_history
test_read_baseline_history_SRCS += test_read_baseline_history.c
test_read_baseline_history_SYS_LIBS += xmap_handel
test_read_baseline_history_SYS_LIBS += DLPORTIO
test_read_baseline_history_LIBS += dxp
test_read_baseline_history_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_read_events
test_read_events_SRCS += test_read_events.c
test_read_events_SYS_LIBS += xmap_handel
test_read_events_SYS_LIBS += DLPORTIO
test_read_events_LIBS += dxp
test_read_events_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_get_modules
test_get_modules_SRCS += test_get_modules.c
test_get_modules_SYS_LIBS += xmap_handel
test_get_modules_SYS_LIBS += DLPORTIO
test_get_modules_LIBS += dxp
test_get_modules_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC_cygwin32 += test_sets
test_sets_SRCS += test_sets.c
test_sets_SYS_LIBS += xmap_handel
test_sets_SYS_LIBS += DLPORTIO
test_sets_LIBS += dxp
test_sets_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================
#===========================
# Build application that lets other apps run with iopl(3)
PROD_IOC_Linux += startWithIopl3
startWithIopl3_SRCS += startWithIopl3.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

