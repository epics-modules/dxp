/* Standard includes... */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* EPICS includes */
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsExit.h>
#include <envDefs.h>

/* Handel includes */
#include <handel.h>
#include <handel_errors.h>
#include <handel_generic.h>
#include <md_generic.h>
#include <handel_constants.h>

/* MCA includes */
#include <drvMca.h>
#include <asynStandardInterfaces.h>

/* Area Detector includes */
#include <asynNDArrayDriver.h>

#include "NDDxp.h"

#define DXP_ALL                  -1
#define XMAP_NCHANS_MODULE         4
#define XMAP_MAX_MCA_BINS      16384
#define XMAP_MCA_BIN_RES         256
#define XMAP_MAX_MAPBUF_SIZE 2097152   /** < The maximum number of bytes in the 2MB mapping mode buffer */
#define MEGABYTE             1048576

#define CALLHANDEL( handel_call, msg ) { \
    xiastatus = handel_call; \
    status = this->xia_checkError( pasynUser, xiastatus, msg ); \
}

/** Only used for debugging/error messages to identify where the message comes from*/
static const char *driverName = "NDDxp";

typedef enum {NDDxpModelXMAP = 0, NDDxpModelSaturn, NDDxpModel4C2X} NDDxpModel_t;
typedef enum {NDDxpModeNormal = 0, NDDxpModeMapping } NDDxpMode_t;
typedef enum {NDDxpPresetCountModeEvents, NDDxpPresetCountModeTriggers} NDDxpPresetCountMode_t;
typedef enum {NDDxpTraceADC, NDDxpTraceBaselineHistory,
              NDDxpTraceTriggerFilter, NDDxpTraceBaselineFilter, NDDxpTraceEnergyFilter,
              NDDxpTraceBaselineSamples, NDDxpTraceEnergySamples} NDDxpTraceMode_t;
static char *NDDxpTraceCommands[] = {"adc_trace", "baseline_history",
                                     "trigger_filter", "baseline_filter", "energy_filter",
                                     "baseline_samples", "energy_samples"};


// This structure holds state information for each channel
// This may or may not be needed.  It was used in old MCA driver
typedef struct {
    double preal;
    double ereal;
    double plive;
    double elive;
    double ptotal;
    double etotal;
    int acquiring;
    int prev_acquiring;
    int erased;
} dxpChannel_t;

typedef enum NDDxpParam_t {
    NDDxpXMAPMode = lastMcaCommand,     /** < Change mode of the XMAP (0=mca; 1=mapping; 2=sca) (int32 read/write) addr: all/any */
    NDDxpStartMode,                     /** < Start mode. (0=clear on start; 1=resume spectrum) */
    NDDxpXMAPRunState,                  /** < XMAP reporting it's runtime state (int bitmap) */
    NDDxpCurrentPixel,                  /** < XMAP mapping mode only: read the current pixel that is being acquired into (int) */
    NDDxpNextPixel,                     /** < XMAP mapping mode only: force a pixel increment in the xmap buffer (write only int). Value is ignored. */
    NDDxpPixelsPerBuffer,
    NDDxpBufferOverrun,
    NDDxpMBytesReceived,
    NDDxpReadSpeed,

    NDDxpReadDXPParams,        /** < Read back values of DXP parameters */
    NDDxpPresetNumPixels,      /** < Preset value how many pixels to acquire in one run (r/w) mapping mode*/
    NDDxpPixelCounter,         /** < Count how many pixels have been acquired (read) mapping mode */
    NDDxpBufferCounter,        /** < Count how many buffers have been collected (read) mapping mode */
    NDDxpPollTime,             /** < Status/data polling time in seconds */
    NDDxpArrayCallbacks,       /** < Enable/disable array callbacks */
    NDDxpDataType,             /** < DataType in NDDataType terms */
    NDDxpArrayData,            /** < Array data: one spectrum in normal/mca mode; One full buffer (incl) headers in mapping mode */
    NDDxpArraySize,            /** < Number of words/items in the array. Multiply with DataType to get bytesize. */
    NDDxpNumPixelsInBuffer,    /** < Number of pixels in on buffer. Intended as read-only but can potentially be write-able (why would you?) */
    NDDxpPresetCountMode,      /** < Sets which type of preset count to send to the HW: either events or triggers (xmap) */
    NDDxpTraceMode,            /** < Select what type of trace to do: ADC, baseline hist, .. etc. */
    NDDxpTraceTime,            /** < Set the trace sample time in us. */
    NDDxpTrace,                /** < The trace array data (read) */
    NDDxpBaselineHistogram,    /** < The baseline histogram array data (read) */
    NDDxpMaxEnergy,            /** < Maximum energy */
    NDDxpPollActive,           /** < Polling the HW for status and data is active/inactive (read)*/
    NDDxpForceRead,            /** < Force reading MCA spectra - used for mcaData when addr=ALL */

    /* runtime statistics */
    NDDxpElapsedRealTime,                  /** < real time in seconds (double) */
    NDDxpElapsedTriggerLiveTime,           /** < live time in seconds (double) */
    NDDxpElapsedLiveTime,            /** < live time in seconds (double) */
    NDDxpTriggers,                  /** < number of triggers received (double) */
    NDDxpEvents,                    /** < total number of events registered (double) */
    NDDxpInputCountRate,            /** < input count rate in Hz (double) */
    NDDxpOutputCountRate,           /** < output count rate in Hz (double) */

    NDDxpPeakingTime,
    NDDxpDynamicRange,
    NDDxpTriggerThreshold,
    NDDxpBaselineThreshold,
    NDDxpEnergyThreshold,
    NDDxpCalibrationEnergy,
    NDDxpADCPercentRule,
    NDDxpMCABinWidth,
    NDDxpPreampGain,
    NDDxpNumMCAChannels,
    NDDxpDetectorPolarity,
    NDDxpResetDelay,
    NDDxpDecayTime,
    NDDxpGapTime,
    NDDxpTriggerPeakingTime,
    NDDxpTriggerGapTime,
    NDDxpBaselineAverage,
    NDDxpBaselineCut,
    NDDxpEnableBaselineCut,
    NDDxpMaxWidth,

    NDDxpLastDriverParam
} NDDxpParam_t;

static asynParamString_t NDDxpParamString[] = {
    {NDDxpXMAPMode,                 "DxpXMAPMode"},
    {NDDxpStartMode,                "DxpStartMode"},
    {NDDxpXMAPRunState,             "DxpXMAPRunState"},
    {NDDxpCurrentPixel,             "DxpCurrentPixel"},
    {NDDxpNextPixel,                "DxpNextPixel"},
    {NDDxpBufferOverrun,            "DxpBufferOverrun"},
    {NDDxpMBytesReceived,           "DxpMBytesReceived"},
    {NDDxpReadSpeed,                "DxpReadSpeed"},

    {NDDxpReadDXPParams,            "DxpReadDXPParams"},
    {NDDxpPresetNumPixels,          "DxpPresetNumPixels"},
    {NDDxpPixelCounter,             "DxpPixelCounter"},
    {NDDxpBufferCounter,            "DxpBufferCounter"},
    {NDDxpPollTime,                 "DxpPollTime"},
    {NDDxpArrayCallbacks,           "DxpArrayCallbacks"},
    {NDDxpDataType,                 "DxpDataType"},
    {NDDxpArrayData,                "DxpArrayData"},
    {NDDxpArraySize,                "DxpArraySize"},
    {NDDxpNumPixelsInBuffer,        "DxpNumPixelsInBuffer"},
    {NDDxpPresetCountMode,          "DxpPresetCountMode"},
    {NDDxpTraceMode,                "DxpTraceMode"},
    {NDDxpTraceTime,                "DxpTraceTime"},
    {NDDxpTrace,                    "DxpTrace"},
    {NDDxpBaselineHistogram,        "DxpBaselineHistogram"},
    {NDDxpMaxEnergy,                "DxpMaxEnergy"},
    {NDDxpPollActive,               "DxpPollActive"},
    {NDDxpForceRead,                "DxpForceRead"},

    {NDDxpElapsedRealTime,          "DxpElapsedRealTime"},
    {NDDxpElapsedTriggerLiveTime,   "DxpElapsedTriggerLiveTime"},
    {NDDxpElapsedLiveTime,          "DxpElapsedLiveTime"},
    {NDDxpTriggers,                 "DxpTriggers"},
    {NDDxpEvents,                   "DxpEvents"},
    {NDDxpInputCountRate,           "DxpInputCountRate"},
    {NDDxpOutputCountRate,          "DxpOutputCountRate"},

    {NDDxpPeakingTime,              "DxpPeakingTime"},
    {NDDxpDynamicRange,             "DxpDynamicRange"},
    {NDDxpTriggerThreshold,         "DxpTriggerThreshold"},
    {NDDxpBaselineThreshold,        "DxpBaselineThreshold"},
    {NDDxpEnergyThreshold,          "DxpEnergyThreshold"},
    {NDDxpCalibrationEnergy,        "DxpCalibrationEnergy"},
    {NDDxpADCPercentRule,           "DxpADCPercentRule"},
    {NDDxpMCABinWidth,              "DxpMCABinWidth"},
    {NDDxpPreampGain,               "DxpPreampGain"},
    {NDDxpNumMCAChannels,           "DxpNumMCAChannels"},
    {NDDxpDetectorPolarity,         "DxpDetectorPolarity"},
    {NDDxpResetDelay,               "DxpResetDelay"},
    {NDDxpDecayTime,                "DxpDecayTime"},
    {NDDxpGapTime,                  "DxpGapTime"},
    {NDDxpTriggerPeakingTime,       "DxpTriggerPeakingTime"},
    {NDDxpTriggerGapTime,           "DxpTriggerGapTime"},
    {NDDxpBaselineAverage,          "DxpBaselineAverage"},
    {NDDxpBaselineCut,              "DxpBaselineCut"},
    {NDDxpEnableBaselineCut,        "DxpEnableBaselineCut"},
    {NDDxpMaxWidth,                 "DxpMaxWidth"}
};

