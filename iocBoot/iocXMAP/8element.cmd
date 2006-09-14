< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DXP)/dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpXMAP:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpXMAP:")
set_pass0_restoreFile("auto_settings8.sav")
set_pass1_restoreFile("auto_settings8.sav")

# Set logging level (1=ERROR, 2=WARNING, 3=XXX, 4=DEBUG)
xiaSetLogLevel(2)
# Execute the following line if you have a Vortex detector or
# another detector with a reset pre-amplifier
xiaInit("xmap8.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, ngroups, pollFrequency)
DXPConfig("DXP1",  8, 1, 100)

dbLoadTemplate("8element.template")

#xiaSetLogLevel(4)
#asynSetTraceMask DXP1   0 255
#asynSetTraceIOMask DXP1 0 2
##asynSetTraceMask DXP1   1 255
#asynSetTraceIOMask DXP1 1 2
#asynSetTraceMask DXP1   2 255
#asynSetTraceIOMask DXP1 2 2
#asynSetTraceMask DXP1   3 255
#asynSetTraceIOMask DXP1 3 2
#asynSetTraceMask DXP1   4 255
#asynSetTraceIOMask DXP1 4 2
#asynSetTraceMask DXP1   5 255
#asynSetTraceIOMask DXP1 5 2
#asynSetTraceMask DXP1   6 255
#asynSetTraceIOMask DXP1 6 2
#asynSetTraceMask DXP1   7 255
#asynSetTraceIOMask DXP1 7 2
#var dxpRecordDebug 10

iocInit

seq dxpMED, "P=dxpXMAP:med:, DXP=dxp, MCA=mca, N_DETECTORS=8"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings8.req", 30)

