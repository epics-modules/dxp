/*  drvDxp.c

    Author: Mark Rivers
    Date: 16-Jun-2001  Modified from mcaAIMServer.cc

    This module is an asyn driver that is called from devMcaAsyn.c and
    devDxpAsyn.cc and commmunicates with an XIA DXP module.  The code is written
    as an asynchronous driver because:
     - Some calls to the XIA Xerxes software are relatively slow and so require
       that device support be asynchronous
     - If more than one task tries to talk to a single DXP4C module through the
       Xerxes software at the same time bad things happen.  By using one asyn
       port per module we guarantee that access to the module is
       serialized, there will only be one task communicating with each module.
     - The server can perform optimizations, like stopping the module, reading
       and caching the information from each channel, and restarting the
       module.  This can be much more efficient than stopping the module 4
       times, once for each channel.
  
    Modifications:
    10-Feb-2002  MLR  Eliminated call to readoutRun(1) after writing a parameter,
                      rather just set forceRead so next time readoutRun is
                      called it will not use cached values.
    13-Feb-2002  MLR  Fixed bug in setting preset live time or real time.
    10-Mar-2002  MLR  Added support for downloading new FiPPI code
    02-Apr-2002  MLR  Set forceRead when acquiring starts, so that next status read
                      will get true value, not cached value
    03-Apr-2002  MLR  Increase stack size for MPF server tasks from 4000 to 6000. Were
                      getting stack overflows in dxp_replace_fipconfig().
    05-Apr-2002  MLR  Interlock call to dxp_replace_fipconfig() with global mutex
                      to prevent conflicts with global structures in Xerxes.
    27-Jun-2004  MLR  Converted from MPF to asyn
    20-Jul-2004  MLR  C++ to C
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <epicsTime.h>
#include <epicsString.h>
#include <iocsh.h>

#ifdef linux
#include <sys/io.h>
#endif

#include "mca.h"
#include "drvMca.h"
#include "dxp.h"
#include "asynDriver.h"
#include "asynInt32.h"
#include "asynFloat64.h"
#include "asynInt32Array.h"
#include "asynDrvUser.h"
#include "asynDxp.h"
#include "xerxes_structures.h"
#include "xerxes.h"
#include <xia_md.h>


#define MAX_CHANS_PER_DXP 4
#define STOP_DELAY 0.01   /* Number of seconds to wait after issuing stop */
                          /* before reading */
#define DXP4C_CACHE_TIME .2     /* Maximum time to use cached values */
#define DXP2X_CACHE_TIME .1
#define DXPX10P_CACHE_TIME .1

static int numDXP4C=0;
static epicsMutexId dxpMutexId=NULL;
static char *fippiFiles[]={"FIPPI0","FIPPI1","FIPPI2","FIPPI3"};

typedef struct {
    int exists;
    int detChan;
    double preal;
    double ereal;
    double plive;
    double elive;
    double ptotal;
    int ptschan;
    int ptechan;
    double etotal;
    unsigned short *params;
    unsigned short *baseline;
    unsigned long *counts;
} dxpChannel_t;

typedef struct {
    int detChan;
    int moduleType;
    double stopDelay;
    epicsTimeStamp statusTime;
    double maxStatusTime;
    int forceRead;
    dxpChannel_t *dxpChannel;
    int acquiring;
    int erased;
    double ereal;
    double start_time;
    double clockRate;
    unsigned short gate;
    unsigned short livetime_index;
    unsigned short realtime_index;
    unsigned short preset_index;
    unsigned short preset_mode_index;
    unsigned short busy_index;
    unsigned short mcalimhi_index;
    unsigned short nsymbols;
    unsigned int nbase;
    unsigned int nchans;
    char *portName;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
    asynInterface dxp;
} drvDxpPvt;


/* These functions are private, not in any interface */
static dxpChannel_t *findChannel(      drvDxpPvt *pPvt, asynUser *pasynUser, 
                                       int signal);
static void pauseRun(                  drvDxpPvt *pPvt, asynUser *pasynUser);
static void stopRun(                   drvDxpPvt *pPvt, asynUser *pasynUser);
static void resumeRun(                 drvDxpPvt *pPvt, asynUser *pasynUser,
                                       unsigned short resume);
static void readoutRun(                drvDxpPvt *pPvt, asynUser *pasynUser);
static void setPreset(                 drvDxpPvt *pPvt, asynUser *pasynUser,
                                       dxpChannel_t *dxpChan, int mode, 
                                       double time);