typedef struct NDDxpBuffers_t {
    char bufChar[2];
    char bufFull[14];
    char buffer[9];
} NDDxpBuffers_t;

static NDDxpBuffers_t NDDxpBuffers[] =
{
        {"a", "buffer_full_a", "buffer_a"},
        {"b", "buffer_full_b", "buffer_b"},
};

/** Number of asyn parameters (asyn commands) this driver supports. */
#define NDDxp_N_PARAMS (sizeof( NDDxpParamString)/ sizeof(NDDxpParamString[0]))


class NDDxp : public asynNDArrayDriver
{
public:
    NDDxp(const char *portName, int nCChannels, int maxBuffers, size_t maxMemory);

    /* virtual methods to override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    virtual asynStatus drvUserCreate( asynUser *pasynUser, const char *drvInfo, const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);

    /* Local methods to this class */
    asynStatus xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, char *xiacmd );
    void shutdown();

    void acquisitionTask();
    asynStatus pollMappingMode();
    int getChannel(asynUser *pasynUser, int *addr);
    int getModuleType();
    asynStatus apply(int channel);
    asynStatus setPresets(asynUser *pasynUser, int addr);
    asynStatus setDxpParam(asynUser *pasynUser, int addr, int function, double value);
    asynStatus getDxpParams(asynUser *pasynUser, int addr);
    asynStatus getAcquisitionStatus(asynUser *pasynUser, int addr);
    asynStatus getAcquisitionStatistics(asynUser *pasynUser, int addr);
    asynStatus getMcaData(asynUser *pasynUser, int addr);
    asynStatus getMappingData(asynUser *pasynUser, int addr);
    asynStatus getTrace(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus getBaselineHistogram(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus changeMode(asynUser *pasynUser, epicsInt32 mode);
    asynStatus setNumChannels(asynUser *pasynUser, epicsInt32 newsize, epicsInt32 *rbValue);

    asynStatus startAcquiring(asynUser *pasynUser);
    asynStatus stopAcquiring(asynUser *pasynUser);

    asynStatus xmapGetModuleChannels(int currentChannel, int* firstChannel, int* nChannels);

private:
    /* Data */
    NDArray *pRaw;
    epicsUInt32 **pMcaRaw;
    epicsUInt32 **pXmapMcaRaw;
    epicsUInt32 *pMapRaw;
    epicsUInt16 *pMapSmallWord;
    epicsFloat64 *tmpStats;

    NDDxpModel_t deviceType;
    int nCards;
    int nChannels;

    epicsEvent *cmdStartEvent;
    epicsEvent *cmdStopEvent;
    epicsEvent *stoppedEvent;

    epicsUInt32 *currentBuf;
    int traceLength;
    int baselineLength;
    epicsInt32 *traceBuffer;
    epicsInt32 *baselineBuffer;

    dxpChannel_t *pChannel;
    char polling;

};

static void c_shutdown(void* arg)
{
    NDDxp *pNDDxp = (NDDxp*)arg;
    pNDDxp->shutdown();
    free(pNDDxp);
}

static void acquisitionTaskC(void *drvPvt)
{
    NDDxp *pNDDxp = (NDDxp *)drvPvt;
    pNDDxp->acquisitionTask();
}

extern "C" int NDDxp_config(const char *portName, int nChannels,
                            int maxBuffers, size_t maxMemory)
{
    new NDDxp(portName, nChannels, maxBuffers, maxMemory);
    return 0;
}

NDDxp::NDDxp(const char *portName, int nChannels, int maxBuffers, size_t maxMemory)
    : asynNDArrayDriver(portName, nChannels + 1, NDDxpLastDriverParam, maxBuffers, maxMemory,
            asynFloat64Mask | asynInt32ArrayMask | asynGenericPointerMask | asynOctetMask | asynInt32Mask | asynDrvUserMask,
            asynFloat64Mask | asynInt32ArrayMask | asynGenericPointerMask | asynOctetMask | asynInt32Mask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0),
    pRaw(NULL)
{
    int status = asynSuccess;
    int i, ch;
    int xiastatus = 0;
    asynUser *pasynUser = this->pasynUserSelf;
    const char *functionName = "NDDxp";

    this->deviceType = (NDDxpModel_t) this->getModuleType();
    this->nChannels = nChannels;
    switch (this->deviceType)
    {
    case NDDxpModelXMAP:
        /* TODO: this solution is a bit crude and not always correct... */
        this->nCards = this->nChannels / XMAP_NCHANS_MODULE;
        break;
    default:
        this->nCards = this->nChannels;
        break;
    }

    this->pChannel = (dxpChannel_t *) calloc(sizeof(dxpChannel_t), this->nChannels);

    /* Register the epics exit function to be called when the IOC exits... */
    xiastatus = epicsAtExit(c_shutdown, (void*)this);
    printf("  epicsAtExit registered: %d\n", xiastatus);

    /* Set the parameters from the camera in our areaDetector param lib */
    printf("  Setting the detector parameters...        ");
    status |= setIntegerParam(NDDxpStartMode, 0);
    status |= setIntegerParam(NDDxpXMAPMode, 0);
    printf("OK\n");

    /* Create the start and stop events that will be used to signal our
     * acquisitionTask when to start/stop polling the HW     */
    printf("  C++ epicsEvent....                         ");
    this->cmdStartEvent = new epicsEvent();
    this->cmdStopEvent = new epicsEvent();
    this->stoppedEvent = new epicsEvent();
    printf("OK\n");

    printf("  Allocating MCA spectrum memory...          ");
    /* allocate a memory pointer for each of the channels */
    this->pMcaRaw = (epicsUInt32**) calloc(this->nChannels, sizeof(epicsUInt32*));
    /* allocate a memory area for each spectrum */
    for (ch = 0; ch < this->nChannels; ch++)
        this->pMcaRaw[ch] = (epicsUInt32*)calloc(XMAP_MAX_MCA_BINS, sizeof(epicsUInt32));
    if (this->deviceType == NDDxpModelXMAP)
    {
        int numxMAPs = (this->nChannels-1)/ XMAP_NCHANS_MODULE + 1;
        int spectSize = XMAP_MAX_MCA_BINS * sizeof(epicsUInt32);
        int xmapSize = spectSize * XMAP_NCHANS_MODULE;
        char *buff;
        this->pXmapMcaRaw = (epicsUInt32**)calloc(numxMAPs, sizeof(epicsUInt32*));
        /* Allocate one big block of memory that is large enough for all channels */
        buff = (char *)malloc(numxMAPs * xmapSize);
        for( i = 0; i < numxMAPs; i++)
            this->pXmapMcaRaw[i] = (epicsUInt32 *)(buff + i*xmapSize);
        for( i = 0; i < this->nChannels; i++)
            this->pMcaRaw[i] = (epicsUInt32 *)(buff + i*spectSize);
    }

    this->tmpStats = (epicsFloat64*)calloc(28, sizeof(epicsFloat64));
    this->currentBuf = (epicsUInt32*)calloc(this->nChannels, sizeof(epicsUInt32));
    printf("OK\n");

    printf("  Getting adc_trace_length...                 ");
    xiastatus = xiaGetSpecialRunData(0, "adc_trace_length", (void *) &(this->traceLength));
    if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetSpecialRunData for adc_trace_length");
    printf("%d\n", this->traceLength);

    /* Allocate a buffer for the trace data */
    this->traceBuffer = (epicsInt32 *)malloc(this->traceLength * sizeof(epicsInt32));

    printf("Getting baseline_length...                     ");
    xiastatus = xiaGetRunData(0, "baseline_length", (void *) &(this->baselineLength));
    if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetRunData for baseline_length");
    printf("%d\n", this->baselineLength);

    /* Allocate a buffer for the baseline histogram data */
    this->baselineBuffer = (epicsInt32 *)malloc(this->baselineLength * sizeof(epicsInt32));

    /* Allocating a temporary buffer for use when collecting mapped MCA spectrums.
     * The XMAP buffer takes up 2MB of 16bit words. Unfortunatly the transfer over PCI
     * uses 32bit words so the data we receive from from the Handel library is 2x2MB large.
     * Thus the 2MB buffer: XMAP_MAX_MAPBUF_SIZE = 2 * 1024 * 1024; */
    printf("  Allocating mapping memory buffer...          ");
    this->pMapRaw = (epicsUInt32*)malloc(2*XMAP_MAX_MAPBUF_SIZE);
    this->pMapSmallWord = (epicsUInt16*)calloc(XMAP_MAX_MAPBUF_SIZE, sizeof(epicsUInt16));
    printf("%p\n", this->pMapRaw);

    /* Start up acquisition thread */
    printf("  Starting up acquisition task...          ");
    this->polling = 1;
    status = (epicsThreadCreate("acquisitionTask",
                epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)acquisitionTaskC, this) == NULL);
    if (status)
    {
        printf("%s:%s epicsThreadCreate failure for image task\n",
                driverName, functionName);
        return;
    } else printf("OK\n");

    /* Set default values for parameters that cannot be read from Handel */
    for (i=0; i<this->nChannels; i++) {
        setIntegerParam(i, NDDxpTraceMode, NDDxpTraceADC);
        setDoubleParam(i, NDDxpTraceTime, 0.1);
    }

    for (ch=0; ch < this->nChannels; ch++) CALLHANDEL( xiaStopRun(ch), "xiaStopRun" )
    /* Read the MCA and DXP parameters once */
    printf("  Calling getDxpParams with DXP_ALL              ");
    this->getDxpParams(this->pasynUserSelf, DXP_ALL);
    printf("OK\n");
    printf("  Calling getAcquisitionStatus with DXP_ALL      ");
    this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
    printf("OK\n");
    printf("  Calling getAcquisitionStatistics with DXP_ALL  ");
    this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
    printf("OK\n");
}

