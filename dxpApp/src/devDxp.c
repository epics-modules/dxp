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

static long init(int after);
static long init_record(dxpRecord *pdxp, int *detChan);
static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         char *name, int param, double dvalue,
                         void *pointer);
static void asynCallback(asynUser *pasynUser);
static void readDxpParams(asynUser *pasynUser);

static char *sca_lo[NUM_DXP_SCAS];
static char *sca_hi[NUM_DXP_SCAS];
#define SCA_NAME_LEN 10

static const struct devDxpDset devDxp = {
    5,
    NULL,
    init,
    init_record,
    NULL,
    send_dxp_msg,
};
epicsExportAddress(dset, devDxp);



static long init(int after)
{
    int i;

    if (after) return(0);
    /* Create SCA strings */
    for (i=0; i<NUM_DXP_SCAS; i++) {
        sca_lo[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_lo[i], "sca%d_lo", i);
        sca_hi[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_hi[i], "sca%d_hi", i);
    }
    return(0);
}


static long init_record(dxpRecord *pdxp, int *detChan)
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

    *detChan = pPvt->channel;

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
    devDxpMessage *pmsg;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::send_dxp_msg: command=%d, pact=%d, "
              "name=%s, param=%d, dvalue=%f, pointer=%p\n",
              command, pdxp->pact,
              name, param, dvalue, pointer);

    if (pdxp->pact) {
       return(0);
    }
    pmsg = pasynManager->memMalloc(sizeof(*pmsg));
    pmsg->dxpCommand = command;
    pmsg->name = name;
    pmsg->param = param;
    pmsg->dvalue = dvalue;
    pmsg->pointer = pointer;

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
    int i;

    pasynManager->getAddr(pasynUser, &detChan);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxp::asynCallback: command=%d\n",
              pmsg->dxpCommand);

     switch(pmsg->dxpCommand) {
     case MSG_DXP_START_RUN:
         xiaStartRun(detChan, 0);
         break;
     case MSG_DXP_STOP_RUN:
         xiaStopRun(detChan);
         break; 
     case MSG_DXP_SET_SHORT_PARAM:
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_SET_SHORT_PARAM"
             " calling xiaSetAcquisitionValues name=%s value=%d\n",
             pmsg->name, pmsg->param);
         /* Must stop run before setting parameters.  We should restart if it was running */
         xiaStopRun(detChan);
         /* Note that we use xiaSetAcquisitionValues rather than xiaSetParameter
          * so that the new value will be save with xiaSaveSystem */
         dvalue = pmsg->param;
         xiaSetAcquisitionValues(detChan, pmsg->name, &dvalue);
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_SET_DOUBLE_PARAM:
         /* Must stop run before setting parameters.  We should restart if it was running */
         xiaStopRun(detChan);
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
         else if (pfield == (void *)&pdxp->base_cut_pct) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline cut=%f\n",
                 pmsg->dvalue);
             /* We will be able to use an acquisition value for this in the next release */
             dvalue = pmsg->dvalue/100. * 32768 + 0.5;
             xiaSetAcquisitionValues(detChan, "BLCUT", &dvalue);
         }
         else if (pfield == (void *)&pdxp->base_len) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline filter length=%f\n",
                 pmsg->dvalue);
             /* We will be able to use an acquisition value for this in the next release */ 
             dvalue = 32768./pmsg->dvalue + 0.5;
             xiaSetAcquisitionValues(detChan, "BLFILTER", &dvalue);
         }
         else if (pfield == (void *)&pdxp->base_thresh) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline threshold=%f\n",
                 pmsg->dvalue);
             /* We will be able to use an acquisition value for this in the next release */ 
             xiaSetAcquisitionValues(detChan, "BASETHRESH", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->base_threshadj) {
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting baseline threshold adjust=%f\n",
                 pmsg->dvalue);
             /* We will be able to use an acquisition value for this in the next release */ 
             xiaSetAcquisitionValues(detChan, "BASTHRADJ", &pmsg->dvalue);
         }
         else if (pfield == (void *)&pdxp->emax) {
             if (pdxpReadbacks->number_mca_channels <= 0.)
                 pdxpReadbacks->number_mca_channels = 2048.;
             /* Set the bin width in eV */
             pdxpReadbacks->emax = pmsg->dvalue;
             dvalue = pmsg->dvalue *1000. / pdxpReadbacks->number_mca_channels;
             asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_SET_DOUBLE_PARAM"
                 " setting emax=%f, mca_bin_width=%f\n",
                 pmsg->dvalue, dvalue);
             xiaSetAcquisitionValues(detChan, "mca_bin_width", &dvalue);
         }
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_SET_SCAS:
         /* Must stop run before setting SCAs.  We should restart if it was running */
         xiaStopRun(detChan);
         xiaSetAcquisitionValues(detChan, "number_of_scas", &pmsg->dvalue);
         for (i=0; i<pmsg->dvalue; i++) {
             xiaSetAcquisitionValues(detChan, sca_lo[i], &pdxpReadbacks->sca_lo[i]);
             xiaSetAcquisitionValues(detChan, sca_hi[i], &pdxpReadbacks->sca_hi[i]);
         }
         readDxpParams(pasynUser);
         break;
     case MSG_DXP_READ_BASELINE:
         xiaGetRunData(detChan, "baseline", pdxp->bptr);
         pdxpReadbacks->newBaselineHistogram = 1;
         break;
     case MSG_DXP_CONTROL_TASK:
         /* Must stop run before setting parameters.  We should restart if it was running */
         xiaStopRun(detChan);
         info[0] = 1.;
         info[1] = pmsg->dvalue;
         asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
             "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
             " doing special run=%s, info=%f %f\n",
             pmsg->name, info[0], info[1]);
         xiaDoSpecialRun(detChan, pmsg->name, info);
         if (strcmp(pmsg->name, "adc_trace") == 0) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading adc_trace\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan, "adc_trace", pdxp->tptr);
            pdxpReadbacks->newAdcTrace = 1;
         }
         else if (strcmp(pmsg->name, "baseline_history") == 0) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                 "devDxp::asynCallback, MSG_DXP_CONTROL_TASK"
                 " reading baseline_history\n",
                 pmsg->dvalue);
            xiaGetSpecialRunData(detChan, "baseline_history", pdxp->bhptr);
            pdxpReadbacks->newBaselineHistory = 1;
         }   
         break;
     case MSG_DXP_READ_PARAMS:
         readDxpParams(pasynUser);
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

