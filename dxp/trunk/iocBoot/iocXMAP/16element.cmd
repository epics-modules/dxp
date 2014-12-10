< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DXP)/dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

# The default callback queue in EPICS base is only 2000 bytes. 
# The dxp detector system needs this to be larger to avoid the error message: 
# "callbackRequest: cbLow ring buffer full" 
# right after the epicsThreadSleep at the end of this script
callbackSetQueueSize(4000)

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("dxpXMAP:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpXMAP:")
set_pass0_restoreFile("auto_settings16.sav")
set_pass1_restoreFile("auto_settings16.sav")

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("xmap16.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, maxBuffers, maxMemory)
NDDxpConfig("DXP1",  16, -1, -1)

dbLoadTemplate("16element.substitutions")

# Create a netCDF file saving plugin
NDFileNetCDFConfigure("DXP1NetCDF", 100, 0, "DXP1", 0)
dbLoadRecords("$(ADCORE)/db/NDPluginBase.template","P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(ADCORE)/db/NDFile.template",      "P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADCORE)/db/NDFileNetCDF.template","P=dxpXMAP:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")

# Create a TIFF file saving plugin
NDFileTIFFConfigure("DXP1TIFF", 20, 0, "DXP1", 0)
dbLoadRecords("$(ADCORE)/db/NDPluginBase.template","P=dxpXMAP:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(ADCORE)/db/NDFile.template",      "P=dxpXMAP:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADCORE)/db/NDFileTIFF.template",  "P=dxpXMAP:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1")

# Create a NeXus file saving plugin
NDFileNexusConfigure("DXP1Nexus", 20, 0, "DXP1", 0, 0, 80000)
dbLoadRecords("$(ADCORE)/db/NDPluginBase.template","P=dxpXMAP:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(ADCORE)/db/NDFile.template",      "P=dxpXMAP:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(ADCORE)/db/NDFileNexus.template", "P=dxpXMAP:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1")


#xiaSetLogLevel(4)
#asynSetTraceMask DXP1 0 0x11
#asynSetTraceIOMask DXP1 0 2

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxpXMAP:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

iocInit

seq dxpMED, "P=dxpXMAP:, DXP=dxp, MCA=mca, N_DETECTORS=16, N_SCAS=32"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings16.req", 30, "P=dxpXMAP:")

### Start the saveData task.
saveData_Init("saveData.req", "P=dxpXMAP:")

# Sleep for 10 seconds to let initialization complete and then turn on AutoApply and do Apply manually once
epicsThreadSleep(10.)
# Turn on AutoApply
dbpf("dxpXMAP:AutoApply", "Yes")
# Manually do Apply once
dbpf("dxpXMAP:Apply", "1")
# Seems to be necessary to resend AutoPixelsPerBuffer to read back correctly from Handel
dbpf("dxpXMAP:AutoPixelsPerBuffer.PROC", "1")

