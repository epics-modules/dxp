# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("../../dbd/dxp.dbd")
registerRecordDeviceDriver(pdbbase)

routerInit
localMessageRouterStart(0)

setSymbol(dxpRecordDebug,0)
setSymbol(mcaDXPServerDebug,0)
setSymbol(devDxpMpfDebug,0)
showSymbol

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

dbLoadRecords("../../../dxp/dxpApp/Db/dxp2x.db","P=dxpLinux:, R=dxp1, INP=#C0 S0  @DXP1")
dbLoadRecords("../../../mca/mcaApp/Db/mca.db", "P=dxpLinux:,M=dxp_adc1,DTYPE=MPF MCA,INP=#C0 S0 @DXP1,NCHAN= 2048")

iocInit

### Start up the autosave task and tell it what to do.
# The task is actually named "save_restore".
# (See also, 'initHooks' above, which is the means by which the values that
# will be saved by the task we're starting here are going to be restored.
#
# Load the list of search directories for request files
< ../requestFileCommands

# save positions every five seconds
create_monitor_set("auto_positions.req", 5)
# save other things every thirty seconds
create_monitor_set("auto_settings.req", 30)