/* virtual methods to override from ADDriver */
asynStatus NDDxp::writeInt32( asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int channel, rbValue, xiastatus, chStep = 1;
    int addr;
    int acquiring, numChans, mode;
    const char* functionName = "writeInt32";
    int firstCh, ignored;

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s [%s]: function=%d value=%d addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    switch(function) {
        case NDDxpXMAPMode:
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s::%s change mode to %d\n",
                driverName, functionName, value);
            status = this->changeMode(pasynUser, value);
            break;

        case NDDxpStartMode:
            if (value == 1 || value == 0) {status = asynSuccess;}
            else {status = asynError;}
            break;

        case NDDxpNextPixel:
            if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
            for (firstCh = 0; firstCh < this->nChannels; firstCh += chStep)
            {
                CALLHANDEL( xiaBoardOperation(firstCh, "mapping_pixel_next", (void*)&ignored), "mapping_pixel_next" )
            }
            setIntegerParam(addr, function, 0);
            break;

        case mcaErase:
            setIntegerParam(addr, NDDxpStartMode, 0);
            getIntegerParam(addr, mcaNumChannels, &numChans);
            getIntegerParam(addr, mcaAcquiring, &acquiring);
            if (acquiring) {
                xiaStopRun(channel);
                setIntegerParam(addr, NDDxpStartMode, 1);
                CALLHANDEL(xiaStartRun(channel, 0), "xiaStartRun(channel, 1)");
            }
            if (channel == DXP_ALL) {
                memset(this->pMcaRaw[0], 0, this->nChannels * numChans * sizeof(epicsUInt32));
            } else {
                memset(this->pMcaRaw[addr], 0, numChans * sizeof(epicsUInt32));
            }
            break;

        case mcaStartAcquire:
            status = this->startAcquiring(pasynUser);
            break;

        case mcaStopAcquire:
            CALLHANDEL(xiaStopRun(channel), "xiaStopRun(detChan)");
            //status = this->stopAcquiring(pasynUser);
            break;


        case mcaNumChannels:
            // rbValue not used here, call setIntegerParam if needed.
            status = this->setNumChannels(pasynUser, value, &rbValue);
            break;

        case mcaReadStatus:
            getIntegerParam(NDDxpXMAPMode, &mode);
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s::%s mcaReadStatus [%d] mode=%d\n", 
                driverName, functionName, function, mode);
            status = this->getAcquisitionStatus(pasynUser, addr);
            if (mode == NDDxpModeNormal) {
                /* If we are acquiring then read the statistics, else we use the cached values */
                getIntegerParam(addr, mcaAcquiring, &acquiring);
                if (acquiring) status = this->getAcquisitionStatistics(pasynUser, addr);
            }
            break;

        case mcaPresetCounts:
        case NDDxpPresetCountMode:
            this->setPresets(pasynUser, addr);
            break;

        case NDDxpReadDXPParams:
            this->getDxpParams(pasynUser, addr);
            break;

        case NDDxpDetectorPolarity:
        case NDDxpEnableBaselineCut:
        case NDDxpBaselineAverage:
            this->setDxpParam(pasynUser, addr, function, (double)value);
            break;
    }


    /* Call the callback */
    callParamCallbacks(addr, addr);
    return status;
}

asynStatus NDDxp::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    const char *functionName = "writeFloat64";

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s [%s]: function=%d value=%f addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setDoubleParam(addr, function, value);

    switch(function) {
        case mcaPresetRealTime:
        case mcaPresetLiveTime:
            this->setPresets(pasynUser, addr);
            break;
        case NDDxpPeakingTime:
        case NDDxpDynamicRange:
        case NDDxpTriggerThreshold:
        case NDDxpBaselineThreshold:
        case NDDxpEnergyThreshold:
        case NDDxpCalibrationEnergy:
        case NDDxpADCPercentRule:
        case NDDxpPreampGain:
        case NDDxpDetectorPolarity:
        case NDDxpResetDelay:
        case NDDxpGapTime:
        case NDDxpTriggerPeakingTime:
        case NDDxpTriggerGapTime:
        case NDDxpBaselineCut:
        case NDDxpMaxWidth:
        case NDDxpMaxEnergy:
            this->setDxpParam(pasynUser, addr, function, value);
            break;
    }

    /* Call the callback */
    callParamCallbacks(addr, addr);

    return status;
}

asynStatus NDDxp::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    int nBins, acquiring,mode;
    int ch;
    const char *functionName = "readInt32Array";

    channel = this->getChannel(pasynUser, &addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d function=%d\n",
        driverName, functionName, addr, channel, function);
    switch (function) {
        case NDDxpTrace:
            status = this->getTrace(pasynUser, channel, value, nElements, nIn);
            break;

        case NDDxpBaselineHistogram:
            status = this->getBaselineHistogram(pasynUser, channel, value, nElements, nIn);
            break;

        case mcaData:
            if (channel == DXP_ALL)
            {
                // if the MCA ALL channel is being read - force reading of all individual
                // channels using the NDDxpForceRead command.
                for (ch = 0; ch < this->nChannels; ch++)
                {
                    setIntegerParam(ch, NDDxpForceRead, 1);
                    callParamCallbacks(ch, ch);
                    setIntegerParam(ch, NDDxpForceRead, 0);
                    callParamCallbacks(ch, ch);
                }
                break;
            }
            getIntegerParam(channel, mcaNumChannels, &nBins);
            if (nBins > (int)nElements) nBins = (int)nElements;
            getIntegerParam(channel, mcaAcquiring, &acquiring);
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
                driverName, functionName, channel, nBins, acquiring);
            *nIn = nBins;
            getIntegerParam(NDDxpXMAPMode, &mode);

            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s::%s mode=%d acquiring=%d\n",
                driverName, functionName, mode, acquiring);
            if (acquiring)
            {
                if (mode == NDDxpModeNormal)
                {
                    /* While acquiring we'll force reading the data from the HW */
                    this->getMcaData(pasynUser, addr);
                } else if (mode == NDDxpModeMapping)
                {
                    /* TODO: need a function call here to parse the latest received
                     * data and post the result here... */
                }
            }

            /* Not sure if we should do this when in mapping mode but we need it in MCA mode... */
            memcpy(value, pMcaRaw[addr], nBins * sizeof(epicsUInt32));
            break;

        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::%s Function not implemented: [%d]\n",
                driverName, functionName, function);
            status = asynError;
            break;
    }
    return(status);
}


int NDDxp::getChannel(asynUser *pasynUser, int *addr)
{
    int channel;
    pasynManager->getAddr(pasynUser, addr);

    channel = *addr;
    switch (this->deviceType)
    {
    case NDDxpModelXMAP:
        if (*addr == this->nChannels) channel = DXP_ALL;
        break;
    default:
        break;
    }
    return channel;
}

