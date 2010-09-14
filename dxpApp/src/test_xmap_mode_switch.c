/* This program tests switching between the mapping modes on the xMAP. 
 * We are getting errors when downloading the firmware.
 */

#include <stdlib.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"

#define MAX_ITERATIONS 100
#define CHECK_STATUS(status) if (status) {printf("Error %d line %d\n", status, __LINE__); exit(status);}

int main(int argc, char **argv)
{
    int iter, mode, status, debugLevel=2;
    double dvalue;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(debugLevel);
    CHECK_STATUS(status);
    status = xiaInit("xmap_test.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);
    
    if (argc > 0) {
        debugLevel = atoi(argv[1]);
        printf("Debug level = %d\n", debugLevel);
    }
    status = xiaSetLogLevel(debugLevel);
    CHECK_STATUS(status);

    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        for (mode=0; mode<3; mode++) {
            printf("Iter=%d, setting mapping mode %d\n", iter, mode);
            dvalue = mode;
            status = xiaSetAcquisitionValues(-1, "mapping_mode", &dvalue);
            CHECK_STATUS(status);
        }
    }
    return(0);
}
