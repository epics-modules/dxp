/* devDXPAsyn.h -- communication between dxp record and device support */

#include <mca.h>
#include <dxpRecord.h>
#include <devSup.h>

typedef enum {
    MSG_DXP_SET_SHORT_PARAM,
    MSG_DXP_SET_LONG_PARAM,
    MSG_DXP_CONTROL_TASK,
    MSG_DXP_READ_PARAMS,
    MSG_DXP_DOWNLOAD_FIPPI
} devDxpCommand;

typedef struct devDxpDset {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        long            (*send_mca_msg)(dxpRecord *pdxp, mcaCommand);
        long            (*send_dxp_msg)(dxpRecord *pdxp, devDxpCommand,
                                        int v1, int v2);
        long            (*read_array)(dxpRecord *pdxp);
} devDXP_dset;