asynStatus NDDxp::xmapGetModuleChannels(int currentChannel, int* firstChannel, int* nChannels)
{
    asynStatus status = asynSuccess;
    int modulus;
    if (this->deviceType != NDDxpModelXMAP)
    {
        *firstChannel = currentChannel;
        *nChannels = this->nChannels;
    } else
    {
        if (currentChannel == DXP_ALL)
        {
            *firstChannel = 0;
            *nChannels = this->nChannels;
        } else
        {
            modulus = currentChannel % XMAP_NCHANS_MODULE;
            *firstChannel = currentChannel - modulus;
            *nChannels = XMAP_NCHANS_MODULE;
        }

    }
    return status;
}

asynStatus NDDxp::apply(int channel)
{
    int i;
    asynStatus status=asynSuccess;
    int xiastatus, ignore;

    if (this->deviceType != NDDxpModelXMAP) return(asynSuccess);

    if (channel < 0) {
        for (i=0; i < this->nChannels; i+=XMAP_NCHANS_MODULE)
        {
            xiastatus = xiaBoardOperation(i, "apply", (void *)&ignore);
            status = this->xia_checkError(this->pasynUserSelf, xiastatus, "apply");
        }
    } else {
        xiastatus = xiaBoardOperation(channel, "apply", (void *)&ignore);
        status = this->xia_checkError(this->pasynUserSelf, xiastatus, "apply");
    }
    return(status);
}


asynStatus NDDxp::setPresets(asynUser *pasynUser, int addr)
{
    NDDxpPresetCountMode_t presetCountMode;
    double presetReal;
    double presetLive;
    double presetCounts;
    double presetValue;
    double presetType;
    int runActive=0;
    double time;
    int channel=addr;
    int channel0;
    const char* functionName = "setPresets";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    getDoubleParam(addr, mcaPresetRealTime, &presetReal);
    getDoubleParam(addr, mcaPresetLiveTime, &presetLive);
    getDoubleParam(addr, mcaPresetCounts, &presetCounts);
    getIntegerParam(addr, NDDxpPresetCountMode, (int *)&presetCountMode);

    xiaGetRunData(channel0, "run_active", &runActive);
    xiaStopRun(channel);

    /* If preset live and real time are both zero set to count forever */
    if ((presetLive == 0.) && (presetReal == 0.)) {
        presetValue = 0.;
        if (this->deviceType == NDDxpModelXMAP) {
            presetType = XIA_PRESET_NONE;
            xiaSetAcquisitionValues(channel, "preset_type", &presetType);
            xiaSetAcquisitionValues(channel, "preset_value", &presetValue);
            this->apply(channel);
        } else {
            xiaSetAcquisitionValues(channel, "preset_standard", &presetValue);
        }
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s:%s: addr=%d channel=%d cleared preset live and real time\n",
            driverName, functionName, addr, channel);
    }
    /* If preset live time is zero and real time is non-zero use real time */
    if ((presetLive == 0.) && (presetReal != 0.)) {
        time = presetReal;
        if (this->deviceType == NDDxpModelXMAP) {
            presetType = XIA_PRESET_FIXED_REAL;
            presetValue = time; /* *1.e6;  xMAP presets are in microseconds */
            xiaSetAcquisitionValues(channel, "preset_type", &presetType);
            xiaSetAcquisitionValues(channel, "preset_value", &presetValue);
            this->apply(channel);
        } else {
            xiaSetAcquisitionValues(channel, "preset_runtime", &time);
        }
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s:%s: addr=%d channel=%d set preset real time = %f\n",
            driverName, functionName, addr, channel, time);
    }
    /* If preset live time is non-zero use live time */
    if (presetLive != 0.) {
        time = presetLive;
        if (this->deviceType == NDDxpModelXMAP) {
            presetType = XIA_PRESET_FIXED_LIVE;
            presetValue = time; /* 1.e6;  xMAP presets are in microseconds */
            xiaSetAcquisitionValues(channel, "preset_type", &presetType);
            xiaSetAcquisitionValues(channel, "preset_value", &presetValue);
            this->apply(channel);
        } else {
            xiaSetAcquisitionValues(channel, "preset_livetime", &time);
        }
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s:%s: addr=%d channel=%d set preset live time = %f\n",
            driverName, functionName, addr, channel, time);
    }
    if (runActive) xiaStartRun(channel, 1);
    return(asynSuccess);
}

asynStatus NDDxp::setDxpParam(asynUser *pasynUser, int addr, int function, double value)
{
    int channel = addr;
    int channel0;
    int runActive=0;
    double dvalue=value;
    int numMcaChannels;
    int xiastatus;
    asynStatus status=asynSuccess;

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    xiaGetRunData(channel0, "run_active", &runActive);
    xiaStopRun(channel);

    switch (function) {
        case NDDxpPeakingTime:
            xiastatus = xiaSetAcquisitionValues(channel, "peaking_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting peaking_time");
            break;
        case NDDxpDynamicRange:
            /* dynamic_range is only supported on the xMAP */
            if (this->deviceType == NDDxpModelXMAP) {
                /* Convert from eV to keV */
                dvalue = value * 1000.;
                xiastatus = xiaSetAcquisitionValues(channel, "dynamic_range", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting dynamic_range");
            }
            break;
        case NDDxpTriggerThreshold:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_threshold", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_threshold");
            break;
        case NDDxpBaselineThreshold:
             dvalue = value * 1000.;    /* Convert to eV */
             xiastatus = xiaSetAcquisitionValues(channel, "baseline_threshold", &dvalue);
             status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_threshold");
             break;
        case NDDxpEnergyThreshold:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "energy_threshold", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting energy_threshold");
            break;
        case NDDxpCalibrationEnergy:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "calibration_energy", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting calibration_energy");
            break;
        case NDDxpADCPercentRule:
            xiastatus = xiaSetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting adc_percent_rule");
            break;
        case NDDxpPreampGain:
            xiastatus = xiaSetAcquisitionValues(channel, "preamp_gain", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting preamp_gain");
            break;
        case NDDxpDetectorPolarity:
            xiastatus = xiaSetAcquisitionValues(channel, "detector_polarity", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting detector_polarity");
            break;
        case NDDxpResetDelay:
            xiastatus = xiaSetAcquisitionValues(channel, "reset_delay", &dvalue);
            break;
        case NDDxpGapTime:
printf("Setting minimum_gap_time = %f\n", dvalue);
            if (this->deviceType == NDDxpModelXMAP) {
                /* On the xMAP the parameter that can be written is minimum_gap_time */
                xiastatus = xiaSetAcquisitionValues(channel, "minimum_gap_time", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "minimum_gap_time");
            } else {
               /* On the Saturn and DXP2X it is gap_time */
                xiastatus = xiaSetAcquisitionValues(channel, "gap_time", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting gap_time");
            }
            break;
        case NDDxpTriggerPeakingTime:
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_peaking_time");
            break;
        case NDDxpTriggerGapTime:
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_gap_time");
            break;
        case NDDxpBaselineAverage:
            if (this->deviceType == NDDxpModelXMAP) {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_average", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_average");
            } else {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_filter_length");
            }
            break;
        case NDDxpMaxWidth:
            xiastatus = xiaSetAcquisitionValues(channel, "maxwidth", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting maxwidth");
            break;
        case NDDxpBaselineCut:
            /* The xMAP does not support this yet */
            if (this->deviceType != NDDxpModelXMAP) {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_cut", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_cut");
                break;
            }
        case NDDxpEnableBaselineCut:
            /* The xMAP does not support this yet */
            if (this->deviceType != NDDxpModelXMAP) {
                xiastatus = xiaSetAcquisitionValues(channel, "enable_baseline_cut", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting enable_baseline_cut");
            }
            break;
        case NDDxpMaxEnergy:
            getIntegerParam(addr, mcaNumChannels, &numMcaChannels);
            if (numMcaChannels <= 0.)
                numMcaChannels = 2048;
            /* Set the bin width in eV */
            dvalue = value * 1000. / numMcaChannels;
            xiastatus = xiaSetAcquisitionValues(channel, "mca_bin_width", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting mca_bin_width");
            /* We always make the calibration energy be 50% of MaxEnergy */
            dvalue = value * 1000. / 2.;
            xiastatus = xiaSetAcquisitionValues(channel, "calibration_energy", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting calibration_energy");
            break;
    }
    this->apply(channel);
    this->getDxpParams(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    return asynSuccess;
}

asynStatus NDDxp::setNumChannels(asynUser* pasynUser, epicsInt32 value, epicsInt32 *rbValue)
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int i, bufLen;
    int modulus, realnbins, dummy, maxNumPixelsInBuffer=0, mode=0;
    double dblNbins, dNumPixPerBuf = -1.0, dTmp;
    const char* functionName = "changeSizeX";

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s new number of bins: %d\n", 
        driverName, functionName, value);

    if (value > XMAP_MAX_MCA_BINS || value < XMAP_MCA_BIN_RES)
    {
        status = asynError;
        return status;
    }

    /* TODO: check here whether the xmap is acquiring (if so, break out...)  */

    /* MCA number of bins on the xmap has a resolution of XMAP_MCA_BIN_RES (256)
     * bins per channel. Since the user can enter any value we must cap this to
     * the closest multiple of 256 */
    modulus = value % XMAP_MCA_BIN_RES;
    if (modulus == 0) realnbins = value;
    else if (modulus < XMAP_MCA_BIN_RES/2) realnbins = value - modulus;
    else realnbins = value + (XMAP_MCA_BIN_RES - modulus);
    *rbValue = realnbins;
    dblNbins = (epicsFloat64)*rbValue;

    for(i = 0; i < this->nChannels; i++)
    {
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "xiaSetAcquisitinValues ch=%d nbins=%.1f\n", 
            i, dblNbins);
        xiastatus = xiaSetAcquisitionValues(i, "number_mca_channels", (void*)&dblNbins);
        status = this->xia_checkError(pasynUser, xiastatus, "number_mca_channels");
        if (status == asynError)
            {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s [%s] can not set nbins=%u (%.3f) ch=%u\n",
                driverName, functionName, this->portName, *rbValue, dblNbins, i);
            return status;
            }
        setIntegerParam(i, mcaNumChannels, *rbValue);
        callParamCallbacks(i,i);
    }

    /* If in mapping mode we need to modify the Y size as well in order to fit in the 2MB buffer!
     * We also need to read out the new lenght of the mapping buffer because that can possibly change... */
    getIntegerParam(NDDxpXMAPMode, &mode);
    if (mode == NDDxpModeMapping)
    {
        for (i = 0; i < this->nChannels; i+=XMAP_NCHANS_MODULE)
        {
            xiastatus = xiaSetAcquisitionValues(i, "num_map_pixels_per_buffer", (void*)&dNumPixPerBuf);
            status = this->xia_checkError(pasynUser, xiastatus, "num_map_pixels_per_buffer");
            xiastatus = xiaBoardOperation(i, "apply", (void*)&dummy);
            status = this->xia_checkError(pasynUser, xiastatus, "apply");

            xiastatus = xiaGetAcquisitionValues(i, "num_map_pixels_per_buffer", (void*)&dTmp);
            status = this->xia_checkError(pasynUser, xiastatus, "GET num_map_pixels_per_buffer");
            maxNumPixelsInBuffer = (int)dTmp;
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                "%s::%s [%d] Got num_map_pixels_per_buffer = %d\n", 
                driverName, functionName, i, maxNumPixelsInBuffer);

            bufLen = 0;
            xiastatus = xiaGetRunData(i, "buffer_len", (void*)&bufLen);
            status = this->xia_checkError(pasynUser, xiastatus, "GET buffer_len");
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s::%s [%d] Got buffer_len = %u\n",
                driverName, functionName, i, bufLen);
            setIntegerParam(i, NDDxpArraySize, (int)bufLen);
            callParamCallbacks(i, i);
        }
        setIntegerParam(NDDxpNumPixelsInBuffer, maxNumPixelsInBuffer);
        callParamCallbacks();
    }

    this->apply(DXP_ALL);

    return status;
}

