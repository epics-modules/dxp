# Allocate 96MB of memory temporarily so that all code loads in 32MB.
mem = malloc(1024*1024*96)

# vxWorks startup file
< cdCommands

< ../nfsCommands

cd topbin
ld < dxpApp.munch
cd startup

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from CARSApp
dbLoadDatabase("$(TOP)/dbd/dxpVX.dbd")
dxpVX_registerRecordDeviceDriver(pdbbase)

# Initialize MPF stuff
routerInit

# Initialize local MPF connection
localMessageRouterStart(0)

# Set debugging flags
mcaRecordDebug = 0
dxpRecordDebug=0
mcaDXPServerDebug=0
devDxpMpfDebug=0

# Setup the ksc2917 hardware definitions
# These are all actually the defaults, so this is not really necessary
# num_cards, addrs, ivec, irq_level
ksc2917_setup(1, 0xFF00, 0x00A0, 2)

# Initialize the CAMAC library.  Note that this is normally done automatically
# in iocInit, but we need to get the CAMAC routines working before iocInit
# because we need to initialize the DXP hardware.
camacLibInit

# Load the DXP stuff
< 16element.cmd

# Generic CAMAC record
dbLoadRecords("$(CAMAC)/camacApp/Db/generic_camac.db","P=13GE2:,R=camac1,SIZE=2048")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (You need the data catcher
# or the equivalent for that.)  This database is configured to use the
# "alldone" database (above) to figure out when motors have stopped moving
# and it's time to trigger detectors.
dbLoadRecords("$(STD)/stdApp/Db/scan.db","P=13GE2:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")

################################################################################
# Setup device/driver support addresses, interrupt vectors, etc.

# dbrestore setup
sr_restore_incomplete_sets_ok = 1
#reboot_restoreDebug=5

iocInit

#Reset the CAMAC crate - may not want to do this after things are all working
#ext = 0
#cdreg &ext, 0, 0, 1, 0
#cccz ext


### Start up the autosave task and tell it what to do.
# The task is actually named "save_restore".
# (See also, 'initHooks' above, which is the means by which the values that
# will be saved by the task we're starting here are going to be restored.

< ../requestFileCommands
#
# save positions every five seconds
#create_monitor_set("auto_positions.req",5)
# save other things every thirty seconds
create_monitor_set("auto_settings.req",30)

# Free the memory we allocated at the beginning of this script
free(mem)