static void getAcquisitionStatus(      drvDxpPvt *pPvt, asynUser *pasynUser,
                                       dxpChannel_t *dxpChan);
static double getCurrentTime(          drvDxpPvt *pPvt, asynUser *pasynUser);
static void dxpInterlock(              drvDxpPvt *pPvt, asynUser *pasynUser, 
                                       int state);
static asynStatus drvDxpWrite(         void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus drvDxpRead(          void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *pivalue, epicsFloat64 *pfvalue);

/* These functions are public, exported in interfaces */
static asynStatus int32Write(          void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 value);
static asynStatus int32Read(           void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *value);
static asynStatus getBounds(           void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *low, epicsInt32 *high);
static asynStatus float64Write(        void *drvPvt, asynUser *pasynUser,
                                       epicsFloat64 value);
static asynStatus float64Read(         void *drvPvt, asynUser *pasynUser,
                                       epicsFloat64 *value);
static asynStatus int32ArrayRead(      void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *data, size_t maxChans,
                                       size_t *nactual);
static asynStatus int32ArrayWrite(     void *drvPvt, asynUser *pasynUser,
                                       epicsInt32 *data, size_t maxChans);
static asynStatus dxpSetShortParam(    void *drvPvt, asynUser *pasynUser, 
                                       unsigned short offset, 
                                       unsigned short value);
static asynStatus dxpCalibrate(        void *drvPvt, asynUser *pasynUser,
                                       int ivalue);
static asynStatus dxpReadParams(       void *drvPvt, asynUser *pasynUser, 
                                       short *params, 
                                       short *baseline);
static asynStatus dxpDownloadFippi(    void *drvPvt, asynUser *pasynUser, 
                                       int fippiIndex);
static asynStatus drvUserCreate(       void *drvPvt, asynUser *pasynUser,
                                       const char *drvInfo,
                                       const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType(      void *drvPvt, asynUser *pasynUser,
                                       const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy(      void *drvPvt, asynUser *pasynUser);
static void report(                    void *drvPvt, FILE *fp, int details);
static asynStatus connect(             void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect(          void *drvPvt, asynUser *pasynUser);


/* asynCommon methods */
static const struct asynCommon drvDxpCommon = {
    report,
    connect,
    disconnect
};

/* asynInt32 methods */
static const asynInt32 drvDxpInt32 = {
    int32Write,
    int32Read,
    getBounds
};

/* asynFloat64 methods */
static const asynFloat64 drvDxpFloat64 = {
    float64Write,
    float64Read
};

/* asynInt32Array methods */
static const asynInt32Array drvDxpInt32Array = {
    int32ArrayWrite,
    int32ArrayRead
};

static const asynDrvUser drvDxpDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};

/* dxp methods */
static const asynDxp drvDxpDxp = {
    dxpSetShortParam,
    dxpCalibrate,
    dxpReadParams,
    dxpDownloadFippi
};



int DXPConfig(const char *portName, int chan1, int chan2, 
                         int chan3, int chan4)
{
    /* chan1 - chan4 are the Xerxes channel numbers for the 4 inputs of this
     * module.  If an input is unused or does not exists set chanN=-1
     * moduleType is 0 for DXP4C, 1 for DXP2X, 2 for DXPX10P */
    int allChans[MAX_CHANS_PER_DXP];
    char boardString[MAXBOARDNAME_LEN];
    int i;
    dxpChannel_t *dxpChannel;
    int detChan;
    asynStatus status;
    drvDxpPvt *pPvt;

    pPvt = callocMustSucceed(1, sizeof(*pPvt),  "DXPConfig");

    /* Copy parameters to object private */
    pPvt->portName = epicsStrDup(portName);
    allChans[0] = chan1;
    allChans[1] = chan2;
    allChans[2] = chan3;
    allChans[3] = chan4;
    for (i=0; i<MAX_CHANS_PER_DXP; i++) {
       if (allChans[i] >= 0) {
          dxp_get_board_type(&allChans[i], boardString);
          if (strcmp(boardString, "dxp4c") == 0) pPvt->moduleType = MODEL_DXP4C;
          else if (strcmp(boardString, "dxp4c2x") == 0) pPvt->moduleType = MODEL_DXP2X;
          else if (strcmp(boardString, "dxpx10p") == 0) pPvt->moduleType = MODEL_DXPX10P;
          else errlogPrintf("dxpConfig: unknown board type=%s\n", boardString);
          break;
       }
    }

    /* Keep track if there are any DXP4C modules */
    if (pPvt->moduleType == MODEL_DXP4C) numDXP4C++;
    if (dxpMutexId==NULL) {
       /* Create mutex for all DXP modules */
       dxpMutexId = epicsMutexCreate();
    }
    
    if (pPvt->moduleType == MODEL_DXP4C) {
    	/* DXP4C is 800ns clock period */
    	pPvt->clockRate = 1./ 800.e-9;
    	/* Maximum time to use old data before a new query is sent. */
    	pPvt->maxStatusTime = DXP4C_CACHE_TIME;
    }
    else if (pPvt->moduleType == MODEL_DXP2X) {
    	/* DXP2X is 400ns clock period */
    	pPvt->clockRate = 1./ 400.e-9;
    	/* Maximum time to use old data before a new query is sent. */
    	pPvt->maxStatusTime = DXP2X_CACHE_TIME;
    }
    else if (pPvt->moduleType == MODEL_DXPX10P) {
    	/* DXPX10P is 800ns clock period */
    	pPvt->clockRate = 1./ 800.e-9;
    	/* Maximum time to use old data before a new query is sent. */
    	pPvt->maxStatusTime = DXPX10P_CACHE_TIME;
    }
    pPvt->stopDelay = STOP_DELAY;
    pPvt->acquiring = 0;
    pPvt->ereal = 0.;
    pPvt->erased = 1;
    pPvt->gate   = 1;
    pPvt->dxpChannel = (dxpChannel_t *)
                calloc(MAX_CHANS_PER_DXP, sizeof(dxpChannel_t));
    /* Copy Xerxes channel numbers to structures */
    for (i=0; i<MAX_CHANS_PER_DXP; i++) pPvt->dxpChannel[i].exists = 0;
    /* Allocate memory arrays for each channel if it is valid */
    for (i=0; i<MAX_CHANS_PER_DXP; i++) {
       dxpChannel = &pPvt->dxpChannel[i];
       if (allChans[i] >= 0) {
          detChan = allChans[i];
          dxpChannel->detChan = detChan;
          dxpChannel->exists = 1;
          dxp_max_symbols(&detChan, &pPvt->nsymbols);
          dxp_nbase(&detChan, &pPvt->nbase);
          dxp_nspec(&detChan, &pPvt->nchans);
          /* If pPvt->nchans == 0 then something is wrong, dxp_initialize failed.
           * It would be nice to have a more robust way to determine this.
           * If this happens, return without creating task */
          if (pPvt->nchans == 0) {
             errlogPrintf("dxpConfig: dxp_nspec, chan=%d, nspec=%d\n", 
                           detChan, pPvt->nchans);
             return(-1);
          }
          dxp_get_symbol_index(&detChan, "LIVETIME0", &pPvt->livetime_index);
          if (pPvt->moduleType == MODEL_DXP2X || pPvt->moduleType == MODEL_DXPX10P) {
             dxp_get_symbol_index(&detChan, "REALTIME0", &pPvt->realtime_index);
             dxp_get_symbol_index(&detChan, "PRESETLEN0", &pPvt->preset_index);
             dxp_get_symbol_index(&detChan, "PRESET", &pPvt->preset_mode_index);
             dxp_get_symbol_index(&detChan, "BUSY", &pPvt->busy_index);
             dxp_get_symbol_index(&detChan, "MCALIMHI", &pPvt->mcalimhi_index);
          }             
          dxpChannel->params = (unsigned short *) calloc(pPvt->nsymbols, 
                                      sizeof(unsigned short));
          dxpChannel->baseline = (unsigned short *) calloc(pPvt->nbase, 
                                      sizeof(short));
          dxpChannel->counts = (unsigned long *) calloc(pPvt->nchans, 
                                       sizeof(long));
          /* Set reasonable defaults for other parameters */
          dxpChannel->plive   = 0.;
          dxpChannel->ptotal  = 0.;
          dxpChannel->ptschan = 0;
          dxpChannel->ptechan = 0;
       }
    }
    pPvt->forceRead = 1;  /* Don't use cache first time */

#ifdef linux
    /* The following should only be done on Linux, and is ugly. 
     * Hopefully we can get rid of it*/
    if (pPvt->moduleType == MODEL_DXPX10P) iopl(3);
#endif

    /* Link with higher level routines */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&drvDxpCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&drvDxpInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&drvDxpFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&drvDxpInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&drvDxpDrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    pPvt->dxp.interfaceType = asynDxpType;
    pPvt->dxp.pinterface  = (void *)&drvDxpDxp;
    pPvt->dxp.drvPvt = pPvt;
    status = pasynManager->registerPort(portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register myself.\n");
        return(-1);
    }
    status = pasynManager->registerInterface(portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register common.\n");
        return(-1);
    }
    status = pasynInt32Base->initialize(portName,&pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register int32.\n");
        return(-1);
    }
    status = pasynFloat64Base->initialize(portName,&pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register float64.\n");
        return(-1);
    }
    status = pasynInt32ArrayBase->initialize(portName,&pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register int32Array.\n");
        return(-1);
    }
    status = pasynManager->registerInterface(portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register drvUser.\n");
        return(-1);
    }
    status = pasynManager->registerInterface(portName,&pPvt->dxp);
    if (status != asynSuccess) {
        errlogPrintf("dxpConfig: Can't register dxp.\n");
        return(-1);
    }

    return(0);
}


static dxpChannel_t *findChannel(drvDxpPvt *pPvt, asynUser *pasynUser, 
                                 int signal)
{
    int i;
    dxpChannel_t *dxpChan = NULL;

    /* Find which channel on this module this signal is */
    for (i=0; i<MAX_CHANS_PER_DXP; i++) {
        if (pPvt->dxpChannel[i].exists && pPvt->dxpChannel[i].detChan == signal) { 
            dxpChan = &pPvt->dxpChannel[i];
        }
    }
    if (dxpChan == NULL) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
                 "(drvDxp::findChannel [%s signal=%d]): not a valid channel\n",
                 pPvt->portName, signal);
    }
    return(dxpChan);
}


static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(drvDxpWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(drvDxpWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus drvDxpWrite(void *drvPvt, asynUser *pasynUser,
                              epicsInt32 ivalue, epicsFloat64 dvalue)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    asynStatus status=asynSuccess;
    unsigned short resume;
    int s;
    double time_now;
    int nparams;
    unsigned short short_value;
    int signal;
    dxpChannel_t *dxpChan;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);

    pPvt->detChan = signal;
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "(drvDxpRead %s detChan=%d, command=%d, value=%f\n",
              pPvt->portName, pPvt->detChan, command, dvalue);
    if (dxpChan == NULL) return(asynError);

    switch (command) {
        case mcaStartAcquire:
            /* Start acquisition. 
             * Don't do anything if already acquiring */
            if (!pPvt->acquiring) {
                pPvt->start_time = getCurrentTime(pPvt, pasynUser);
                if (pPvt->erased) resume=0; else resume=1;
                /* The DXP2X could be "acquiring" even though the
                 * flag is clear, because of internal presets.  
                 * Stop run. */
                stopRun(pPvt, pasynUser);
                s = dxp_start_one_run(&pPvt->detChan, &pPvt->gate, &resume);
                pPvt->acquiring = 1;
                pPvt->erased = 0;
                pPvt->forceRead = 1; /* Don't used cached acquiring status next time */
                asynPrint(pasynUser, ASYN_TRACE_FLOW,
                          "(drvDxpPvt [%s detChan=%d]): start acq. status=%d\n",
                          pPvt->portName, pPvt->detChan, s);
            }
            break;
        case mcaStopAcquire:
            /* stop data acquisition */
            stopRun(pPvt, pasynUser);
            if (pPvt->acquiring) {
                time_now = getCurrentTime(pPvt, pasynUser);
                pPvt->ereal += pPvt->start_time - time_now;
                pPvt->start_time = time_now;
                pPvt->acquiring = 0;
            }
            break;
        case mcaErase:
            pPvt->ereal = 0.;
            dxpChan->elive = 0.;
            dxpChan->etotal = 0.;
            /* If DXP is acquiring, turn it off and back on and don't 
             * set the erased flag */
            if (pPvt->acquiring) {
                stopRun(pPvt, pasynUser);
                resume = 0;
                s = dxp_start_one_run(&pPvt->detChan, &pPvt->gate, &resume);
            } else {
                pPvt->erased = 1;
            }
            break;
        case mcaReadStatus:
            getAcquisitionStatus(pPvt, pasynUser, dxpChan);
            break;
        case mcaChannelAdvanceInternal:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for DXP */
            break;
        case mcaChannelAdvanceExternal:
            /* set channel advance source to external */
            /* This is a NOOP for DXP */
            break;
        case mcaNumChannels:
            /* set number of channels
            * On DXP4C nothing to do, always 1024 channels.
            * On DXP2X set MCALIMHI */
            if (pPvt->moduleType == MODEL_DXP2X || 
                pPvt->moduleType == MODEL_DXPX10P) {
                nparams = 1;
               	pPvt->nchans = ivalue;
               	short_value = pPvt->nchans;
      		dxp_download_one_params(&pPvt->detChan, &nparams, 
      		     	                &pPvt->mcalimhi_index, &short_value);
            }
            break;
        case mcaModePHA:
            /* set mode to Pulse Height Analysis */
            /* This is a NOOP for DXP */
            break;
        case mcaModeMCS:
            /* set mode to MultiChannel Scaler */
            /* This is a NOOP for DXP */
            break;
        case mcaModeList:
            /* set mode to LIST (record each incoming event) */
            /* This is a NOOP for DXP */
            break;
        case mcaSequence:
            /* set sequence number */
            /* This is a NOOP for DXP */
            break;
        case mcaPrescale:
            /* This is a NOOP for DXP */
            break;
        case mcaPresetSweeps:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP for DXP */
            break;
        case mcaPresetLowChannel:
            /* set lower side of region integrated for preset counts */
            dxpChan->ptschan = ivalue;
            break;
        case mcaPresetHighChannel:
            /* set high side of region integrated for preset counts */
            dxpChan->ptechan = ivalue;
            break;
        case mcaDwellTime:
            /* set dwell time */
            /* This is a NOOP for DXP */
            break;
        case mcaPresetRealTime:
            /* set preset real time. */
            setPreset(pPvt, pasynUser, dxpChan, mcaPresetRealTime, dvalue);
            break;
        case mcaPresetLiveTime:
            /* set preset live time. */
            setPreset(pPvt, pasynUser, dxpChan, mcaPresetLiveTime, dvalue);
            break;
        case mcaPresetCounts:
            /* set preset counts */
            dxpChan->ptotal = ivalue;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvDXPWrite, invalid command=%d\n", command);
            status = asynError;
            break;
    }
    return(status);
}

