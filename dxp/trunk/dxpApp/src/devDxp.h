/* devDXPAsyn.h -- communication between dxp record and device support */

#include <mca.h>
#include <dxpRecord.h>
#include <devSup.h>

typedef enum {
    MSG_DXP_SET_SHORT_PARAM,
    MSG_DXP_SET_DOUBLE_PARAM,
    MSG_DXP_CONTROL_TASK,
    MSG_DXP_READ_PARAMS
} devDxpCommand;

typedef struct dxpReadbacks {
    long fast_peaks;
    long slow_peaks;
    double icr;
    double ocr;
    double slow_trig;
    double pktim;
    double gaptim;
    double adc_rule;
    double mca_bin_width;
    double number_mca_channels;
    double emax;
    double fast_trig;
    double trig_pktim;
    double trig_gaptim;
    int newBaselineHistory;
    int newBaselineHistogram;
    int newAdcTrace;
} dxpReadbacks;

typedef struct devDxpDset {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        long            (*send_dxp_msg)(dxpRecord *pdxp, devDxpCommand,
                                        char *name, int param, double dvalue,
                                        void *pointer);
} devDXP_dset;
