#include "handel.h"

#define MAX_ITERATIONS 1000
#define NUM_CHANNELS 16
#define TRACE_LEN 4096
#define TRACE_TIME 100
#define CHECK_STATUS(status) if (status) {printf("Error %d\n", status);}

int main(int argc, char **argv)
{
    int iter, chan;
    int data[TRACE_LEN];
    double info[2];
    int status;

    printf("Initializing ...\n");
    status = xiaSetLogLevel(2);
    CHECK_STATUS(status);
    status = xiaInit("xmap16.ini");
    CHECK_STATUS(status);
    status = xiaStartSystem();
    CHECK_STATUS(status);

    for (iter=0; iter<MAX_ITERATIONS; iter++) {
        printf("Iteration=%d\n", iter);
        for (chan=0; chan<NUM_CHANNELS; chan++) {
            info[0] = 0.;
            /* Trace time is in ns */
            info[1] = TRACE_TIME;

            status = xiaDoSpecialRun(chan, "adc_trace", info);
            CHECK_STATUS(status);

            status = xiaGetSpecialRunData(chan, "adc_trace", data);
            CHECK_STATUS(status);
        }
    }
    return(0);
}