asynStatus NDDxp::changeMode(asynUser *pasynUser, epicsInt32 mode)
{
    asynStatus status = asynSuccess;
    int xiastatus, acquiring;
    int i;
    int firstCh, chStep = 1, bufLen;
    double dummy, dMode=0, dTmp, dNumPixPerBuf = -1.0;
    const char* functionName = "changeMode";

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s Changing mode to %d\n",
        driverName, functionName, mode);

    if (mode < 0 || mode > 1) return asynError;
    getIntegerParam(mcaAcquiring, &acquiring);
    if (acquiring) return asynError;

    dMode = (double)mode;
    CALLHANDEL( xiaSetAcquisitionValues(DXP_ALL, "mapping_mode", (void*)&dMode), "mapping_mode" )
    if (status == asynError) return status;

    switch(mode)
    {
    case NDDxpModeNormal:
        getIntegerParam(mcaNumChannels, (int*)&bufLen);
        setIntegerParam(NDDxpDataType, NDUInt32);
        for (i=0; i < this->nChannels; i++)
        {
            /* Clear the normal mapping mode status settings */
            setIntegerParam(i, NDDxpEvents, 0);
            setDoubleParam(i, NDDxpInputCountRate, 0);
            setDoubleParam(i, NDDxpOutputCountRate, 0);

            /* Set the new ArraySize down to just one buffer length */
            setIntegerParam(i, NDDxpArraySize, (int)bufLen);
            callParamCallbacks(i,i);
        }
        break;
    case NDDxpModeMapping:
        int nPixelsInBuffer = 0;
        setIntegerParam(NDDxpDataType, NDUInt16);
        if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
        for (firstCh = 0; firstCh < this->nChannels; firstCh+=chStep)
        {
            xiastatus = xiaSetAcquisitionValues(firstCh, "num_map_pixels_per_buffer", (void*)&dNumPixPerBuf);
            status = this->xia_checkError(pasynUser, xiastatus, "num_map_pixels_per_buffer");
            xiastatus = xiaBoardOperation(firstCh, "apply", (void*)&dummy);
            status = this->xia_checkError(pasynUser, xiastatus, "apply");
        }

        for (firstCh = 0; firstCh < this->nChannels; firstCh+=chStep)
        {
            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(firstCh, "num_map_pixels_per_buffer", (void*)&dTmp);
            status = this->xia_checkError(pasynUser, xiastatus, "GET num_map_pixels_per_buffer");
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                "%s::%s [%d] Got num_map_pixels_per_buffer = %.1f\n", 
                driverName, functionName, firstCh, dTmp);
            nPixelsInBuffer = (int)dTmp;

            bufLen = 0;
            xiastatus = xiaGetRunData(firstCh, "buffer_len", (void*)&bufLen);
            status = this->xia_checkError(pasynUser, xiastatus, "GET buffer_len");
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                "%s::%s [%d] Got buffer_len = %u\n", 
                driverName, functionName, firstCh, bufLen);
            setIntegerParam(firstCh, NDDxpArraySize, (int)bufLen);
            callParamCallbacks(firstCh, firstCh);

            /* Clear the normal mapping mode status settings */
            for (i=0; i < XMAP_NCHANS_MODULE; i++)
            {
                setDoubleParam(firstCh+i, NDDxpTriggers, 0);
                setDoubleParam(firstCh+i, NDDxpElapsedRealTime, 0);
                setDoubleParam(firstCh+i, NDDxpElapsedTriggerLiveTime, 0);
                setDoubleParam(firstCh+i, NDDxpElapsedLiveTime, 0);
                callParamCallbacks(firstCh+i, firstCh+i);
            }
        }
        setIntegerParam(NDDxpNumPixelsInBuffer, nPixelsInBuffer);
        break;
    }

    return status;
}

asynStatus NDDxp::getAcquisitionStatus(asynUser *pasynUser, int addr)
{
    int acquiring=0, run_active;
    int ivalue;
    int channel=addr;
    asynStatus status;
    int xiastatus;
    int i, chStep = 1;
    const char *functionName = "getAcquisitionStatus";

    if (addr == this->nChannels) channel = DXP_ALL;
    else if (addr == DXP_ALL) addr = this->nChannels;
    //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
    //    "%s::%s addr=%d channel=%d\n", 
    //    driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
        for (i=0; i<this->nChannels; i+=chStep) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatus(pasynUser, i);
            getIntegerParam(i, mcaAcquiring, &ivalue);
            acquiring = MAX(acquiring, ivalue);
        }
        setIntegerParam(addr, mcaAcquiring, acquiring);
    } else {
        /* Get the run time status from the handel library - informs whether the
         * HW is acquiring or not.        */
        CALLHANDEL( xiaGetRunData(channel, "run_active", &run_active), "xiaGetRunData (run_active)" )
        /* If Handel thinks the run is active, but the hardware does not, then
         * stop the run */
        if (run_active == XIA_RUN_HANDEL)
            CALLHANDEL( xiaStopRun(channel), "xiaStopRun")
        /* Get the acquiring state from the XIA hardware */
        acquiring = (run_active & XIA_RUN_HARDWARE);
        setIntegerParam(addr, mcaAcquiring, acquiring);
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s addr=%d channel=%d: acquiring=%d\n",
        driverName, functionName, addr, channel, acquiring);
    return(asynSuccess);
}