static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(drvDxpRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(drvDxpRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus drvDxpRead(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    asynStatus status=asynSuccess;
    int signal;
    dxpChannel_t *dxpChan;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);
    pPvt->detChan = signal;
    if (dxpChan == NULL) return(asynError);

    switch (command) {
        case mcaAcquiring:
            *pivalue = pPvt->acquiring;
            break;
        case mcaDwellTime:
            *pfvalue = 0.;
            break;
        case mcaElapsedLiveTime:
            *pfvalue = pPvt->dxpChannel->elive;
            break;
        case mcaElapsedRealTime:
            *pfvalue = pPvt->dxpChannel->ereal;
            break;
        case mcaElapsedCounts:
            *pfvalue = dxpChan->etotal;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvDxp::drvDxpRead got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    return(status);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 0;
    return(asynSuccess);
}

static asynStatus int32ArrayRead(void *drvPvt, asynUser *pasynUser, 
                                 epicsInt32 *data, size_t maxChans, 
                                 size_t *nactual)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    int signal;
    dxpChannel_t *dxpChan;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);
    if (dxpChan == NULL) return(asynError);

    if (pPvt->erased) {
        memset(data, 0, pPvt->nchans*sizeof(int));
    } else {
        readoutRun(pPvt, pasynUser);
        memcpy(data, dxpChan->counts, pPvt->nchans*sizeof(int));
    }
    *nactual = pPvt->nchans;
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                               epicsInt32 *data, size_t maxChans)
{
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvDxp::mcaWriteData, write operations not allowed\n");
    return(asynError);
}


