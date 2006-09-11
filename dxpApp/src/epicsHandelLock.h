/* File to interlock access to Handel library in a multi-threaded environment */
#include <epicsMutex.h>

/* Global mutex */
extern epicsMutexId epicsHandelMutexId;

/* Function prototypes */
int epicsHandelLock();
int epicsHandelUnlock();

