/* dxpRegister.c */

#include <iocsh.h>
#include <epicsExport.h>

#include <xerxes.h>
#include <xia_md.h>

int DXPConfig(const char *serverName, int chan1, int chan2,
                         int chan3, int chan4, int queueSize);

static const iocshArg DXPConfigArg0 = { "server name",iocshArgString};
static const iocshArg DXPConfigArg1 = { "channel 1",iocshArgInt};
static const iocshArg DXPConfigArg2 = { "channel 2",iocshArgInt};
static const iocshArg DXPConfigArg3 = { "channel 3",iocshArgInt};
static const iocshArg DXPConfigArg4 = { "channel 4",iocshArgInt};
static const iocshArg DXPConfigArg5 = { "queue size",iocshArgInt};
static const iocshArg * const DXPConfigArgs[6] = {&DXPConfigArg0,
                                                  &DXPConfigArg1, 
                                                  &DXPConfigArg2, 
                                                  &DXPConfigArg3, 
                                                  &DXPConfigArg4, 
                                                  &DXPConfigArg5}; 
static const iocshFuncDef DXPConfigFuncDef = {"DXPConfig",6,DXPConfigArgs};
static void DXPConfigCallFunc(const iocshArgBuf *args)
{
    DXPConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}

static const iocshArg dxp_logArg0 = { "logging level",iocshArgInt};
static const iocshArg * const dxp_logArgs[1] = {&dxp_logArg0};
static const iocshFuncDef dxp_logFuncDef = {"dxp_md_set_log_level",1,dxp_logArgs};
static void dxp_logCallFunc(const iocshArgBuf *args)
{
    dxp_md_set_log_level(args[0].ival);
}


static const iocshFuncDef dxp_initializeFuncDef = {"dxp_initialize",0,0};
static void dxp_initializeCallFunc(const iocshArgBuf *args)
{
    dxp_initialize();
}

void dxpRegister(void)
{
    iocshRegister(&dxp_initializeFuncDef,dxp_initializeCallFunc);
    iocshRegister(&dxp_logFuncDef,dxp_logCallFunc);
    iocshRegister(&DXPConfigFuncDef,DXPConfigCallFunc);
}

epicsExportRegistrar(dxpRegister);
