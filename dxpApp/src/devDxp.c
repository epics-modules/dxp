/*
    devDxp.c
    Author: Mark Rivers

    This is device support for the DXP record with the asyn driver.

    28-Jun-2004  MLR  Created from old file devDxpMpf.cc that used MPF 

*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <errlog.h>
#include <dbCommon.h>
#include <dbEvent.h>
#include <recSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <cantProceed.h>
#include <epicsExport.h>
#include <epicsString.h>
#include <epicsThread.h>

#include <asynDriver.h>
#include <asynInt32.h>
#include <asynEpicsUtils.h>

#include "dxpRecord.h"
#include "devDxp.h"
#include "handel.h"

typedef struct {
    dxpRecord *pdxp;
    asynUser *pasynUser;
    char *portName;
    int channel;
} devDxpPvt;

typedef struct {
    devDxpCommand dxpCommand;
    char *name;
    short param;
    double dvalue;
    void *pointer;
} devDxpMessage;

static long init_record(dxpRecord *pdxp, int *module);
static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         char *name, int param, double dvalue,
                         void *pointer);
static void asynCallback(asynUser *pasynUser);

static const struct devDxpDset devDxp = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    send_dxp_msg,
};
epicsExportAddress(dset, devDxp);


static long init_record(dxpRecord *pdxp, int *module)
{
    char *userParam;
    asynUser *pasynUser;
    asynStatus status;
    devDxpPvt *pPvt;

    /* Allocate asynDxpPvt private structure */
    pPvt = callocMustSucceed(1, sizeof(devDxpPvt), 
                             "devDxp:: init_record");
     /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(asynCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->pdxp = pdxp;
    pdxp->dpvt = pPvt;

    status = pasynEpicsUtils->parseLink(pasynUser, &pdxp->inp,
                                        &pPvt->portName, &pPvt->channel,
                                        &userParam);
    if (status != asynSuccess) {
        errlogPrintf("devDxp::init_record %s bad link %s\n",
                     pdxp->name, pasynUser->errorMessage);
    }

    *module = pPvt->channel;

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, 
                                         pPvt->channel);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::init_record, connectDevice failed\n");
        goto bad;
    }

    return(0);
    bad:
    pdxp->pact = 1;
    return(0);
}


static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         char *name, int param, double dvalue,
                         void *pointer)
{
    devDxpPvt *pPvt = (devDxpPvt *)pdxp->dpvt;
    asynUser *pasynUser;
    int status;
    devDxpMessage *pmsg = pasynManager->memMalloc(sizeof(*pmsg));

    pmsg->dxpCommand = command;
    pmsg->name = name;
    pmsg->param = param;
    pmsg->dvalue = dvalue;
    pmsg->pointer = pointer;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::send_dxp_msg: command=%d, pact=%d"
              "name=%s, param=%d, dvalue=%f, pointer=%p\n",
              pmsg->dxpCommand, pdxp->pact,
              name, param, dvalue, pointer);

    if (pdxp->pact) {
       return(0);
    }
    pasynUser = pasynManager->duplicateAsynUser(pPvt->pasynUser,
                                                asynCallback, 0);
    pasynUser->userData = pmsg;
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUser, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::send_dxp_msg: error calling queueRequest, %s\n",
                  pasynUser->errorMessage);
        return(-1);
    }
    if (interruptAccept) pdxp->pact = 1;
    return(0);
}