static asynStatus dxpReadParams(void *drvPvt, asynUser *pasynUser, 
                         short *params, short *baseline)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    int signal;
    dxpChannel_t *dxpChan;

    pasynManager->getAddr(pasynUser, &signal);
    dxpChan = findChannel(pPvt, pasynUser, signal);
    if (dxpChan == NULL) return(asynError);
    pPvt->detChan = signal;

    readoutRun(pPvt, pasynUser);
    memcpy(params, dxpChan->params, pPvt->nsymbols*sizeof(unsigned short));
    memcpy(baseline, dxpChan->baseline, pPvt->nbase*sizeof(unsigned short));
    return(asynSuccess);
}

static asynStatus dxpSetShortParam(void *drvPvt, asynUser *pasynUser, 
                            unsigned short offset, unsigned short value)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    int signal;
    int nparams;

    pasynManager->getAddr(pasynUser, &signal);
    pPvt->detChan = signal;
    /* Set a short parameter */
    pauseRun(pPvt, pasynUser);
    nparams = 1;
    dxp_download_one_params(&signal, &nparams, &offset, &value);
    resumeRun(pPvt, pasynUser, 1);
    pPvt->forceRead = 1; /* Force a read of new parameters next time */
    return(asynSuccess);
}

static asynStatus dxpCalibrate(void *drvPvt, asynUser *pasynUser,
                                        int ivalue)
{
    /* Calibrate */
    int signal;
    pasynManager->getAddr(pasynUser, &signal);
    dxp_calibrate_one_channel(&signal, &ivalue);
    return(asynSuccess);
}

