/*
    devDxpAsyn.c
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
#include <recSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <cantProceed.h>
#include <epicsExport.h>
#include <epicsString.h>
#include <epicsMessageQueue.h>

#include <asynDriver.h>

#include "dxpRecord.h"
#include "drvMcaAsyn.h"
#include "devDxpAsyn.h"
#include "drvDxpAsyn.h"
#include "xerxes_structures.h"
#include "xerxes.h"

typedef struct {
    dxpRecord *pdxp;
    asynUser *pasynUser;
    char *portName;
    epicsMessageQueueId msgQueueId;
    int channel;
    asynMca *pasynMcaInterface;
    void *asynMcaInterfacePvt;
    asynDxp *pasynDxpInterface;
    void *asynDxpInterfacePvt;
    unsigned short nsymbols;
    unsigned int nbase;
    unsigned short *params;
    unsigned short *baseline;
} devDxpAsynPvt;

typedef enum {
   mcaMessage,
   dxpMessage
} dxpMessage_t;

/* For now we use a fixed-size message queue.  This will be fixed later */
#define MSG_QUEUE_SIZE 100
typedef struct {
    dxpMessage_t type;
    mcaCommand mcaCommand;
    devDxpCommand dxpCommand;
    int value1;
    int value2;
} devDxpAsynMessage;

static long init_record(dxpRecord *pdxp);
static long send_mca_msg(dxpRecord *pdxp, mcaCommand command); 
static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         int v1, int v2);
static long send_msg(dxpRecord *pdxp, devDxpAsynMessage msg); 
static long read_array(dxpRecord *pdxp);
static void asynCallback(asynUser *pasynUser);

static const struct devDxpDset devDxpAsyn = {
    7,
    NULL,
    NULL,
    init_record,
    NULL,
    send_mca_msg,
    send_dxp_msg,
    read_array
};
epicsExportAddress(dset, devDxpAsyn);


static long init_record(dxpRecord *pdxp)
{
    struct vmeio *pvmeio;
    asynUser *pasynUser;
    int addr=0;
    asynStatus status;
    asynInterface *pasynInterface;
    devDxpAsynPvt *pPvt;

    /* Allocate asynMcaPvt private structure */
    pPvt = callocMustSucceed(1, sizeof(devDxpAsynPvt), 
                             "devDxpAsyn:: init_record");
     /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(asynCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->pdxp = pdxp;
    pdxp->dpvt = pPvt;

    /* Get the VME link field */
    /* Get the signal from the VME signal */
    pvmeio = (struct vmeio*)&(pdxp->inp.value);
    pPvt->channel = pvmeio->signal;
    /* Get the port name from the parm field */
    pPvt->portName = epicsStrDup(pvmeio->parm);

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, addr);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxpAsyn::init_record, connectDevice failed\n");
        goto bad;
    }

    /* Get the asynDxp interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynDxpType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxpAsyn::init_record, find dxp interface failed\n");
        goto bad;
    }
    pPvt->pasynDxpInterface = (asynDxp *)pasynInterface->pinterface;
    pPvt->asynDxpInterfacePvt = pasynInterface->drvPvt;

    pPvt->msgQueueId = epicsMessageQueueCreate(MSG_QUEUE_SIZE,
                                               sizeof(devDxpAsynMessage));
    if (pPvt->msgQueueId == 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, message queue create failed\n");
        goto bad;
    }

    dxp_max_symbols(&pPvt->channel, &pPvt->nsymbols);
    dxp_nbase(&pPvt->channel, &pPvt->nbase);
    pPvt->params = callocMustSucceed(pPvt->nsymbols, sizeof(unsigned short),
                                     "devMcaAsyn init_record()");
    pPvt->baseline = callocMustSucceed(pPvt->nbase, sizeof(unsigned short),
                                     "devMcaAsyn init_record()");
    return(0);
bad:
    pdxp->pact = 1;
    return(-1);
}


static long send_mca_msg(dxpRecord *pdxp, mcaCommand command)
{
    devDxpAsynMessage msg;

    msg.type = mcaMessage;
    msg.mcaCommand = command;
    return(send_msg(pdxp, msg));
}

static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         int value1, int value2)
{
    devDxpAsynMessage msg;

    msg.type = dxpMessage;
    msg.dxpCommand = command;
    msg.value1 = value1;
    msg.value2 = value2;
    return(send_msg(pdxp, msg));
}

static long send_msg(dxpRecord *pdxp, devDxpAsynMessage msg)
{
    devDxpAsynPvt *pPvt = (devDxpAsynPvt *)pdxp->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;
    asynUser *pasynUserCopy;
    int status;

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxpAsyn::send_msg: type=%d, command=%d, pact=%d\n",
              msg.type, 
              (msg.type == mcaMessage) ? msg.mcaCommand : msg.dxpCommand, 
              pdxp->pact);

    /* Make a copy of asynUser.  This is needed because we can have multiple
     * requests queued.  It will be freed in the callback */
    pasynUserCopy = pasynManager->duplicateAsynUser(pasynUser, asynCallback, 0);

    /* This is the only command that results in a callback to the record */
    if ((msg.type == mcaMessage) && (msg.mcaCommand == MSG_DXP_READ_PARAMS)) 
        pdxp->pact = 1;
    /* Put the message on the message queue */
    epicsMessageQueueSend(pPvt->msgQueueId, (void *)&msg, sizeof(msg));
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUserCopy, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxpAsyn::send_msg: error calling queueRequest, %s\n",
                  pasynUser->errorMessage);
        return(-1);
    }
    return(0);
}