asynStatus NDDxp::getAcquisitionStatistics(asynUser *pasynUser, int addr)
{
    double dvalue, triggerLiveTime=0, energyLiveTime=0, realTime=0, icr=0, ocr=0;
    int events=0, triggers=0;
    int ivalue;
    int acquiring=0;
    int channel=addr;
    int resume;
    int i;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == this->nChannels) channel = DXP_ALL;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d\n", 
        driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s start DXP_ALL\n", 
            driverName, functionName);
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatistics(pasynUser, i);
            getDoubleParam(i, mcaElapsedLiveTime, &dvalue);
            energyLiveTime = MAX(energyLiveTime, dvalue);
            getDoubleParam(i, NDDxpElapsedTriggerLiveTime, &dvalue);
            triggerLiveTime = MAX(triggerLiveTime, dvalue);
            getDoubleParam(i, mcaElapsedRealTime, &realTime);
            realTime = MAX(realTime, dvalue);
            getIntegerParam(i, NDDxpEvents, &ivalue);
            events = MAX(events, ivalue);
            getIntegerParam(i, NDDxpTriggers, &ivalue);
            triggers = MAX(triggers, ivalue);
            getDoubleParam(i, NDDxpInputCountRate, &dvalue);
            icr = MAX(icr, dvalue);
            getDoubleParam(i, NDDxpOutputCountRate, &dvalue);
            ocr = MAX(ocr, dvalue);
            getIntegerParam(i, mcaAcquiring, &ivalue);
            acquiring = MAX(acquiring, ivalue);
        }
        setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
        setDoubleParam(addr, NDDxpElapsedTriggerLiveTime, triggerLiveTime);
        setDoubleParam(addr, mcaElapsedRealTime, realTime);
        setIntegerParam(addr,NDDxpEvents, events);
        setIntegerParam(addr, NDDxpTriggers, triggers);
        setDoubleParam(addr, NDDxpInputCountRate, icr);
        setDoubleParam(addr, NDDxpOutputCountRate, ocr);
        setIntegerParam(addr, mcaAcquiring, acquiring);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s end DXP_ALL\n", 
            driverName, functionName);
    } else {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s start channel %d\n", 
            driverName, functionName, addr);
        getIntegerParam(addr, NDDxpStartMode, &resume);
        //if (this->pChannel[addr].erased) {
        if (resume == 0) {
            setDoubleParam(addr, mcaElapsedLiveTime, 0);
            setDoubleParam(addr, mcaElapsedRealTime, 0);
            setIntegerParam(addr, NDDxpEvents, 0);
            setDoubleParam(addr, NDDxpInputCountRate, 0);
            setDoubleParam(addr, NDDxpOutputCountRate, 0);
            setIntegerParam(addr, NDDxpTriggers, 0);
            setDoubleParam(addr, NDDxpElapsedTriggerLiveTime, 0);
        } else {
            xiaGetRunData(channel, "runtime", &realTime);
            setDoubleParam(addr, mcaElapsedRealTime, realTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d runtime=%f\n", 
                driverName, functionName, addr, realTime);
            if (this->deviceType == NDDxpModelXMAP) {
                xiaGetRunData(channel, "trigger_livetime", &triggerLiveTime);
            } else {
                xiaGetRunData(channel, "livetime", &triggerLiveTime);
            }
            setDoubleParam(addr, NDDxpElapsedTriggerLiveTime, triggerLiveTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d trigger livetime=%f\n", 
                driverName, functionName, addr, triggerLiveTime);

            xiaGetRunData(channel, "events_in_run", &events);
            setIntegerParam(addr, NDDxpEvents, events);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d events=%d\n", 
                driverName, functionName, addr, events);

            if (this->deviceType == NDDxpModelXMAP) {
                // We cannot read triggers on the xMAP unless we read module_statistics which is too
                // complex for now.  Instead read ICR and compute triggers
                xiaGetRunData(channel, "input_count_rate", &icr);
                triggers = (int)(ocr * triggerLiveTime);
            } else {
               xiaGetRunData(channel, "triggers", &triggers);
            }
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d triggers=%d\n", 
                driverName, functionName, addr, triggers);
            setIntegerParam(addr, NDDxpTriggers, triggers);
            
            /* Note - we could read ICR and OCR, but these can be computed from 
             * the above values, and it takes 10msec each to read them, so don't */
            icr = triggers/triggerLiveTime;
            ocr = events/realTime; 

            //xiaGetRunData(channel, "input_count_rate", &icr);
            setDoubleParam(addr, NDDxpInputCountRate, icr);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                "%s::%s  channel %d ICR=%f\n", 
                driverName, functionName, addr, icr);
            //xiaGetRunData(channel, "output_count_rate", &ocr);
            setDoubleParam(addr, NDDxpOutputCountRate, ocr);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d OCR=%f\n", 
                driverName, functionName, addr, ocr);

            if (this->deviceType == NDDxpModelXMAP) {
                // On the xMAP we can read the energy live time.  We need to determine if this is more
                // accurate than the computed value we use on the Saturn below
                xiaGetRunData(channel, "livetime", &energyLiveTime);
            } else {
                // The Saturn and DXP4C-2X don't have an energy livetime readout.
                if (icr < 1.0) icr = 1.0;
                energyLiveTime = realTime * ocr/icr;
            }
            setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d energy livetime=%f\n", 
                driverName, functionName, addr, energyLiveTime);

            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s end channel %d\n", 
                driverName, functionName, addr);
        }
    }
    //asynPrint(pasynUser, ASYN_TRACE_FLOW,
    //    "%s::%s addr=%d channel=%d: acquiring=%d\n",
    //    driverName, functionName, addr, channel, acquiring);
    return(asynSuccess);
}

asynStatus NDDxp::getDxpParams(asynUser *pasynUser, int addr)
{
    int i;
    double mcaBinWidth, numMcaChannels;
    double dvalue;
    double emax;
    int channel=addr;

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getDxpParams(pasynUser, i);
        }
    } else {
        xiaGetAcquisitionValues(channel, "energy_threshold", &dvalue);
        /* Convert energy threshold from eV to keV */
        dvalue /= 1000.;
        setDoubleParam(channel, NDDxpEnergyThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "peaking_time", &dvalue);
        setDoubleParam(channel, NDDxpPeakingTime, dvalue);
        xiaGetAcquisitionValues(channel, "gap_time", &dvalue);
        setDoubleParam(channel, NDDxpGapTime, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_threshold", &dvalue);
        /* Convert trigger threshold from eV to keV */
        dvalue = dvalue / 1000.;
        setDoubleParam(channel, NDDxpTriggerThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
        setDoubleParam(channel, NDDxpTriggerPeakingTime, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
        setDoubleParam(channel, NDDxpTriggerGapTime, dvalue);
        xiaGetAcquisitionValues(channel, "preamp_gain", &dvalue);
        setDoubleParam(channel, NDDxpPreampGain, dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
           xiaGetAcquisitionValues(channel, "baseline_average", &dvalue);
        } else {
           xiaGetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
        }
        setIntegerParam(channel, NDDxpBaselineAverage, (int)dvalue);
        xiaGetAcquisitionValues(channel, "baseline_threshold", &dvalue);
        dvalue/= 1000.;  /* Convert to keV */
        setDoubleParam(channel, NDDxpBaselineThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "maxwidth", &dvalue);
        setDoubleParam(channel, NDDxpMaxWidth, dvalue);
        if (this->deviceType != NDDxpModelXMAP) {
           xiaGetAcquisitionValues(channel, "baseline_cut", &dvalue);
            setDoubleParam(channel, NDDxpBaselineCut, dvalue);
           xiaGetAcquisitionValues(channel, "enable_baseline_cut", &dvalue);
            setIntegerParam(channel, NDDxpEnableBaselineCut, (int)dvalue);
        }
        xiaGetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
        setDoubleParam(channel, NDDxpADCPercentRule, dvalue);
        if (this->deviceType == NDDxpModelXMAP) {
            xiaGetAcquisitionValues(channel, "dynamic_range", &dvalue);
        } else dvalue = 0.;
        setDoubleParam(channel, NDDxpDynamicRange, dvalue);
        xiaGetAcquisitionValues(channel, "calibration_energy", &dvalue);
        setDoubleParam(channel, NDDxpCalibrationEnergy, dvalue);
        xiaGetAcquisitionValues(channel, "mca_bin_width", &mcaBinWidth);
        setDoubleParam(channel, NDDxpMCABinWidth, mcaBinWidth);
        xiaGetAcquisitionValues(channel, "number_mca_channels", &numMcaChannels);
        setIntegerParam(channel, mcaNumChannels, (int)numMcaChannels);
        /* Compute emax from mcaBinWidth and mcaNumChannels, convert eV to keV */
        emax = numMcaChannels * mcaBinWidth / 1000.;
        setDoubleParam(channel, NDDxpMaxEnergy, emax);
        xiaGetAcquisitionValues(channel, "detector_polarity", &dvalue);
        setIntegerParam(channel, NDDxpDetectorPolarity, (int)dvalue);
        xiaGetAcquisitionValues(channel, "decay_time", &dvalue);
        setDoubleParam(channel, NDDxpDecayTime, dvalue);
        xiaGetAcquisitionValues(channel, "reset_delay", &dvalue);
        setDoubleParam(channel, NDDxpResetDelay, dvalue);

        // SCAs need work
        //xiaGetAcquisitionValues(channel, "number_of_scas", &dvalue);
        //for (i=0; i<pdxpReadbacks->number_scas; i++) {
        //   xiaGetAcquisitionValues(channel, sca_lo[i], &pdxpReadbacks->sca_lo[i]);
        //    xiaGetAcquisitionValues(channel, sca_hi[i], &pdxpReadbacks->sca_hi[i]);
        //}
        ///* The sca data on the xMAP is double, it is long on other products */
        //if (minfo->moduleType == DXP_XMAP) {
        //    xiaGetRunData(channel, "sca", pdxpReadbacks->sca_counts);
        //} else {
        //    long long_sca_counts[NUM_DXP_SCAS];
        //    xiaGetRunData(channel, "sca", long_sca_counts);
        //    for (i=0; i<pdxpReadbacks->number_scas; i++) {
        //        pdxpReadbacks->sca_counts[i] = long_sca_counts[i];
        //    }
        //}
    }
    return(asynSuccess);
}


asynStatus NDDxp::getMcaData(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    int xiastatus, paramStatus;
    int arrayCallbacks;
    int nChannels;
    int channel=addr;
    int spectrumCounter;
    int i;
    NDDataType_t dataType;
    epicsTimeStamp now;
    const char* functionName = "getMcaData";

    if (addr == this->nChannels) channel = DXP_ALL;

    paramStatus  = getIntegerParam(mcaNumChannels, &nChannels);
    paramStatus |= getIntegerParam(NDDxpArrayCallbacks, &arrayCallbacks);
    paramStatus |= getIntegerParam(NDDxpDataType, (int *)&dataType);
    paramStatus |= getIntegerParam(NDDxpPixelCounter, &spectrumCounter);

    epicsTimeGetCurrent(&now);

    /* If this is an xMAP and channel=DXP_ALL we can do an optimisation by reading all of the spectra
     * on a single xMAP with a single call. */
    if ((this->deviceType == NDDxpModelXMAP) && (channel==DXP_ALL)) {
        //xiaGetRunDataCmd = "module_mca";
        //nChansOnBoard = XMAP_NCHANS_MODULE;

    }

    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
                this->getMcaData(pasynUser, i);
        }
    } else {

        getIntegerParam(addr, NDDxpPixelCounter, &spectrumCounter);

        /* Read the MCA spectrum from Handel.
        * For most devices this means getting 1 channel spectrum here.
        * For the XMAP we get all 4 channels on the board in one go here */
        CALLHANDEL( xiaGetRunData(addr, "mca", (void*)this->pMcaRaw[addr]),"xiaGetRunData")
        spectrumCounter++;
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, (const char *)pMcaRaw[addr], nChannels*sizeof(epicsUInt32),
            "%s::%s Got MCA spectrum channel:%d ptr:%p\n",
            driverName, functionName, channel, pMcaRaw[addr]);

        if (arrayCallbacks)
        {
            /* Allocate a buffer for callback
             * -and just telling it where to find the data in the pMcaRaw buffer... */
            this->pRaw = this->pNDArrayPool->alloc(1, &nChannels, dataType, 0, (void*)(pMcaRaw[addr]) );
            this->pRaw->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
            this->pRaw->uniqueId = spectrumCounter;
            //this->unlock();
            doCallbacksGenericPointer(this->pRaw, NDDxpArrayData, addr);
            //this->lock();
            this->pRaw->release();
        }
    }
    return status;
}

