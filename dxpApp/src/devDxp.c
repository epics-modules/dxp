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
#include <recSup.h>
#include <recGbl.h>
#include <alarm.h>
#include <cantProceed.h>
#include <epicsExport.h>
#include <epicsString.h>

#include <asynDriver.h>

#include "dxpRecord.h"
#include "devDxp.h"
#include "asynDxp.h"
#include "asynMca.h"
#include "xerxes_structures.h"
#include "xerxes.h"

typedef struct {
    dxpRecord *pdxp;
    asynUser *pasynUser;
    char *portName;
    int channel;
    asynMca *pasynMca;
    void *asynMcaPvt;
    asynDxp *pasynDxp;
    void *asynDxpPvt;
    unsigned short nsymbols;
    unsigned int nbase;
    unsigned short *params;
    unsigned short *baseline;
} devDxpPvt;

typedef enum {
   mcaMessage,
   dxpMessage
} dxpMessage_t;

typedef struct {
    dxpMessage_t type;
    mcaCommand mcaCommand;
    devDxpCommand dxpCommand;
    int value1;
    int value2;
} devDxpMessage;

static long init_record(dxpRecord *pdxp);
static long send_mca_msg(dxpRecord *pdxp, mcaCommand command); 
static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         int v1, int v2);
static long send_msg(dxpRecord *pdxp, devDxpMessage *msg); 
static long read_array(dxpRecord *pdxp);
static void asynCallback(asynUser *pasynUser);

static const struct devDxpDset devDxp = {
    7,
    NULL,
    NULL,
    init_record,
    NULL,
    send_mca_msg,
    send_dxp_msg,
    read_array
};
epicsExportAddress(dset, devDxp);


static long init_record(dxpRecord *pdxp)
{
    struct vmeio *pvmeio;
    asynUser *pasynUser;
    asynStatus status;
    asynInterface *pasynInterface;
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

    /* Get the VME link field */
    /* Get the signal from the VME signal */
    pvmeio = (struct vmeio*)&(pdxp->inp.value);
    pPvt->channel = pvmeio->signal;
    /* Get the port name from the parm field */
    pPvt->portName = epicsStrDup(pvmeio->parm);

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, pPvt->portName, 
                                         pPvt->channel);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::init_record, connectDevice failed\n");
        goto bad;
    }

    /* Get the asynDxp interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynDxpType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::init_record, find dxp interface failed\n");
        goto bad;
    }
    pPvt->pasynDxp = (asynDxp *)pasynInterface->pinterface;
    pPvt->asynDxpPvt = pasynInterface->drvPvt;

    /* Get the asynMca interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynMcaType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::init_record, find mca interface failed\n");
        goto bad;
    }
    pPvt->pasynMca = (asynMca *)pasynInterface->pinterface;
    pPvt->asynMcaPvt = pasynInterface->drvPvt;

    dxp_max_symbols(&pPvt->channel, &pPvt->nsymbols);
    dxp_nbase(&pPvt->channel, &pPvt->nbase);
    pPvt->params = callocMustSucceed(pPvt->nsymbols, sizeof(unsigned short),
                                     "devMcaAsyn init_record()");
    pPvt->baseline = callocMustSucceed(pPvt->nbase, sizeof(unsigned short),
                                     "devMcaAsyn init_record()");
    return(0);
bad:
    pdxp->pact = 1;
    return(0);
}


static long send_mca_msg(dxpRecord *pdxp, mcaCommand command)
{
    devDxpMessage *pmsg = pasynManager->memMalloc(sizeof(*pmsg));

    pmsg->type = mcaMessage;
    pmsg->mcaCommand = command;
    return(send_msg(pdxp, pmsg));
}

static long send_dxp_msg(dxpRecord *pdxp, devDxpCommand command, 
                         int value1, int value2)
{
    devDxpMessage *pmsg = pasynManager->memMalloc(sizeof(*pmsg));

    pmsg->type = dxpMessage;
    pmsg->dxpCommand = command;
    pmsg->value1 = value1;
    pmsg->value2 = value2;
    return(send_msg(pdxp, pmsg));
}

static long send_msg(dxpRecord *pdxp, devDxpMessage *pmsg)
{
    devDxpPvt *pPvt = (devDxpPvt *)pdxp->dpvt;
    asynUser *pasynUser;
    int status;

    /* Note that the only message that requires a callback to the record is
     * send by read_array, and it has already set pact=1 */

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
              "devDxp::send_msg: type=%d, command=%d, pact=%d\n",
              pmsg->type, 
              (pmsg->type == mcaMessage) ? pmsg->mcaCommand : pmsg->dxpCommand, 
              pdxp->pact);

    pasynUser = pasynManager->duplicateAsynUser(pPvt->pasynUser,
                                                asynCallback, 0);
    pasynUser->userData = pmsg;
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUser, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devDxp::send_msg: error calling queueRequest, %s\n",
                  pasynUser->errorMessage);
        return(-1);
    }
    return(0);
}


void asynCallback(asynUser *pasynUser)
{
    devDxpPvt *pPvt = (devDxpPvt *)pasynUser->userPvt;
    devDxpMessage *pmsg = pasynUser->userData;
    dxpRecord *pdxp = pPvt->pdxp;
    rset *prset = (rset *)pdxp->rset;
    int status;

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "devDxp::asynCallback: type=%d, command=%d\n",
              pmsg->type, 
              (pmsg->type == mcaMessage) ? pmsg->mcaCommand:pmsg->dxpCommand);

    switch (pmsg->type) {
    case mcaMessage:
        pPvt->pasynMca->command(pPvt->asynMcaPvt,
                                pPvt->pasynUser,
                                pmsg->mcaCommand, 0, 0);
        break;
    case dxpMessage:
        switch(pmsg->dxpCommand) {
        case MSG_DXP_SET_SHORT_PARAM:
            pPvt->pasynDxp->setShortParam(pPvt->asynMcaPvt,
                                          pPvt->pasynUser,
                                          pmsg->value1, pmsg->value2);
            break;
        case MSG_DXP_DOWNLOAD_FIPPI:
            pPvt->pasynDxp->downloadFippi(pPvt->asynMcaPvt,
                                          pPvt->pasynUser,
                                          pmsg->value1);
            break;
        case MSG_DXP_CALIBRATE:
            pPvt->pasynDxp->calibrate(pPvt->asynMcaPvt,
                                      pPvt->pasynUser,
                                      pmsg->value1);
            break;
        case MSG_DXP_READ_PARAMS:
            pPvt->pasynDxp->readParams(pPvt->asynMcaPvt,
                                       pPvt->pasynUser,
                                       pPvt->params, pPvt->baseline);
            dbScanLock((dbCommon *)pdxp);
            (*prset->process)(pdxp);
            dbScanUnlock((dbCommon *)pdxp);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "devDxp::asynCallback, invalid command=%d\n",
                      pmsg->dxpCommand);
        }
    default:
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devDxp::asynCallback, invalid message type=%d\n",
                  pmsg->type);
    }
    pasynManager->memFree(pmsg, sizeof(*pmsg));
    status = pasynManager->freeAsynUser(pasynUser);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::asynCallback: error in freeAsynUser\n");
    }
}


static long read_array(dxpRecord *pdxp)
{
    devDxpPvt *pPvt = (devDxpPvt *)pdxp->dpvt;

    /* When this routine is called with pact=0 it sends a message */
    if (pdxp->pact == 0) {
       pdxp->pact = 1;
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