static void readDxpParams(asynUser *pasynUser)
{
    devDxpPvt *pPvt = (devDxpPvt *)pasynUser->userPvt;
    dxpRecord *pdxp = pPvt->pdxp;
    dxpReadbacks *pdxpReadbacks = pdxp->rbptr;
    int i;
    double number_mca_channels;
    double dvalue;
    int detChan;
    int acquiring;

    pasynManager->getAddr(pasynUser, &detChan);
    xiaGetRunData(detChan, "input_count_rate", &pdxpReadbacks->icr);
    xiaGetRunData(detChan, "output_count_rate", &pdxpReadbacks->ocr);
    /* Convert count rates from kHz to Hz */
    pdxpReadbacks->icr *= 1000.;
    pdxpReadbacks->ocr *= 1000.;
    xiaGetRunData(detChan, "triggers", &pdxpReadbacks->fast_peaks);
    xiaGetRunData(detChan, "events_in_run", &pdxpReadbacks->slow_peaks);
    xiaGetRunData(detChan, "run_active", &acquiring);
    /* run_active returns multiple bits - convert to 0/1 */
    pdxpReadbacks->acquiring = (acquiring != 0);
    xiaGetParamData(detChan, "values", pdxp->pptr);
    /* Only read the following if the channel is not acquiring. They
     * have already been read when they were last changed, and we want
     * to be as efficient as possible */
    if (pdxpReadbacks->acquiring == 0) {
        xiaGetAcquisitionValues(detChan, "energy_threshold",
                                &pdxpReadbacks->slow_trig);
        /* Convert energy thresholds from eV to keV */
        pdxpReadbacks->slow_trig /= 1000.;
        xiaGetAcquisitionValues(detChan, "peaking_time",
                                &pdxpReadbacks->pktim);
        xiaGetAcquisitionValues(detChan, "gap_time",
                                &pdxpReadbacks->gaptim);
        xiaGetAcquisitionValues(detChan, "trigger_threshold",
                                &pdxpReadbacks->fast_trig);
        /* Convert energy thresholds from eV to keV */
        pdxpReadbacks->fast_trig /= 1000.;
        xiaGetAcquisitionValues(detChan, "trigger_peaking_time",
                                &pdxpReadbacks->trig_pktim);
        xiaGetAcquisitionValues(detChan, "trigger_gap_time",
                                &pdxpReadbacks->trig_gaptim);
        xiaGetAcquisitionValues(detChan, "adc_percent_rule",
                                &pdxpReadbacks->adc_rule);
        xiaGetAcquisitionValues(detChan, "mca_bin_width",
                                &pdxpReadbacks->mca_bin_width);
        xiaGetAcquisitionValues(detChan, "number_mca_channels",
                                &number_mca_channels);
        /* If the number of mca channels has changed then recompute mca_bin_width from
         * emax */
        if (number_mca_channels != pdxpReadbacks->number_mca_channels) {
           pdxpReadbacks->number_mca_channels = number_mca_channels;
           if (pdxpReadbacks->number_mca_channels <= 0.)
               pdxpReadbacks->number_mca_channels = 2048.;
           /* Set the bin width in eV */
           dvalue = pdxpReadbacks->emax *1000. / pdxpReadbacks->number_mca_channels;
           asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
               "devDxp::readDxpParams resetting mca_bin_width"
               " setting emax=%f, mca_bin_width=%f\n",
               pdxpReadbacks->emax, dvalue);
           xiaSetAcquisitionValues(detChan, "mca_bin_width", &dvalue);
           xiaGetAcquisitionValues(detChan, "mca_bin_width",
                                   &pdxpReadbacks->mca_bin_width);
        }
 
        /* There seems to be a bug in Handel.  It gives error reading number_of_scas
         * written to, and then it always reads backs as 16, no matter what was written
         * Comment out reading it, just set to 16 for now
         * xiaGetAcquisitionValues(detChan, "number_of_scas",
         *                      &pdxpReadbacks->number_scas); */
        pdxpReadbacks->number_scas = 16;
        for (i=0; i<pdxpReadbacks->number_scas; i++) {
            xiaGetAcquisitionValues(detChan, sca_lo[i], &pdxpReadbacks->sca_lo[i]);
            xiaGetAcquisitionValues(detChan, sca_hi[i], &pdxpReadbacks->sca_hi[i]);
        }
        xiaGetRunData(detChan, "sca", pdxpReadbacks->sca_counts);
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
        "devDxp::readDxpParams\n"
        "input_count_rate:     %f\n"
        "output_count_rate:    %f\n"
        "triggers:             %d\n"
        "events_in_run:        %d\n"
        "run_active:           %d\n"
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
        pdxpReadbacks->fast_peaks, pdxpReadbacks->slow_peaks, acquiring,
        pdxpReadbacks->slow_trig, pdxpReadbacks->pktim,
        pdxpReadbacks->gaptim,
        pdxpReadbacks->fast_trig, pdxpReadbacks->trig_pktim,
        pdxpReadbacks->trig_gaptim,
        pdxpReadbacks->adc_rule, pdxpReadbacks->mca_bin_width,
        pdxpReadbacks->number_mca_channels);
}
