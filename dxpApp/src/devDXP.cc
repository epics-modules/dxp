// devDxpMpf.cc

/*
    Author: Mark Rivers
    17-Jun-2001

    This is device support for the DXP record with MPF servers.

    Modified:
    8-Feb-2002  MLR  Removed connectIO routine and modified sendFloat64Message
                     to call connectWait, which was recently added to DevMpf.
    4-Apr-2002  MLR  In completeIO if udf==1 (first successful reply) set udf=2 
                     so that we can take special action in dxpRecord.c.

*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dbAccess.h"
#include "dbDefs.h"
#include "link.h"
#include "errlog.h"
#include "dbCommon.h"
#include "dxpRecord.h"
#include "recSup.h"
#include "recGbl.h"
#include "alarm.h"
#include <epicsExport.h>

#include "Message.h"
#include "Float64Message.h"
#include "Int32ArrayMessage.h"
#include "Float64ArrayMessage.h"
#include "DevMpf.h"

#include "mca.h"
#include "dxpServer.h"
#include "xerxes_structures.h"
#include "xerxes.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=devDxpMpfDebug) errlogPrintf(f,## v); }
#endif
volatile int devDxpMpfDebug = 0;
}


class DevDxpMpf : public DevMpf
{
public:
        DevDxpMpf(dbCommon*,DBLINK*);
        long startIO(dbCommon* pr);
        long completeIO(dbCommon* pr,Message* m);
        virtual void receiveReply(dbCommon* pr, Message* m);
        static long init_record(void*);
        static long send_msg(dxpRecord *pdxp, unsigned long msg, 
                             int v1, int v2);
        static long read_array(dxpRecord *pdxp);
private:
        int sendFloat64Message(int cmd, int v1, int v2);
        int channel;
        unsigned short nsymbols;
        unsigned int nbase;
};

// This device support module cannot use the standard MPF macros for creating
// DSETS because we want to define additional functions which those macros do
// not allow.

typedef struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        long            (*send_msg)(dxpRecord *pdxp, unsigned long msg, 
                         int v1, int v2);
        long            (*read_array)(dxpRecord *pdxp);
} DXPMPF_DSET;

extern "C" {
    DXPMPF_DSET devDxpMpf = {
        6,
        NULL,
        NULL,
        DevDxpMpf::init_record,
        NULL,
        DevDxpMpf::send_msg,
        DevDxpMpf::read_array
    };
};
epicsExportAddress(DXPMPF_DSET, devDxpMpf);


DevDxpMpf::DevDxpMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    channel=io->signal;
    dxp_max_symbols(&channel, &nsymbols);
    dxp_nbase(&channel, &nbase);
    return;
}

long DevDxpMpf::startIO(dbCommon* pr)
{
    Float64Message *pm = new Float64Message;

    DEBUG(5,"DevDxpMpf::startIO, enter, record=%s, channel=%d\n", 
                  pr->name, channel);
    pm->address = channel;
    pm->cmd = MSG_DXP_READ_PARAMS;
    return(sendReply(pm));
}

long DevDxpMpf::read_array(dxpRecord *pdxp)
{
    // This static function is called by the DXP record to read the data.
    // It just calls DevMpf::read_write.
    
    DEBUG(5,"DevDxpMpf::read_array, enter, record=%s\n", pdxp->name);
    return(DevMpf::read_write(pdxp));
}

long DevDxpMpf::completeIO(dbCommon* pr,Message* m)
{
    // This function is called when a message comes back with data
    // Note that the message that comes back is nominally an Int32Array message
    // However, it really contains 2 unsigned short arrays: the parameter
    // array[nsymbols], followed by the baseline array[nbase].
    // This trick will work fine as long as the MPF client and server are on
    // the same CPU, or at least have the same endian-ness.
    dxpRecord* pdxp = (dxpRecord*)pr;
    Int32ArrayMessage *pam = (Int32ArrayMessage *)m;
    unsigned short *params, *baseline;

    switch (m->getType()) {
    case messageTypeInt32Array:
       DEBUG(5,"DevDxpMpf::completeIO, record=%s, status=%d\n", 
                  pdxp->name, pam->status);
       if(pam->status) {
           recGblSetSevr(pdxp,READ_ALARM,INVALID_ALARM);
           delete m;
           return(MPF_NoConvert);
       }
       params = (unsigned short *)pam->value;
       memcpy(pdxp->pptr, params, nsymbols*sizeof(unsigned short));
       baseline = (unsigned short *)pam->value + nsymbols;
       memcpy(pdxp->bptr, baseline, nbase*sizeof(unsigned short));
       if (pdxp->udf==1) pdxp->udf=2;
       break;

    default:
        errlogPrintf("%s ::completeIO illegal message type %d\n",
                    pdxp->name,m->getType());
        recGblSetSevr(pdxp,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_NoConvert);
    }
    delete m;
    return(MPF_OK);
}

long DevDxpMpf::init_record(void* v)
{
    dxpRecord* pdxp = (dxpRecord*)v;
    DevDxpMpf *pDevDxpMpf = new DevDxpMpf((dbCommon*)pdxp,&(pdxp->inp));
    pDevDxpMpf->bind();
    return(0);
}


long DevDxpMpf::send_msg(dxpRecord *pdxp, unsigned long msg, int v1, int v2)
{
    DevDxpMpf* devDxpMpf = (DevDxpMpf *)pdxp->dpvt;

    return(devDxpMpf->sendFloat64Message(msg, v1, v2));
}

int DevDxpMpf::sendFloat64Message(int cmd, int v1, int v2)
{
    Float64Message *pfm = new Float64Message;
    // We use a Float64Message even though we are sending ints to be compatible
    // with the mca record device support, which is talking to the same server
    
    // Wait for connect
    if (!connectWait(30.)) {
       DEBUG(1, "(sendFloat64Message): connectWait failed!\n");
       return(-1);
    }
    pfm->cmd = cmd;
    pfm->value = v1;
    pfm->extra = v2;
    pfm->address = channel;
    return(send(pfm, replyTypeReceiveReply));
}

void DevDxpMpf::receiveReply(dbCommon* pr, Message* m)
{
    // This routine is called when replies are received from the server for 
    // messages sent by sendFloat64Message().  It simply deletes the reply, since
    // the record is not waiting for the response.
    dxpRecord* pdxp = (dxpRecord*)pr;
    DEBUG(5, "%s DevDxpMpf:receiveReply enter\n", pdxp->name);
    delete m;
}
