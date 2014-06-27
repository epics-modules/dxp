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
save_restoreSet_status_prefix("dxpMercury:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxpMercury:")
set_pass0_restoreFile("auto_settings4.sav")
set_pass1_restoreFile("auto_settings4.sav")

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("mercury4.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, maxBuffers, maxMemory)
NDDxpConfig("DXP1",  4, -1, -1)

dbLoadTemplate("4element.substitutions")

# Create a netCDF file saving plugin
NDFileNetCDFConfigure("DXP1NetCDF", 100, 0, DXP1, 0)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=dxpMercury:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",      "P=dxpMercury:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFileNetCDF.template","P=dxpMercury:,R=netCDF1:,PORT=DXP1NetCDF,ADDR=0,TIMEOUT=1")

# Create a TIFF file saving plugin
NDFileTIFFConfigure("DXP1TIFF", 20, 0, "DXP1", 0)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=dxpMercury:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",      "P=dxpMercury:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFileTIFF.template",  "P=dxpMercury:,R=TIFF1:,PORT=DXP1TIFF,ADDR=0,TIMEOUT=1")

# Create a NeXus file saving plugin
NDFileNexusConfigure("DXP1Nexus", 20, 0, "DXP1", 0, 0, 80000)
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDPluginBase.template","P=dxpMercury:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1,NDARRAY_PORT=DXP1,NDARRAY_ADDR=0")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFile.template",      "P=dxpMercury:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1")
dbLoadRecords("$(AREA_DETECTOR)/ADApp/Db/NDFileNexus.template", "P=dxpMercury:,R=Nexus1:,PORT=DXP1Nexus,ADDR=0,TIMEOUT=1")

#xiaSetLogLevel(4)
#asynSetTraceMask DXP1 0 0x11
#asynSetTraceIOMask DXP1 0 2

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxpMercury:,MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

iocInit

seq dxpMED, "P=dxpMercury:, DXP=dxp, MCA=mca, N_DETECTORS=4, N_SCAS=32"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings4.req", 30, "P=dxpMercury:")

### Start the saveData task.
saveData_Init("saveData.req", "P=dxpMercury:")

# Sleep for 15 seconds to let initialization complete and then turn on AutoApply and do Apply manually once
epicsThreadSleep(15.)
# Turn on AutoApply
dbpf("dxpMercury:AutoApply", "Yes")
# Manually do Apply once
dbpf("dxpMercury:Apply", "1")
# Seems to be necessary to resend AutoPixelsPerBuffer to read back correctly from Handel
dbpf("dxpMercury:AutoPixelsPerBuffer.PROC", "1")