asynStatus NDDxp::getMappingData(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    int xiastatus, paramStatus=asynSuccess;
    int arrayCallbacks;
    int channel=addr;
    int ch, chStep = 1, bufLen;
    NDDataType_t dataType;
    int buf = 0, nWords, i;
    epicsUInt32 *data = this->pMapRaw;
    int dims[2], pixelCounter, bufferCounter, itemsInArray;
    epicsTimeStamp now, after;
    double readoutTime, readoutBurstSpeed, mbytesRead, MBbufSize;
    const char* functionName = "getMappingData";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;

    if (channel == DXP_ALL) /* All channels */
    {
        for (ch=0; ch<this->nChannels; ch+=chStep)
        {
            /* Call ourselves recursively but with a specific channel */
            status = this->getMappingData(pasynUser, ch);
        }
        return status;
    }

    paramStatus |= getIntegerParam(NDDxpDataType, (int *)&dataType);
    paramStatus |= getIntegerParam(NDDxpPixelCounter, &pixelCounter);
    paramStatus |= getIntegerParam(NDDxpBufferCounter, &bufferCounter);
    paramStatus |= getIntegerParam(NDDxpArraySize, &itemsInArray);
    paramStatus |= getDoubleParam(NDDxpMBytesReceived, &mbytesRead);
    MBbufSize = (double)((itemsInArray)*sizeof(epicsUInt16)) / (double)MEGABYTE;

    epicsTimeGetCurrent(&now);
    bufferCounter++;
    buf = this->currentBuf[channel];

    CALLHANDEL( xiaGetRunData(channel, "buffer_len", (void*)&bufLen) , "buffer_len")
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s buffer length: %d Getting data...\n",
        driverName, functionName, bufLen);

    /* the buffer is full so read it out */
    CALLHANDEL( xiaGetRunData(channel, NDDxpBuffers[buf].buffer, (void*)data), "NDDxpBuffers[buf].buffer" )
    epicsTimeGetCurrent(&after);
    paramStatus |= getIntegerParam(NDDxpArrayCallbacks, &arrayCallbacks);

    readoutTime = epicsTimeDiffInSeconds(&after, &now);
    readoutBurstSpeed = MBbufSize / readoutTime;
    mbytesRead += MBbufSize;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
        driverName, functionName, MBbufSize, itemsInArray, readoutTime, readoutBurstSpeed);

    if (arrayCallbacks)
    {
        //this->pRaw = this->pNDArrayPool->alloc(2, dims, dataType, 0, (void*)data );
        this->pRaw = this->pNDArrayPool->alloc(1, dims, dataType, itemsInArray, NULL );

        /* First get rid of the empty parts of the 32 bit words. The Handel library
         * provides a 4MB 32 bit/word wide buffer -even though the data only takes up
         * 2MB 16bit word size. So we strip off the empty top 16bit of each 32 bit
         * word and is left with 16 bit valid data... */
        nWords = itemsInArray / sizeof(epicsUInt16);
        for(i = 0; i < nWords; i++)
        {
            *(((epicsUInt16*)this->pRaw->pData)+i) = (epicsUInt16)*(data + i);
        }

        this->pRaw->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
        this->pRaw->uniqueId = bufferCounter;
        this->pRaw->dataSize = itemsInArray;
        //this->unlock();
        doCallbacksGenericPointer(this->pRaw, NDDxpArrayData, channel);
        //this->lock();
        this->pRaw->release();

        setDoubleParam(NDDxpMBytesReceived, mbytesRead);
        setDoubleParam(NDDxpReadSpeed, readoutBurstSpeed);
        callParamCallbacks();
    }

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s Done reading! Ch=%d bufchar=%s\n",
        driverName, functionName, channel, NDDxpBuffers[buf].bufChar);
    /* Notify XMAP that we read out the buffer */
    CALLHANDEL( xiaBoardOperation(channel, "buffer_done", (void*)NDDxpBuffers[buf].bufChar), "buffer_done" )
    if (buf == 0) this->currentBuf[channel] = 1;
    else this->currentBuf[channel] = 0;

    return status;
}

/* Get trace data */
asynStatus NDDxp::getTrace(asynUser* pasynUser, int addr,
                           epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
    int xiastatus, channel=addr;
    int i;
    double info[2];
    double traceTime;
    int traceMode;
    //const char *functionName = "getTrace";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        getDoubleParam(channel, NDDxpTraceTime, &traceTime);
        getIntegerParam(channel, NDDxpTraceMode, &traceMode);
        info[0] = 0.;
        /* Convert from us to ns */
        info[1] = traceTime * 1000.;

        xiastatus = xiaDoSpecialRun(channel, NDDxpTraceCommands[traceMode], (void*)info);
        status = this->xia_checkError(pasynUser, xiastatus, NDDxpTraceCommands[traceMode]);
        if (status == asynError) return asynError;

        *actualLen = this->traceLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetSpecialRunData(channel, "adc_trace", (void*)this->traceBuffer);
        status = this->xia_checkError( pasynUser, xiastatus, "adc_trace" );
        if (status == asynError) return status;

        memcpy(data, this->traceBuffer, *actualLen * sizeof(epicsInt32));
    }
    return status;
}

/* Get trace data */
asynStatus NDDxp::getBaselineHistogram(asynUser* pasynUser, int addr,
                                       epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
    int i;
    int xiastatus, channel=addr;
    //const char *functionName = "getBaselineHistogram";

    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        *actualLen = this->baselineLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetRunData(channel, "baseline", this->baselineBuffer);
        status = this->xia_checkError(pasynUser, xiastatus, "baseline" );
        if (status == asynError) return status;

        memcpy(data, this->baselineBuffer, *actualLen * sizeof(epicsInt32));
    }
    return status;
}


