#include <stdlib.h>
#include <sys/time.h>
#include "handel.h"
#include "handel_generic.h"
#include "handel_constants.h"

#define MAX_ITERATIONS 10
#define MAX_CHECKS 1000
#define NUM_CHANNELS 16
#define COUNT_TIME 0.1
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status); exit(status);}

int main(int argc, char **argv)
{
    int iter, check, chan;
    int acquiring[NUM_CHANNELS];
    int done;
    int status;
    int ignore;
    double presetValue;
    double presetType;
    struct timeval starttime, polltime;
    struct timezone timezone;
    double timediff;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    presetType = XIA_PRESET_FIXED_REAL;
    presetValue = COUNT_TIME;
    status = xiaSetAcquisitionValues(-1, "preset_type", &presetType);
    CHECK_STATUS(status);
    status = xiaSetAcquisitionValues(-1, "preset_values", &presetValue);
    CHECK_STATUS(status);
    for (chan=0; chan<NUM_CHANNELS; chan+=4) { 
        status = xiaBoardOperation(chan, "apply", &ignore);
        CHECK_STATUS(status);
    }


    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        status = xiaStartRun(-1, 0);
        printf("Start acquisition %d\n", iter+1);
        gettimeofday(&starttime, &timezone);
        CHECK_STATUS(status);
 
        for (check=0; check<MAX_CHECKS; check++) {
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                status = xiaGetRunData(chan, "run_active", &acquiring[chan]);
                CHECK_STATUS(status);
                gettimeofday(&polltime, &timezone);
                if (acquiring[chan] == XIA_RUN_HANDEL) {
                    status = xiaStopRun(chan);
                    CHECK_STATUS(status);
                    printf("Stopped run on channel %d\n", chan);
                }
            }
            done = 1;
            timediff = (polltime.tv_sec + polltime.tv_usec/1.e6) -
                       (starttime.tv_sec + starttime.tv_usec/1.e6);
            printf("time=%f, acquiring=", timediff);
            for (chan=0; chan<NUM_CHANNELS; chan++) {
                printf(" %d", acquiring[chan]);
                if (acquiring[chan] & XIA_RUN_HARDWARE) done=0;
            }
            printf(" done=%d\n", done);
            if (done) break;
        }
    }
    return(0);
}
