/* drvDXPAsyn.h -- communication between device and driver using asyn */

#include <asynDriver.h>

/* The number of module types.  Currently 3: DXP4C and DXP2X, DXPX10P */
#define MAX_MODULE_TYPES 3
#define MODEL_DXP4C   0
#define MODEL_DXP2X   1
#define MODEL_DXPX10P 2

#define asynDxpType "asynDxp"
typedef struct {
    asynStatus (*setShortParam)(void *drvPvt, asynUser *pasynUser,
                                unsigned short offset,
                                unsigned short value);
    asynStatus (*calibrate)(void *drvPvt, asynUser *pasynUser,
                            int ivalue);
    asynStatus (*readParams)(void *drvPvt, asynUser *pasynUser,
                             short *params,
                             short *baseline);
    asynStatus (*downloadFippi)(void *drvPvt, asynUser *pasynUser,
                                int fippiIndex);
} asynDxp;
