#include <epicsMutex.h>
#include "epicsHandelLock.h"

epicsMutexId epicsHandelMutexId = 0;

int epicsHandelLock()
{
    if (epicsHandelMutexId == 0) {
        epicsHandelMutexId = epicsMutexMustCreate();
    }
    epicsMutexMustLock(epicsHandelMutexId);
    return(0);
}

int epicsHandelUnlock()
{
    epicsMutexUnlock(epicsHandelMutexId);
    return(0);
}
