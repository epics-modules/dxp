/* asynDxp.h -- communication between device and driver using asyn */

#ifndef asynDxpH
#define asynDxpH

#include <asynDriver.h>

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

#endif /* asynDxpH */
