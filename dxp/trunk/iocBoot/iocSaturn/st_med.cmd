# Load the environment variables for the paths to applications
< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("../../dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# Initialize the XIA software
# Set logging level (1=ERROR, 2=WARNING, 3=XXX, 4=DEBUG)
xiaSetLogLevel(2)
# Execute the following line if you have a Vortex detector or 
# another detector with a reset pre-amplifier
xiaInit("vortex.ini")
# Execute the following line if you have a Ketek detector or 
# another detector with an RC pre-amplifier
#xiaInit("ketek.ini")
xiaStartSystem

# DXPConfig(serverName, nchans)
DXPConfig("DXP1",  1)

# DXP record
# Execute the following line if you have a Vortex detector or 
# another detector with a reset pre-amplifier
dbLoadRecords("$(DXP)/dxpApp/Db/dxp2x_reset.db","P=dxpSaturn:med:, R=dxp1, INP=@asyn(DXP1 0)")
# Execute the following line if you have a Ketek detector or 
# another detector with an RC pre-amplifier
#dbLoadRecords("$(DXP)/dxpApp/Db/dxp2x_rc.db","P=dxpSaturn:med:, R=dxp1, INP=@asyn(DXP1 0)")

# MCA record
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=dxpSaturn:med:, M=mca1, DTYP=asynMCA,INP=@asyn(DXP1 0),NCHAN=2048")

# Database to copy MCA ROIs to DXP SCAs
dbLoadTemplate("dxp_sca.substitutions")

# Setup for save_restore
set_savefile_path("autosave")
save_restoreSet_status_prefix("dxpSaturn:med:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "dxpSaturn:med:")
set_pass0_restoreFile(auto_settings.sav)
set_pass1_restoreFile(auto_settings.sav)
set_requestfile_path("./")
set_requestfile_path("$(DXP)/dxpApp/Db")
set_requestfile_path("$(MCA)/mcaApp/Db")

# Debugging flags
#asynSetTraceMask DXP1 0 255
#var mcaRecordDebug 10
#var dxpRecordDebug 10

iocInit

### Start up the autosave task and tell it what to do.

# Save settings every thirty seconds
create_monitor_set("auto_settings_med.req", 30)