void asynCallback(asynUser *pasynUser)
{
    devDxpAsynPvt *pPvt = (devDxpAsynPvt *)pasynUser->userPvt;
    dxpRecord *pdxp = pPvt->pdxp;
    devDxpAsynMessage msg;
    rset *prset = (rset *)pdxp->rset;
    int status;

    /* Read message from queue */
    status = epicsMessageQueueReceive(pPvt->msgQueueId, &msg, sizeof(msg));

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxpAsyn::asynCallback: type=%d, command=%d\n",
              msg.type, 
              (msg.type == mcaMessage) ? msg.mcaCommand:msg.dxpCommand);

    switch (msg.type) {
    case mcaMessage:
        pPvt->pasynMcaInterface->command(pPvt->asynMcaInterfacePvt,
                                         pPvt->pasynUser, pPvt->channel,
                                         msg.mcaCommand, 0, 0);
        break;
    case dxpMessage:
        switch(msg.dxpCommand) {
        case MSG_DXP_SET_SHORT_PARAM:
            pPvt->pasynDxpInterface->setShortParam(pPvt->asynMcaInterfacePvt,
                                                   pPvt->pasynUser, pPvt->channel,
                                                   msg.value1, msg.value2);
            break;
        case MSG_DXP_DOWNLOAD_FIPPI:
            pPvt->pasynDxpInterface->downloadFippi(pPvt->asynMcaInterfacePvt,
                                                   pPvt->pasynUser, pPvt->channel,
                                                   msg.value1);
            break;
        case MSG_DXP_CALIBRATE:
            pPvt->pasynDxpInterface->calibrate(pPvt->asynMcaInterfacePvt,
                                               pPvt->pasynUser, pPvt->channel,
                                               msg.value1);
            break;
        case MSG_DXP_READ_PARAMS:
            pPvt->pasynDxpInterface->readParams(pPvt->asynMcaInterfacePvt,
                                                pPvt->pasynUser, pPvt->channel,
                                                pPvt->params, pPvt->baseline);
            dbScanLock((dbCommon *)pdxp);
            (*prset->process)(pdxp);
            dbScanUnlock((dbCommon *)pdxp);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "devDxpAsyn::asynCallback, invalid command=%d\n",
                      msg.dxpCommand);
        }
    default:
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devDxpAsyn::asynCallback, invalid message type=%d\n",
                  msg.type);
    }
    status = pasynManager->freeAsynUser(pasynUser);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::asynCallback: error in freeAsynUser\n");
    }
}


static long read_array(dxpRecord *pdxp)
{
    devDxpAsynPvt *pPvt = (devDxpAsynPvt *)pdxp->dpvt;

    /* When this routine is called with pact=0 it sends a message */
    if (pdxp->pact == 0) {
       send_dxp_msg(pdxp, MSG_DXP_READ_PARAMS, 0, 0);
       return(0);
    }
    /* pact was non-zero, so this is a callback from record after I/O 
       completes */
    /* Copy data from private buffer to record */
    memcpy(pdxp->pptr, pPvt->params, pPvt->nsymbols*sizeof(unsigned short));
    memcpy(pdxp->bptr, pPvt->baseline, pPvt->nbase*sizeof(unsigned short));
    if (pdxp->udf==1) pdxp->udf=2;
    return(0);
}