void asynCallback(asynUser *pasynUser)
{
    devDxpPvt *pPvt = (devDxpPvt *)pasynUser->userPvt;
    devDxpMessage *pmsg = pasynUser->userData;
    dxpRecord *pdxp = pPvt->pdxp;
    rset *prset = (rset *)pdxp->rset;
    dxpReadbacks *pdxpReadbacks = pdxp->rbptr;
    int status;
    int detChan;
    void *pfield;
    double info[2];
    double dvalue;

    pasynManager->getAddr(pasynUser, &detChan);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxp::asynCallback: command=%d\n",
              pmsg->dxpCommand);

     switch(pmsg->dxpCommand) {
     case MSG_DXP_SET_SHORT_PARAM:
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_SET_SHORT_PARAM"
             " calling xiaSetAcquisitionValues name=%s value=%d\n",
             pmsg->name, pmsg->param);
         /* Note that we use xiaSetAcquisitionValues rather than xiaSetParameter
          * so that the new value will be save with xiaSaveSystem */
         xiaSetAcquisitionValues(detChan, pmsg->name, &pmsg->param);
         break;
     case MSG_DXP_SET_DOUBLE_PARAM:
         pfield = pmsg->pointer;
         if (pfield == (void *)&pdxp->slow_trig) {
             /* Convert from keV to eV */
             dvalue = pmsg->dvalue * 1000.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting energy_threshold=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "energy_threshold", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->pktim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting peaking_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "peaking_time", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->gaptim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting gap_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "gap_time", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->adc_rule) {
             /* Make our "calibration energy be emax/2. in eV */
             dvalue = pdxp->emax * 1000. / 2.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting calibration_energy=%f\n",
                 dvalue);
             xiaSetAcquisitionValues(detChan, "calibration_energy", &dvalue);
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting adc_percent_rule=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "adc_percent_rule", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->fast_trig) {
             /* Convert from keV to eV */
             dvalue = pmsg->dvalue * 1000.;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_threshold=%f\n",
                 dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_threshold", &dvalue);
         }
         else if (pfield == (void *)&pdxp->trig_pktim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_peaking_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_peaking_time", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->trig_gaptim) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting trigger_gap_time=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "trigger_gap_time", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->emax) {
             if (pdxpReadbacks->number_mca_channels <= 0.)
                 pdxpReadbacks->number_mca_channels = 2048.;
             /* Set the bin width in eV */
             dvalue = pmsg->dvalue *1000. / pdxpReadbacks->number_mca_channels;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting mca_bin_width=%f\n",
                 pmsg->dvalue);
             xiaSetAcquisitionValues(detChan, "mca_bin_width", &dvalue);
         }
         break;
     case MSG_DXP_CONTROL_TASK:
         info[0] = 1.;
         info[1] = pmsg->dvalue;
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
             " doing special run=%s, info=%f %f\n",
             pmsg->name, info[0], info[1]);
         xiaDoSpecialRun(detChan, pmsg->name, info);
         if (strcmp(pmsg->name, "adc_trace") == 0) {
            epicsThreadSleep(0.1);
            xiaStopRun(detChan);
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading adc_trace\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan, "adc_trace", pdxp->tptr);
            pdxpReadbacks->newAdcTrace = 1;
         }
         else if (strcmp(pmsg->name, "baseline_history") == 0) {
            epicsThreadSleep(0.1);
            xiaStopRun(detChan);
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading baseline_history\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan, "baseline_history", pdxp->bhptr);
            pdxpReadbacks->newBaselineHistory = 1;
         }   
         break;
     /* We should probably read all of the parameters after anything is written */
     case MSG_DXP_READ_PARAMS:
         xiaGetRunData(detChan, "baseline", pdxp->bptr);
         pdxpReadbacks->newBaselineHistogram = 1;
         xiaGetRunData(detChan, "input_count_rate", &pdxpReadbacks->icr);
         xiaGetRunData(detChan, "output_count_rate", &pdxpReadbacks->ocr);
         xiaGetRunData(detChan, "triggers", &pdxpReadbacks->fast_peaks);
         xiaGetRunData(detChan, "events_in_run", &pdxpReadbacks->slow_peaks);
         xiaGetParamData(detChan, "values", pdxp->pptr);
         xiaGetAcquisitionValues(detChan, "energy_threshold", 
                                 &pdxpReadbacks->slow_trig);
         xiaGetAcquisitionValues(detChan, "peaking_time", 
                                 &pdxpReadbacks->pktim);
         xiaGetAcquisitionValues(detChan, "gap_time", 
                                 &pdxpReadbacks->gaptim);
         xiaGetAcquisitionValues(detChan, "trigger_threshold", 
                                 &pdxpReadbacks->fast_trig);
         xiaGetAcquisitionValues(detChan, "trigger_peaking_time", 
                                 &pdxpReadbacks->trig_pktim);
         xiaGetAcquisitionValues(detChan, "trigger_gap_time", 
                                 &pdxpReadbacks->trig_gaptim);
         xiaGetAcquisitionValues(detChan, "adc_percent_rule", 
                                 &pdxpReadbacks->adc_rule);
         xiaGetAcquisitionValues(detChan, "mca_bin_width", 
                                 &pdxpReadbacks->mca_bin_width);
         xiaGetAcquisitionValues(detChan, "number_mca_channels", 
                                 &pdxpReadbacks->number_mca_channels);
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_READ_PARAMS\n"
             "input_count_rate:     %f\n"
             "output_count_rate:    %f\n"
             "triggers:             %d\n"
             "events_in_run:        %d\n"
             "energy_threshold:     %f\n"
             "peaking_time:         %f\n"
             "gap_time:             %f\n"
             "trigger_threshold:    %f\n"
             "trigger_peaking_time: %f\n"
             "trigger_gap_time:     %f\n"
             "adc_percent_rule:     %f\n"
             "mca_bin_width:        %f\n"
             "number_mca_channels:  %f\n",
             pdxpReadbacks->icr, pdxpReadbacks->ocr, 
             pdxpReadbacks->fast_peaks, pdxpReadbacks->slow_peaks,
             pdxpReadbacks->slow_trig, pdxpReadbacks->pktim, 
             pdxpReadbacks->gaptim,
             pdxpReadbacks->fast_trig, pdxpReadbacks->trig_pktim, 
             pdxpReadbacks->trig_gaptim,
             pdxpReadbacks->adc_rule, pdxpReadbacks->mca_bin_width,
             pdxpReadbacks->number_mca_channels);
         /* Convert count rates from kHz to Hz */
         pdxpReadbacks->icr *= 1000.;
         pdxpReadbacks->ocr *= 1000.;
         /* Convert energy thresholds from eV to keV */
         pdxpReadbacks->fast_trig /= 1000.;
         pdxpReadbacks->slow_trig /= 1000.;
         break;
     default:
         asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                   "devDxp::asynCallback, invalid command=%d\n",
                   pmsg->dxpCommand);
         break;
     }
    pasynManager->memFree(pmsg, sizeof(*pmsg));
    status = pasynManager->freeAsynUser(pasynUser);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::asynCallback: error in freeAsynUser\n");
    }
    if (pdxp->udf==1) pdxp->udf=0;
    if (!interruptAccept) return;
    dbScanLock((dbCommon *)pdxp);
    (*prset->process)(pdxp);
    dbScanUnlock((dbCommon *)pdxp);
}
