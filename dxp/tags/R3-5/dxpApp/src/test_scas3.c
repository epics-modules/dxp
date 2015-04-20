#include "stdlib.h"
#include "epicsTime.h"
#include "handel.h"
#include "handel_generic.h"

#define NUM_SCAS 64
#define NUM_CHANNELS 4
#define NUM_MCA_BINS 2048
#define NUM_LOOPS 2
static char *sca_lo[NUM_SCAS];
static char *sca_hi[NUM_SCAS];
#define SCA_NAME_LEN 10
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int setSCAs(int channel)
{
    int j;
    int status;
    double num_scas = NUM_SCAS;
    double dvalue;
    xiaSetAcquisitionValues(channel, "number_of_scas", &num_scas);
    for (j=0; j<NUM_SCAS; j++) {
         dvalue = 24. * j;
         status = xiaSetAcquisitionValues(channel, sca_lo[j], &dvalue);
         CHECK_STATUS(status);
         dvalue = (24 * j) + 10;
         status = xiaSetAcquisitionValues(channel, sca_hi[j], &dvalue);
         CHECK_STATUS(status);
    }
    return 0;
}
int main(int argc, char **argv)
{
    int i, j, loop;
    int ignore=0;
    epicsTimeStamp tStart, tEnd;
    int status;

    printf("Initializing ...\n");
    xiaSetLogLevel(2);
    xiaInit("test4.ini");
    xiaStartSystem();
    for (i=0; i<NUM_SCAS; i++) {
        sca_lo[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_lo[i], "sca%d_lo", i);
        sca_hi[i] = calloc(1, SCA_NAME_LEN);
        sprintf(sca_hi[i], "sca%d_hi", i);
    }
    for (loop=0; loop<NUM_LOOPS; loop++) {
        printf("Loop = %d/%d\n", loop+1, NUM_LOOPS);
        epicsTimeGetCurrent(&tStart);
        for (i=0; i<NUM_CHANNELS; i++) {
            printf("  channel=%d\n", i);
            for (j=0; j<NUM_SCAS; j++) {
                setSCAs(i);
            }
        }
        status = xiaBoardOperation(0, "apply", &ignore);
        CHECK_STATUS(status);
        epicsTimeGetCurrent(&tEnd);
        printf("Time = %f\n", epicsTimeDiffInSeconds(&tEnd, &tStart));
    } 
    return(0);
}