static asynStatus dxpDownloadFippi(void *drvPvt, asynUser *pasynUser, 
                                            int fippiIndex)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;
    int signal;
    pasynManager->getAddr(pasynUser, &signal);

    pPvt->detChan = signal;
    /* Download new FiPPI file */
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "(downloadFippi [%s detChan=%d]): "
              "download FiPPI file=%s\n",
              pPvt->portName, pPvt->detChan, fippiFiles[fippiIndex]);
    /* dxp_replace_fipconfig modifies global structures in Xerxes
     * (reading FiPPI files, etc.).  There must be only 1 task doing 
     * this at once.  Take the mutex that blocks all other DXP tasks
     */
    epicsMutexLock(dxpMutexId);
    dxp_replace_fipconfig(&signal, fippiFiles[fippiIndex]);
    epicsMutexUnlock(dxpMutexId);
    return(asynSuccess);
}


static void pauseRun(drvDxpPvt *pPvt, asynUser *pasynUser)
{
   /* Pause the run if the module is acquiring. Wait for a short interval for 
    * DXP4C to finish processing */
   /* Don't do anything on DXP-2X, since this function is only needed
    * to read out module when acquiring, which is not a problem on DXP2X. */
   if (pPvt->moduleType == MODEL_DXP2X || 
       pPvt->moduleType == MODEL_DXPX10P) return;
   if (pPvt->acquiring) dxp_stop_one_run(&pPvt->detChan);
   epicsThreadSleep(pPvt->stopDelay);
}