/** Create an asyn user for the driver.
 * Maps the integer/enum asyn commands on to a string representation that
 * can be used to indicate a certain command in in the INP/OUT field of a record.
 * \param pasynUser
 * \param drvInfo
 * \param pptypeName
 * \param psize
 * \return asynStatus Either asynError or asynSuccess
 */
asynStatus NDDxp::drvUserCreate( asynUser *pasynUser, const char *drvInfo, const char **pptypeName, size_t *psize)
{
    asynStatus status = asynError;
    int param;
    const char *functionName = "drvUserCreate";

    status = findParam((asynParamString_t *)mcaCommands, MAX_MCA_COMMANDS, drvInfo, &param);
    if (status == asynSuccess)
    {
        pasynUser->reason = param;
        if (pptypeName) { *pptypeName = epicsStrDup(drvInfo); }
        if (psize) { *psize = sizeof(param); }
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: (mca) drvInfo=%s, param=%d\n",
            driverName, functionName, drvInfo, param);
    }

    /* Secondly see if this is one of the drivers local parameters */
    if (status != asynSuccess)
    {
        status = findParam(NDDxpParamString, NDDxp_N_PARAMS, drvInfo, &param);
        if (status == asynSuccess)
        {
            pasynUser->reason = param;
            if (pptypeName) { *pptypeName = epicsStrDup(drvInfo); }
            if (psize) { *psize = sizeof(param); }
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s:%s: (NDDxp) drvInfo=%s, param=%d\n",
                driverName, functionName, drvInfo, param);
        }
    }

    if (status != asynSuccess)
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
            "%s::%s ERROR did not find reason %s in either mcaCommands nor NDDxpParams table!\n",
            driverName, functionName, drvInfo);
    }

    return status;
}

asynStatus NDDxp::startAcquiring(asynUser *pasynUser)
{
    asynStatus status = asynSuccess;
    int resume, xiastatus;
    int channel, addr, i;
    int acquiring;
    const char *functionName = "startAcquire";

    channel = this->getChannel(pasynUser, &addr);
    getIntegerParam(addr, NDDxpStartMode, &resume);
    getIntegerParam(addr, mcaAcquiring, &acquiring);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s ch=%d acquiring=%d resume=%d\n",
        driverName, functionName, channel, acquiring, resume);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    /* make sure we use buffer A to start with */
    //for (firstCh=0; firstCh < this->nChannels; firstCh += XMAP_NCHANS_MODULE) this->currentBuf[firstCh] = 0;

    // do xiaStart command
    CALLHANDEL( xiaStartRun(channel, resume), "xiaStartRun()" )

    setIntegerParam(addr, NDDxpStartMode, 1); /* reset the resume flag */
    setIntegerParam(addr, mcaAcquiring, 1); /* Set the acquiring flag */

    if (channel == DXP_ALL) {
        for (i=0; i<this->nChannels; i++) {
            setIntegerParam(addr, mcaAcquiring, 1);
            callParamCallbacks(i, i);
        }
    }

    callParamCallbacks(addr, addr);

    // signal cmdStartEvent to start the polling thread
    this->cmdStartEvent->signal();
    return status;
}

/** Thread used to poll the hardware for status and data.
 *
 */
void NDDxp::acquisitionTask()
{
    asynUser *pasynUser = this->pasynUserSelf;
    int paramStatus;
    int i;
    int mode;
    int acquiring = 0;
    epicsFloat64 pollTime, sleeptime, dtmp;
    epicsTimeStamp now, start;
    const char* functionName = "acquisitionTask";

    setDoubleParam(NDDxpPollTime, 0.01);

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s acquisition task started!\n",
        driverName, functionName);
//    epicsEventTryWait(this->stopEventId); /* clear the stop event if it wasn't already */

    this->lock();

    while (this->polling) /* ... round and round and round we go ... */
    {

        getIntegerParam(this->nChannels, mcaAcquiring, &acquiring);

        if (!acquiring)
        {
            /* Wait for a signal that tells this thread that the transmission
             * has started and we can start asking for data...     */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%s]: waiting for acquire to start\n", 
                driverName, functionName, this->portName);

            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s:%s Waiting for acquisition to start!\n",
                driverName, functionName);
            this->cmdStartEvent->wait();
            this->lock();
            asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%s]: started! (mode=%d)\n", 
                driverName, functionName, this->portName, mode);
        }
        epicsTimeGetCurrent(&start);

        /* In this loop we only read the acquisition status to minimise overhead.
         * If a transition from acquiring to done is detected then we read the statistics
         * and the data. */
        this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
        getIntegerParam(this->nChannels, mcaAcquiring, &acquiring);
        getIntegerParam(NDDxpXMAPMode, &mode);
        //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        //    "%s::%s prevAcquiring=%d acquiring=%d mode=%d\n",
        //    driverName, functionName, prevAcquiring, acquiring, mode);
        if (mode == NDDxpModeNormal && (!acquiring))
        {
            /* There must have just been a transition from acquiring to not acquiring */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s Detected acquisition stop! Now reading statistics\n",
                driverName, functionName);
            this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s Detected acquisition stop! Now reading data\n",
                driverName, functionName);
            this->getMcaData(this->pasynUserSelf, DXP_ALL);
            //printf("acquisition task, detected stop, called getAcquisitionStatistics and getMcaData\n");
        } else if (mode == NDDxpModeMapping)
        {
            this->pollMappingMode();
        }

        /* Do callbacks for all channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s Doing callbacks\n",
            driverName, functionName);
        for (i=0; i<=this->nChannels; i++) callParamCallbacks(i, i);
        paramStatus |= getDoubleParam(NDDxpPollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleeptime = pollTime - dtmp;
        if (sleeptime > 0.0)
        {
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s Sleeping for %f seconds\n",
                driverName, functionName, sleeptime);
            this->unlock();
            epicsThreadSleep(sleeptime);
            this->lock();
        }
    }
}

/** Check if the current mapping buffer is full in which case it reads out the data */
asynStatus NDDxp::pollMappingMode()
{
    asynStatus status = asynSuccess;
    asynUser *pasynUser = this->pasynUserSelf;
    int xiastatus;
    int ch, chStep = 1, buf, isfull = 0;
    int currentPixel = 0;
    const char* functionName = "pollMappingMode";

    if (this->deviceType == NDDxpModelXMAP) chStep = XMAP_NCHANS_MODULE;
    /* Step through all the first channels on all the boards in the system if
     * the device is an XMAP, otherwise just step through each individual channel. */
    for (ch=0; ch < this->nChannels; ch+=chStep)
    {
        buf = this->currentBuf[ch];

        CALLHANDEL( xiaGetRunData(ch, "current_pixel", (void*)&currentPixel) , "current_pixel" )
        setIntegerParam(ch, NDDxpCurrentPixel, (int)currentPixel);
        callParamCallbacks(ch, ch);
        CALLHANDEL( xiaGetRunData(ch, NDDxpBuffers[buf].bufFull, (void*)&isfull), "NDDxpBuffers[buf].bufFull" )
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s::%s %s isfull=%d\n",
            driverName, functionName, NDDxpBuffers[buf].bufFull, isfull);
        if (isfull)
        {
            status = this->getMappingData(pasynUser, (int)ch);
        }
    }
    return status;
}


void NDDxp::report(FILE *fp, int details)
{
    asynNDArrayDriver::report(fp, details);
}



asynStatus NDDxp::xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, char *xiacmd )
{
    if (xiastatus == XIA_SUCCESS) return asynSuccess;

    asynPrint( pasynUser, ASYN_TRACE_ERROR, 
        "### NDDxp: XIA ERROR: %d (%s)\n", 
        xiastatus, xiacmd );
    return asynError;
}

void NDDxp::shutdown()
{
    int status;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s: shutting down...\n", driverName);
    this->polling = 0;
    epicsThreadSleep(1.0);
    status = xiaExit();
    if (status == XIA_SUCCESS)
    {
        printf("%s closed down successfully. You can now exit the iocshell ###\n",
            driverName);
        return;
    }
    printf("xiaExit() error: %d\n", status);
    return;
}

int NDDxp::getModuleType()
{
    /* This function returns an enum of type DXP_MODULE_TYPE for the module type.
     * It returns the type of the first module in the system.
     * IMPORTANT ASSUMPTION: It is assumed that a single EPICS IOC will only
     * be controlling a single type of XIA module (xMAP, Saturn, DXP2X)
     * If there is an error it returns -1.
     */
    char module_alias[MAXALIAS_LEN];
    char module_type[MAXITEM_LEN];
    int status;

    /* Get the module alias for the first channel */
    status = xiaGetModules_VB(0, module_alias);
    /* Get the module type for this module */
    status = xiaGetModuleItem(module_alias, "module_type", module_type);
    /* Look for known module types */
    if (strcmp(module_type, "xmap") == 0) return(NDDxpModelXMAP);
    if (strcmp(module_type, "dxpx10p") == 0) return(NDDxpModelSaturn);
    if (strcmp(module_type, "dxp4c2x") == 0) return(NDDxpModel4C2X);
    return(-1);
}

