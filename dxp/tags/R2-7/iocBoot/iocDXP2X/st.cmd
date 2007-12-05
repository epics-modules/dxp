# If running on a PowerPC with more than 32MB of memory and not built with long jump ...
# Allocate 96MB of memory temporarily so that all code loads in 32MB.
mem = malloc(1024*1024*96)

# vxWorks startup file
< cdCommands

< ../nfsCommands

cd topbin
ld < dxpApp.munch
cd startup

# Increase size of buffer for error logging from default 1256
errlogInit(50000)

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build.
dbLoadDatabase("$(TOP)/dbd/dxpVX.dbd")
dxpVX_registerRecordDeviceDriver(pdbbase)

putenv("EPICS_TS_MIN_WEST=300")

# Set debugging flags
#mcaRecordDebug = 10
#dxpRecordDebug = 10
#asynSetTraceMask("DXP1",0,255)


# Setup the ksc2917 hardware definitions
# These are all actually the defaults, so this is not really necessary
# num_cards, addrs, ivec, irq_level
ksc2917_setup(1, 0xFF00, 0x00A0, 2)

# Initialize the CAMAC library.  Note that this is normally done automatically
# in iocInit, but we need to get the CAMAC routines working before iocInit
# because we need to initialize the DXP hardware.
camacLibInit

# Initialize XIA Handel
# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("16element_reset.ini")
xiaStartSystem()

# DXPConfig(serverName, nchans)
DXPConfig("DXP1", 16)

# Load all of the MCA and DXP records
dbLoadTemplate("16element.template")

dbLoadRecords("$(DXP)/dxpApp/Db/dxpMED.db","P=dxp2X:med:")

# Generic CAMAC record
dbLoadRecords("$(CAMAC)/camacApp/Db/generic_camac.db","P=dxp2X:,R=camac1,SIZE=2048")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (You need the data catcher
# or the equivalent for that.)  This database is configured to use the
# "alldone" database (above) to figure out when motors have stopped moving
# and it's time to trigger detectors.
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=dxp2X:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")

< ../save_restore.cmd
save_restoreSet_status_prefix("dxp2X:")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=dxp2X:")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")


iocInit

< ../requestFileCommands
create_monitor_set("auto_settings.req",30)

seq &dxpMED, "P=dxp2X:med:, DXP=dxp, MCA=mca, N_DETECTORS=16"

# Free the memory we allocated at the beginning of this script
free(mem)

# This should not be needed, but for some reason output is being turned off by
# iocInit
xiaSetLogOutput("stdout")
xiaSetLogLevel(4)