static void stopRun(drvDxpPvt *pPvt, asynUser *pasynUser)
{
   /* Stop the run.  Wait for a short interval to finish processing */
   dxp_stop_one_run(&pPvt->detChan);
   /* On both the DXP4C and DXP2X wait a short time, so other commands won't
    * come in before module is done processing 
    * Should check Busy parameter on DXP2X. */
   epicsThreadSleep(pPvt->stopDelay);
}

static void resumeRun(drvDxpPvt *pPvt, asynUser *pasynUser, 
                      unsigned short resume)
{
   /* Resume the run.  resume=0 to erase, 1 to not erase. */
   /* Don't do anything on DXP-2X, since this function is only needed to resume
    * a run after pauseRun, used to read out module when acquiring */
   if (pPvt->moduleType == MODEL_DXP2X || 
       pPvt->moduleType == MODEL_DXPX10P) return;
   if (pPvt->acquiring) dxp_start_one_run(&pPvt->detChan, &pPvt->gate, &resume);
}

static void dxpInterlock(drvDxpPvt *pPvt, asynUser *pasynUser, int state)
{
   /* This function takes (state=1) or releases (state=0) a mutex blocking
    * access to all DXP modules.  Its purpose is to increase the performance of
    * readoutRUN() whenever there are any DXP4C modules in the system.  Without this
    * interlock all of the mcDXPServer tasks are accessing the crate "simultaneously".
    * This is not a resource conflict at the hardware level, because the CAMAC driver
    * handles that.  However, it is a performance problem, because readoutRun stops
    * acquisition on a DXP4C (with pauseRun) and then reads out all 4 channels.  The
    * module is "dead" during this time, not acquiring.  We want to minimize this
    * time.  If other DXPs are being accessed during this time, they are increasing
    * the dead time for that module.  The solution is to prevent other DXP server tasks
    * from reading their modules while this module is disabled.  The mutex does
    * this.  This mutex is not needed if there are no DXP4C modules in the system
    * (i.e. all modules are DXP2X) because the DXP2X can be read out while it is
    * acquiring and pauseRun() does nothing on the DPX4 DXP2X. */
   if (numDXP4C == 0) return;
   if (state) epicsMutexLock(dxpMutexId);
   else       epicsMutexUnlock(dxpMutexId);
}

