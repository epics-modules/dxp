epicsEnvSet(MCA,"/home/dxp/mca")
epicsEnvSet(DXP,"/home/dxp/dxp")

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("../../dbd/dxp.dbd")
dxp_registerRecordDeviceDriver(pdbbase)

routerInit
localMessageRouterStart(0)

var(dxpRecordDebug,0)
var(mcaDXPServerDebug,0)
var(devDxpMpfDebug,0)
var

# DXP and mca records for the Vortex detector

# Change the following environment variable to point to the desired
# system configuration file
epicsEnvSet("XIA_CONFIG", "xiasystem.cfg")

# Set environment variables for the FiPPI files. 
# FIPPI0,FIPPI1,FIPPI2,FIPPI3 should point to the files for 
# decimation=0,2,4,6 respectively. FIPPI_DEFAULT should point to the
# file that will be loaded initially at boot time.
epicsEnvSet("FIPPI0", "f01xpd0j.fip")
epicsEnvSet("FIPPI1", "f01xpd2j.fip")
epicsEnvSet("FIPPI2", "f01xpd4j.fip")
epicsEnvSet("FIPPI3", "f01xpd6j.fip")
epicsEnvSet("FIPPI_DEFAULT", "f01xpd4j.fip")

# Set logging level (1=ERROR, 2=WARNING, 3=XXX, 4=DEBUG)
dxp_md_set_log_level(2)

dxp_initialize

# DXPConfig(serverName, detChan1, detChan2, detChan3, detChan4, queueSize)
DXPConfig("DXP1",  0,  -1, -1, -1, 300)

dbLoadRecords("$(DXP)/dxpApp/Db/dxp2x.db","P=dxpLinux:med:, R=dxp1, INP=#C0 S0  @DXP1")
dbLoadRecords("$(MCA)/mcaApp/Db/simple_mca.db", "P=dxpLinux:med:,M=mca1,DTYP=MPF MCA,INP=#C0 S0 @DXP1,PREC=2,CHANS=2048")
dbLoadRecords("$(DXP)/dxpApp/Db/1element_dxp.db","P=dxpLinux:med:")

set_pass0_restoreFile(auto_settings_med.sav)
set_pass1_restoreFile(auto_settings_med.sav)
iocInit

### Start up the autosave task and tell it what to do.
# The task is actually named "save_restore".
# (See also, 'initHooks' above, which is the means by which the values that
# will be saved by the task we're starting here are going to be restored.
#
# Load the list of search directories for request files
set_requestfile_path("./")
set_requestfile_path("$(DXP)/dxpApp/Db")
set_requestfile_path("$(MCA)/mcaApp/Db")

# save positions every five seconds
#create_monitor_set("auto_positions.req", 5)
# save other things every thirty seconds
create_monitor_set("auto_settings_med.req", 30)