static void readoutRun(drvDxpPvt *pPvt, asynUser *pasynUser)
{
   dxpChannel_t *dxpChan;
   int i, s;
   epicsTimeStamp tstart, now;

   /* Readout the run if it has not been done "recently */
   epicsTimeGetCurrent(&now);
   if (!pPvt->forceRead && 
       (epicsTimeDiffInSeconds(&now, &pPvt->statusTime) < pPvt->maxStatusTime)) 
       return;
   dxpInterlock(pPvt, pasynUser, 1);
   pauseRun(pPvt, pasynUser);
   epicsTimeGetCurrent(&tstart);
   for (i=0; i<MAX_CHANS_PER_DXP; i++) {
      dxpChan = &pPvt->dxpChannel[i];
      if (dxpChan->exists) {
         s=dxp_readout_detector_run(&dxpChan->detChan, dxpChan->params, 
                                    dxpChan->baseline, dxpChan->counts);
      }
   }
   resumeRun(pPvt, pasynUser, 1);
   epicsTimeGetCurrent(&pPvt->statusTime);
   pPvt->forceRead = 0;
   asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "(readoutRun for det. %d, time=%f\n", 
             pPvt->detChan, epicsTimeDiffInSeconds(&pPvt->statusTime, &tstart));
   dxpInterlock(pPvt, pasynUser, 0);
}

static double getCurrentTime(drvDxpPvt *pPvt, asynUser *pasynUser)
{
    struct timespec timespec;
    clock_gettime(CLOCK_REALTIME, &timespec);
    return (timespec.tv_sec + timespec.tv_nsec*1.e-9);
}


static void getAcquisitionStatus(drvDxpPvt *pPvt, asynUser *pasynUser, 
                                 dxpChannel_t *dxpChan) 
{
   int i;
   double time_now;

   if (pPvt->erased) {
      pPvt->dxpChannel->elive = 0.;
      pPvt->dxpChannel->ereal = 0.;
      dxpChan->etotal = 0.;
      pPvt->ereal = 0.;
   } else {
      readoutRun(pPvt, pasynUser);
      dxpChan->etotal =  0.;
      switch(pPvt->moduleType) {
         case MODEL_DXP4C:
            if (pPvt->acquiring) {
               time_now = getCurrentTime(pPvt, pasynUser);
               pPvt->ereal += time_now - pPvt->start_time;
               dxpChan->ereal = pPvt->ereal;
               pPvt->start_time = time_now;
            }
            /* Compute livetime from the parameter array */
            pPvt->dxpChannel->elive = 
               ((dxpChan->params[pPvt->livetime_index]<<16) +
               dxpChan->params[pPvt->livetime_index+1]) /
               pPvt->clockRate;
            break;
         case MODEL_DXP2X:
         case MODEL_DXPX10P:
            /* Compute livetime from the parameter array. The DXP2X
             * uses 3 words to store it.*/
            pPvt->dxpChannel->elive = 
               ((dxpChan->params[pPvt->livetime_index+2] * 4294967296.) +
               (dxpChan->params[pPvt->livetime_index] * 65536.) +
               (dxpChan->params[pPvt->livetime_index+1])) /
               pPvt->clockRate;
            /* Compute realtime from the parameter array. The DXP2X
             * uses 3 words to store it.*/
            pPvt->dxpChannel->ereal = 
               ((dxpChan->params[pPvt->realtime_index+2] * 4294967296.) +
               (dxpChan->params[pPvt->realtime_index] * 65536.) +
               (dxpChan->params[pPvt->realtime_index+1])) /
               pPvt->clockRate;
            break;
      }
      /* Compute counts in preset count window */
      if ((dxpChan->ptschan > 0) &&
          (dxpChan->ptechan > 0) &&
          (dxpChan->ptechan > dxpChan->ptschan)) {
         for (i=dxpChan->ptschan; i<=dxpChan->ptechan; i++) {
              dxpChan->etotal += dxpChan->counts[i];
         }
      }
      /* See if any of the presets have been exceeded.  The DXP2X handles
       * preset live and real time internally, the DXP4C does not */
      
      switch (pPvt->moduleType) {
      case MODEL_DXP4C:
         if (((pPvt->dxpChannel->preal != 0.0) && 
              (pPvt->ereal > pPvt->dxpChannel->preal)) ||
             ((pPvt->dxpChannel->plive != 0.0) &&
              (pPvt->dxpChannel->elive > pPvt->dxpChannel->plive)) ||
             ((dxpChan->ptotal != 0.0) && 
              (dxpChan->etotal > dxpChan->ptotal))) {
            stopRun(pPvt, pasynUser);
            pPvt->acquiring = 0;
         }
         break;
      case MODEL_DXP2X:
      case MODEL_DXPX10P:
         if ((dxpChan->params[pPvt->busy_index] == 0) || 
             (dxpChan->params[pPvt->busy_index] == 99)) {
            pPvt->acquiring = 0;
         } else {
            pPvt->acquiring = 1;
         }
         if ((dxpChan->ptotal != 0.0) && 
             (dxpChan->etotal > dxpChan->ptotal)) {
            stopRun(pPvt, pasynUser);
            pPvt->acquiring = 0;
         }
         break;
      }
   }
   asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "(getAcquisitionStatus [%s detChan=%d]): acquiring=%d\n",
             pPvt->portName, pPvt->detChan, pPvt->acquiring);
}



static void setPreset(drvDxpPvt *pPvt, asynUser *pasynUser,
                      dxpChannel_t *dxpChan, 
                      int mode, double time)
{
   int nparams=3;
   unsigned short offsets[3];
   unsigned short values[3];
   unsigned short time_mode=0;
   
   switch (mode) {
      case mcaPresetRealTime:
         dxpChan->preal = time;
         break;
      case mcaPresetLiveTime:
         dxpChan->plive = time;
         break;
   }
   
   if (pPvt->moduleType == MODEL_DXP2X || pPvt->moduleType == MODEL_DXPX10P) {
      /* If preset live and real time are both zero set to count forever */
      if ((dxpChan->plive == 0.) && (dxpChan->preal == 0.)) time_mode = 0;
      /* If preset live time is zero and real time is non-zero use real time */
      if ((dxpChan->plive == 0.) && (dxpChan->preal != 0.)) {
         time = dxpChan->preal;
         time_mode = 1;
      }
      /* If preset live time is non-zero use live time */
      if (dxpChan->plive != 0.) {
         time = dxpChan->plive;
         time_mode = 2;
      }
      values[0] = ((int)(time*pPvt->clockRate)) >> 16;
      offsets[0] = pPvt->preset_index;
      values[1] = ((int)(time*pPvt->clockRate)) & 0xffff;
      offsets[1] = pPvt->preset_index+1;
      values[2] = time_mode;
      offsets[2] = pPvt->preset_mode_index;
      dxp_download_one_params(&pPvt->detChan, &nparams, offsets, values);
   }
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    char *pstring;

    for (i=0; i<MAX_MCA_COMMANDS; i++) {
        pstring = mcaCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = mcaCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(mcaCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvDxp::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvDxp::drvUserCreate, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    mcaCommand command = pasynUser->reason;

    *pptypeName = NULL;
    *psize = 0;
    if (pptypeName)
        *pptypeName = epicsStrDup(mcaCommands[command].commandString);
    if (psize) *psize = sizeof(command);
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
void report(void *drvPvt, FILE *fp, int details)
{
    drvDxpPvt *pPvt = (drvDxpPvt *)drvPvt;

    fprintf(fp, "drvDxpPvt %s: connected\n", pPvt->portName);
    if (details >= 1) {
        fprintf(fp, "              nchans: %d\n", pPvt->nchans);
    }
}

/* Connect */
asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Disconnect */
asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* iocsh functions */
int DXPConfig(const char *serverName, int chan1, int chan2,
                         int chan3, int chan4);

static const iocshArg DXPConfigArg0 = { "server name",iocshArgString};
static const iocshArg DXPConfigArg1 = { "channel 1",iocshArgInt};
static const iocshArg DXPConfigArg2 = { "channel 2",iocshArgInt};
static const iocshArg DXPConfigArg3 = { "channel 3",iocshArgInt};
static const iocshArg DXPConfigArg4 = { "channel 4",iocshArgInt};
static const iocshArg * const DXPConfigArgs[5] = {&DXPConfigArg0,
                                                  &DXPConfigArg1, 
                                                  &DXPConfigArg2, 
                                                  &DXPConfigArg3, 
                                                  &DXPConfigArg4}; 
static const iocshFuncDef DXPConfigFuncDef = {"DXPConfig",5,DXPConfigArgs};
static void DXPConfigCallFunc(const iocshArgBuf *args)
{
    DXPConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival, 
              args[4].ival);
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

