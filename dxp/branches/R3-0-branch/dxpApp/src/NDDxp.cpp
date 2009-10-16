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

typedef enum {NDDxp_xMAP = 0, NDDxp_Saturn, NDDxp_4C2X} NDDxpDevice_t;
typedef enum {NDDxp_normal = 0, NDDxp_mapping } NDDxpmode_t;
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
    NDDxp_xmap_mode = lastMcaCommand,     /** < Change mode of the XMAP (0=mca; 1=mapping; 2=sca) (int32 read/write) addr: all/any */
    NDDxp_start_mode,                     /** < Start mode. (0=clear on start; 1=resume spectrum) */
    NDDxp_xmap_runstate,                  /** < XMAP reporting it's runtime state (int bitmap) */
    NDDxp_current_pixel,                  /** < XMAP mapping mode only: read the current pixel that is being acquired into (int) */
    NDDxp_next_pixel,                     /** < XMAP mapping mode only: force a pixel increment in the xmap buffer (write only int). Value is ignored. */
    NDDxp_pixels_per_buffer,
    NDDxp_buffer_overrun,
    NDDxp_mbytes_received,
    NDDxp_read_speed,

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
    NDDxp_stat_realtime,                  /** < real time in seconds (double) */
    NDDxp_stat_triggerlivetime,           /** < live time in seconds (double) */
    NDDxp_stat_energylivetime,            /** < live time in seconds (double) */
    NDDxp_stat_triggers,                  /** < number of triggers received (int) */
    NDDxp_stat_events,                    /** < total number of events registered (int) */
    NDDxp_stat_inputcountrate,            /** < input count rate in Hz (double) */
    NDDxp_stat_outputcountrate,           /** < output count rate in Hz (double) */

    NDDxp_xia_peaking_time,
    NDDxp_xia_dynamic_range,
    NDDxp_xia_trigger_threshold,
    NDDxp_xia_baseline_threshold,
    NDDxp_xia_energy_threshold,
    NDDxp_xia_calibration_energy,
    NDDxp_xia_adc_percent_rule,
    NDDxp_xia_mca_bin_width,
    NDDxp_xia_preamp_gain,
    NDDxp_xia_number_mca_channels,
    NDDxp_xia_detector_polarity,
    NDDxp_xia_reset_delay,
    NDDxp_xia_decay_time,
    NDDxp_xia_gap_time,
    NDDxp_xia_trigger_peaking_time,
    NDDxp_xia_trigger_gap_time,
    NDDxp_xia_baseline_average,
    NDDxp_xia_baseline_cut,
    NDDxp_xia_enable_baseline_cut,
    NDDxp_xia_maxwidth,

    NDDxpLastDriverParam
    } NDDxpParam_t;

static asynParamString_t NDDxpParamString[] = {
    {NDDxp_xmap_mode,                "NDDxp_XMAP_MODE"},
    {NDDxp_start_mode,               "NDDxp_START_MODE"},
    {NDDxp_xmap_runstate,            "NDDxp_XMAP_RUNSTATE"},
    {NDDxp_current_pixel,            "NDDxp_CURRENT_PIXEL"},
    {NDDxp_next_pixel,               "NDDxp_NEXT_PIXEL"},
    {NDDxp_buffer_overrun,           "NDDxp_BUFFER_OVERRUN"},
    {NDDxp_mbytes_received,          "NDDxp_MBYTES_RECEIVED"},
    {NDDxp_read_speed,               "NDDxp_READ_SPEED"},

    {NDDxpReadDXPParams,             "NDDxpReadDXPParams"},
    {NDDxpPresetNumPixels,           "NDDxpPresetNumPixels"},
    {NDDxpPixelCounter,              "NDDxpPixelCounter"},
    {NDDxpBufferCounter,             "NDDxpBufferCounter"},
    {NDDxpPollTime,                  "NDDxpPollTime"},
    {NDDxpArrayCallbacks,            "NDDxpArrayCallbacks"},
    {NDDxpDataType,                  "NDDxpDataType"},
    {NDDxpArrayData,                 "NDDxpArrayData"},
    {NDDxpArraySize,                 "NDDxpArraySize"},
    {NDDxpNumPixelsInBuffer,         "NDDxpNumPixelsInBuffer"},
    {NDDxpPresetCountMode,           "NDDxpPresetCountMode"},
    {NDDxpTraceMode,                 "NDDxpTraceMode"},
    {NDDxpTraceTime,                 "NDDxpTraceTime"},
    {NDDxpTrace,                     "NDDxpTrace"},
    {NDDxpBaselineHistogram,         "NDDxpBaselineHistogram"},
    {NDDxpMaxEnergy,                 "NDDxpMaxEnergy"},
    {NDDxpPollActive,                "NDDxpPollActive"},
    {NDDxpForceRead,                 "NDDxpForceRead"},

    {NDDxp_stat_realtime,            "NDDxp_STAT_REALTIME"},
    {NDDxp_stat_triggerlivetime,     "NDDxp_STAT_TRIGLTIME"},
    {NDDxp_stat_energylivetime,      "NDDxp_STAT_ELTIME"},
    {NDDxp_stat_triggers,            "NDDxp_STAT_TRIGGERS"},
    {NDDxp_stat_events,              "NDDxp_STAT_EVENTS"},
    {NDDxp_stat_inputcountrate,      "NDDxp_STAT_INCNTRATE"},
    {NDDxp_stat_outputcountrate,     "NDDxp_STAT_OUTCNTRATE"},

    {NDDxp_xia_peaking_time,         "peaking_time"},
    {NDDxp_xia_dynamic_range,        "dynamic_range"},
    {NDDxp_xia_trigger_threshold,    "trigger_threshold"},
    {NDDxp_xia_baseline_threshold,   "baseline_threshold"},
    {NDDxp_xia_energy_threshold,     "energy_threshold"},
    {NDDxp_xia_calibration_energy,   "calibration_energy"},
    {NDDxp_xia_adc_percent_rule,     "adc_percent_rule"},
    {NDDxp_xia_mca_bin_width,        "mca_bin_width"},
    {NDDxp_xia_preamp_gain,          "preamp_gain"},
    {NDDxp_xia_number_mca_channels,  "number_mca_channels"},
    {NDDxp_xia_detector_polarity,    "detector_polarity"},
    {NDDxp_xia_reset_delay,          "reset_delay"},
    {NDDxp_xia_decay_time,           "decay_time"},
    {NDDxp_xia_gap_time,             "gap_time"},
    {NDDxp_xia_trigger_peaking_time, "trigger_peaking_time"},
    {NDDxp_xia_trigger_gap_time,     "trigger_gap_time"},
    {NDDxp_xia_baseline_average,     "baseline_average"},
    {NDDxp_xia_baseline_cut,         "baseline_cut"},
    {NDDxp_xia_enable_baseline_cut,  "enable_baseline_cut"},
    {NDDxp_xia_maxwidth,             "maxwidth"}
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

    NDDxpDevice_t deviceType;
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

    this->deviceType = (NDDxpDevice_t) this->getModuleType();
    this->nChannels = nChannels;
    switch (this->deviceType)
    {
    case NDDxp_xMAP:
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
    status |= setIntegerParam(NDDxp_start_mode, 0);
    status |= setIntegerParam(NDDxp_xmap_mode, 0);
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
    if (this->deviceType == NDDxp_xMAP)
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
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%s]: function=%d value=%d addr=%d channel=%d\n",
                driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    switch(function)
    {
    case NDDxp_xmap_mode:
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s change mode to %d\n", driverName, functionName, value);
        status = this->changeMode(pasynUser, value);
        break;

    case NDDxp_start_mode:
        if (value == 1 || value == 0) {status = asynSuccess;}
        else {status = asynError;}
        break;

    case NDDxp_next_pixel:
        if (this->deviceType == NDDxp_xMAP) chStep = XMAP_NCHANS_MODULE;
        for (firstCh = 0; firstCh < this->nChannels; firstCh += chStep)
        {
            CALLHANDEL( xiaBoardOperation(firstCh, "mapping_pixel_next", (void*)&ignored), "mapping_pixel_next" )
        }
        setIntegerParam(addr, function, 0);
        break;

    case mcaErase:
        setIntegerParam(addr, NDDxp_start_mode, 0);
        getIntegerParam(addr, mcaNumChannels, &numChans);
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (acquiring) {
            xiaStopRun(channel);
            setIntegerParam(addr, NDDxp_start_mode, 1);
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
        getIntegerParam(NDDxp_xmap_mode, &mode);
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s mcaReadStatus [%d] mode=%d\n", driverName, functionName, function, mode);
        status = this->getAcquisitionStatus(pasynUser, addr);
        if (mode == NDDxp_normal) {
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

    case NDDxp_xia_detector_polarity:
    case NDDxp_xia_enable_baseline_cut:
    case NDDxp_xia_baseline_average:
        this->setDxpParam(pasynUser, addr, function, (double)value);
        break;

    /* These are no-ops for DXP */
    case mcaChannelAdvanceInternal:
    case mcaChannelAdvanceExternal:
    case mcaModePHA:
    case mcaModeMCS:
    case mcaModeList:
    case mcaSequence:
    case mcaPrescale:
    case mcaPresetSweeps:
    case mcaPresetLowChannel:
    case mcaPresetHighChannel:
        break;

    default:
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s::%s Function not implemented:%d; val=%d\n",
                    driverName, functionName, function, value);
        // Temporarily don't set error flag for unknown parameter until we actually implement all of them
        //status = asynError;
        break;
    }

    /* Call the callback for the specific address .. and address ... weird? */
    if (status != asynError) callParamCallbacks(addr, addr);
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
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%s]: function=%d value=%f addr=%d channel=%d\n",
                driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setDoubleParam(addr, function, value);

    switch(function)
    {
        case mcaPresetRealTime:
        case mcaPresetLiveTime:
            this->setPresets(pasynUser, addr);
            break;
        case NDDxp_xia_peaking_time:
        case NDDxp_xia_dynamic_range:
        case NDDxp_xia_trigger_threshold:
        case NDDxp_xia_baseline_threshold:
        case NDDxp_xia_energy_threshold:
        case NDDxp_xia_calibration_energy:
        case NDDxp_xia_adc_percent_rule:
        case NDDxp_xia_preamp_gain:
        case NDDxp_xia_detector_polarity:
        case NDDxp_xia_reset_delay:
        case NDDxp_xia_gap_time:
        case NDDxp_xia_trigger_peaking_time:
        case NDDxp_xia_trigger_gap_time:
        case NDDxp_xia_baseline_cut:
        case NDDxp_xia_maxwidth:
        case NDDxpMaxEnergy:
            this->setDxpParam(pasynUser, addr, function, value);
            break;
        /* These are no-ops for DXP */
        case mcaDwellTime:
            break;
        case NDDxpTraceTime:
            /* Nothing to do */
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s::%s Function not implemented: [%d]; val=%.5f\n",
                        driverName, functionName, function, value);
            // Temporarily don't set error flag for unknown parameter until we actually implement all of them
            //status = asynError;
            break;
    }

    /* Call the callback for the specific address .. and address ... weird? */
    if (status != asynError) callParamCallbacks(addr, addr);

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

    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s addr=%d channel=%d function=%d\n",
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
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
                    driverName, functionName, channel, nBins, acquiring);
            *nIn = nBins;
            getIntegerParam(NDDxp_xmap_mode, &mode);

            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s mode=%d acquiring=%d\n",
                    driverName, functionName, mode, acquiring);
            if (acquiring)
            {
                if (mode == NDDxp_normal)
                {
                    /* While acquiring we'll force reading the data from the HW */
                    this->getMcaData(pasynUser, addr);
                } else if (mode == NDDxp_mapping)
                {
                    /* TODO: need a function call here to parse the latest received
                     * data and post the result here... */
                }
            }

            /* Not sure if we should do this when in mapping mode but we need it in MCA mode... */
            memcpy(value, pMcaRaw[addr], nBins * sizeof(epicsUInt32));
            break;

        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s::%s Function not implemented: [%d]\n",
                    driverName, functionName, function);
            // Temporarily don't set error flag for unknown parameter until we actually implement all of them
            //status = asynError;
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
    case NDDxp_xMAP:
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
    if (this->deviceType != NDDxp_xMAP)
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

    if (this->deviceType != NDDxp_xMAP) return(asynSuccess);

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
        if (this->deviceType == NDDxp_xMAP) {
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
        if (this->deviceType == NDDxp_xMAP) {
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
        if (this->deviceType == NDDxp_xMAP) {
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
        case NDDxp_xia_peaking_time:
            xiastatus = xiaSetAcquisitionValues(channel, "peaking_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting peaking_time");
            break;
        case NDDxp_xia_dynamic_range:
            /* dynamic_range is only supported on the xMAP */
            if (this->deviceType == NDDxp_xMAP) {
                /* Convert from eV to keV */
                dvalue = value * 1000.;
                xiastatus = xiaSetAcquisitionValues(channel, "dynamic_range", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting dynamic_range");
            }
            break;
        case NDDxp_xia_trigger_threshold:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_threshold", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_threshold");
            break;
        case NDDxp_xia_baseline_threshold:
             dvalue = value * 1000.;    /* Convert to eV */
             xiastatus = xiaSetAcquisitionValues(channel, "baseline_threshold", &dvalue);
             status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_threshold");
             break;
        case NDDxp_xia_energy_threshold:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "energy_threshold", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting energy_threshold");
            break;
        case NDDxp_xia_calibration_energy:
            /* Convert from keV to eV */
            dvalue = value * 1000.;
            xiastatus = xiaSetAcquisitionValues(channel, "calibration_energy", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting calibration_energy");
            break;
        case NDDxp_xia_adc_percent_rule:
            xiastatus = xiaSetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting adc_percent_rule");
            break;
        case NDDxp_xia_preamp_gain:
            xiastatus = xiaSetAcquisitionValues(channel, "preamp_gain", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting preamp_gain");
            break;
        case NDDxp_xia_detector_polarity:
            xiastatus = xiaSetAcquisitionValues(channel, "detector_polarity", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting detector_polarity");
            break;
        case NDDxp_xia_reset_delay:
            xiastatus = xiaSetAcquisitionValues(channel, "reset_delay", &dvalue);
            break;
        case NDDxp_xia_gap_time:
            if (this->deviceType == NDDxp_xMAP) {
                /* On the xMAP the parameter that can be written is minimum_gap_time */
                xiastatus = xiaSetAcquisitionValues(channel, "minimum_gap_time", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "minimum_gap_time");
            } else {
               /* On the Saturn and DXP2X it is gap_time */
                xiastatus = xiaSetAcquisitionValues(channel, "gap_time", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting gap_time");
            }
            break;
        case NDDxp_xia_trigger_peaking_time:
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_peaking_time");
            break;
        case NDDxp_xia_trigger_gap_time:
            xiastatus = xiaSetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting trigger_gap_time");
            break;
        case NDDxp_xia_baseline_average:
            if (this->deviceType == NDDxp_xMAP) {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_average", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_average");
            } else {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_filter_length");
            }
            break;
        case NDDxp_xia_maxwidth:
            xiastatus = xiaSetAcquisitionValues(channel, "maxwidth", &dvalue);
            status = this->xia_checkError(pasynUser, xiastatus, "setting maxwidth");
            break;
        case NDDxp_xia_baseline_cut:
            /* The xMAP does not support this yet */
            if (this->deviceType != NDDxp_xMAP) {
                xiastatus = xiaSetAcquisitionValues(channel, "baseline_cut", &dvalue);
                status = this->xia_checkError(pasynUser, xiastatus, "setting baseline_cut");
                break;
            }
        case NDDxp_xia_enable_baseline_cut:
            /* The xMAP does not support this yet */
            if (this->deviceType != NDDxp_xMAP) {
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

    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s new number of bins: %d\n", driverName, functionName, value);

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
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "xiaSetAcquisitinValues ch=%d nbins=%.1f\n", i, dblNbins);
        xiastatus = xiaSetAcquisitionValues(i, "number_mca_channels", (void*)&dblNbins);
        status = this->xia_checkError(pasynUser, xiastatus, "number_mca_channels");
        if (status == asynError)
            {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s [%s] can not set nbins=%u (%.3f) ch=%u\n",
                        driverName, functionName, this->portName, *rbValue, dblNbins, i);
            return status;
            }
        setIntegerParam(i, mcaNumChannels, *rbValue);
        callParamCallbacks(i,i);
    }

    /* If in mapping mode we need to modify the Y size as well in order to fit in the 2MB buffer!
     * We also need to read out the new lenght of the mapping buffer because that can possibly change... */
    getIntegerParam(NDDxp_xmap_mode, &mode);
    if (mode == NDDxp_mapping)
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
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%d] Got num_map_pixels_per_buffer = %d\n", driverName, functionName, i, maxNumPixelsInBuffer);

            bufLen = 0;
            xiastatus = xiaGetRunData(i, "buffer_len", (void*)&bufLen);
            status = this->xia_checkError(pasynUser, xiastatus, "GET buffer_len");
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%d] Got buffer_len = %u\n", driverName, functionName, i, bufLen);
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

    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s Changing mode to %d\n",driverName, functionName, mode);

    if (mode < 0 || mode > 1) return asynError;
    getIntegerParam(mcaAcquiring, &acquiring);
    if (acquiring) return asynError;

    dMode = (double)mode;
    CALLHANDEL( xiaSetAcquisitionValues(DXP_ALL, "mapping_mode", (void*)&dMode), "mapping_mode" )
    if (status == asynError) return status;

    switch(mode)
    {
    case NDDxp_normal:
        getIntegerParam(mcaNumChannels, (int*)&bufLen);
        setIntegerParam(NDDxpDataType, NDUInt32);
        dummy = 0.0;
        for (i=0; i < this->nChannels; i++)
        {
            /* Clear the normal mapping mode status settings */
            setIntegerParam(i, NDDxp_stat_events, 0);
            setDoubleParam(i, NDDxp_stat_inputcountrate, dummy);
            setDoubleParam(i, NDDxp_stat_outputcountrate, dummy);

            /* Set the new ArraySize down to just one buffer length */
            setIntegerParam(i, NDDxpArraySize, (int)bufLen);
            callParamCallbacks(i,i);
        }
        break;
    case NDDxp_mapping:
        int nPixelsInBuffer = 0;
        setIntegerParam(NDDxpDataType, NDUInt16);
        if (this->deviceType == NDDxp_xMAP) chStep = XMAP_NCHANS_MODULE;
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
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%d] Got num_map_pixels_per_buffer = %.1f\n", driverName, functionName, firstCh, dTmp);
            nPixelsInBuffer = (int)dTmp;

            bufLen = 0;
            xiastatus = xiaGetRunData(firstCh, "buffer_len", (void*)&bufLen);
            status = this->xia_checkError(pasynUser, xiastatus, "GET buffer_len");
            asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s [%d] Got buffer_len = %u\n", driverName, functionName, firstCh, bufLen);
            setIntegerParam(firstCh, NDDxpArraySize, (int)bufLen);
            callParamCallbacks(firstCh, firstCh);

            /* Clear the normal mapping mode status settings */
            dummy = 0.0;
            for (i=0; i < XMAP_NCHANS_MODULE; i++)
            {
                setIntegerParam(firstCh+i, NDDxp_stat_triggers, 0);
                setDoubleParam(firstCh+i, NDDxp_stat_realtime, dummy);
                setDoubleParam(firstCh+i, NDDxp_stat_triggerlivetime, dummy);
                setDoubleParam(firstCh+i, NDDxp_stat_energylivetime, dummy);
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
    //asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s addr=%d channel=%d\n", driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        if (this->deviceType == NDDxp_xMAP) chStep = XMAP_NCHANS_MODULE;
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
    double dvalue, triggerLiveTime=0, energyLiveTime=0, realTime=0, icr=0, ocr=0, events=0, triggers=0;
    int ivalue;
    int acquiring=0;
    int channel=addr;
    int resume;
    int i;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == this->nChannels) channel = DXP_ALL;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s addr=%d channel=%d\n", driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s start DXP_ALL\n", driverName, functionName);
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatistics(pasynUser, i);
            getDoubleParam(i, mcaElapsedLiveTime, &dvalue);
            energyLiveTime = MAX(energyLiveTime, dvalue);
            getDoubleParam(i, NDDxp_stat_triggerlivetime, &dvalue);
            triggerLiveTime = MAX(triggerLiveTime, dvalue);
            getDoubleParam(i, mcaElapsedRealTime, &realTime);
            realTime = MAX(realTime, dvalue);
            getDoubleParam(i, NDDxp_stat_events, &dvalue);
            events = MAX(events, dvalue);
            getDoubleParam(i, NDDxp_stat_triggers, &dvalue);
            triggers = MAX(triggers, dvalue);
            getDoubleParam(i, NDDxp_stat_inputcountrate, &dvalue);
            icr = MAX(icr, dvalue);
            getDoubleParam(i, NDDxp_stat_outputcountrate, &dvalue);
            ocr = MAX(ocr, dvalue);
            getIntegerParam(i, mcaAcquiring, &ivalue);
            acquiring = MAX(acquiring, ivalue);
        }
        setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
        setDoubleParam(addr, NDDxp_stat_triggerlivetime, triggerLiveTime);
        setDoubleParam(addr, mcaElapsedRealTime, realTime);
        setDoubleParam(addr, NDDxp_stat_events, events);
        setDoubleParam(addr, NDDxp_stat_triggers, triggers);
        setDoubleParam(addr, NDDxp_stat_inputcountrate, icr);
        setDoubleParam(addr, NDDxp_stat_outputcountrate, ocr);
        setIntegerParam(addr, mcaAcquiring, acquiring);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s end DXP_ALL\n", driverName, functionName);
    } else {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s start channel %d\n", driverName, functionName, addr);
        getIntegerParam(addr, NDDxp_start_mode, &resume);
        //if (this->pChannel[addr].erased) {
        if (resume == 0) {
            setDoubleParam(addr, mcaElapsedLiveTime, 0);
            setDoubleParam(addr, mcaElapsedRealTime, 0);
            setDoubleParam(addr, NDDxp_stat_events, 0);
            setDoubleParam(addr, NDDxp_stat_inputcountrate, 0);
            setDoubleParam(addr, NDDxp_stat_outputcountrate, 0);
            setDoubleParam(addr, NDDxp_stat_triggers, 0);
            setDoubleParam(addr, NDDxp_stat_triggerlivetime, 0);
        } else {
            xiaGetRunData(channel, "runtime", &realTime);
            setDoubleParam(addr, mcaElapsedRealTime, realTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d runtime=%f\n", 
                driverName, functionName, addr, realTime);
            if (this->deviceType == NDDxp_xMAP) {
                xiaGetRunData(channel, "trigger_livetime", &triggerLiveTime);
            } else {
                xiaGetRunData(channel, "livetime", &triggerLiveTime);
            }
            setDoubleParam(addr, NDDxp_stat_triggerlivetime, triggerLiveTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d trigger livetime=%f\n", 
                driverName, functionName, addr, triggerLiveTime);

            xiaGetRunData(channel, "events_in_run", &events);
            setIntegerParam(addr, NDDxp_stat_events, events);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d events=%d\n", 
                driverName, functionName, addr, events);

            if (this->deviceType == NDDxp_xMAP) {
                // We cannot read triggers on the xMAP unless we read module_statistics which is too
                // complex for now.  Instead read ICR and compute triggers
                xiaGetRunData(channel, "input_count_rate", &icr);
                triggers = (int)(ocr * triggerLiveTime);
            } else {
               xiaGetRunData(channel, "triggers", &triggers);
            }
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d triggers=%d\n", 
                driverName, functionName, addr, triggers);
            setIntegerParam(addr, NDDxp_stat_triggers, triggers);
            
            /* Note - we could read ICR and OCR, but these can be computed from 
             * the above values, and it takes 10msec each to read them, so don't */
            icr = triggers/triggerLiveTime;
            ocr = events/realTime; 

            //xiaGetRunData(channel, "input_count_rate", &icr);
            setDoubleParam(addr, NDDxp_stat_inputcountrate, icr);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d ICR=%f\n", 
                driverName, functionName, addr, icr);
            //xiaGetRunData(channel, "output_count_rate", &ocr);
            setDoubleParam(addr, NDDxp_stat_outputcountrate, ocr);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d OCR=%f\n", 
                driverName, functionName, addr, ocr);

            if (this->deviceType == NDDxp_xMAP) {
                // On the xMAP we can read the energy live time.  We need to determine if this is more
                // accurate than the computed value we use on the Saturn below
                xiaGetRunData(channel, "livetime", &energyLiveTime);
            } else {
                // The Saturn and DXP4C-2X don't have an energy livetime readout.
                if (icr < 1.0) icr = 1.0;
                energyLiveTime = realTime * ocr/icr;
            }
            setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s  channel %d energy livetime=%f\n", 
                driverName, functionName, addr, energyLiveTime);

            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s end channel %d\n", driverName, functionName, addr);
        }
    }
/*    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s::%s addr=%d channel=%d: acquiring=%d\n",
              driverName, functionName, addr, channel, acquiring);*/
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
        setDoubleParam(channel, NDDxp_xia_energy_threshold, dvalue);
        xiaGetAcquisitionValues(channel, "peaking_time", &dvalue);
        setDoubleParam(channel, NDDxp_xia_peaking_time, dvalue);
        xiaGetAcquisitionValues(channel, "gap_time", &dvalue);
        setDoubleParam(channel, NDDxp_xia_gap_time, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_threshold", &dvalue);
        /* Convert trigger threshold from eV to keV */
        dvalue = dvalue / 1000.;
        setDoubleParam(channel, NDDxp_xia_trigger_threshold, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_peaking_time", &dvalue);
        setDoubleParam(channel, NDDxp_xia_trigger_peaking_time, dvalue);
        xiaGetAcquisitionValues(channel, "trigger_gap_time", &dvalue);
        setDoubleParam(channel, NDDxp_xia_trigger_gap_time, dvalue);
        xiaGetAcquisitionValues(channel, "preamp_gain", &dvalue);
        setDoubleParam(channel, NDDxp_xia_preamp_gain, dvalue);
        if (this->deviceType == NDDxp_xMAP) {
           xiaGetAcquisitionValues(channel, "baseline_average", &dvalue);
        } else {
           xiaGetAcquisitionValues(channel, "baseline_filter_length", &dvalue);
        }
        setIntegerParam(channel, NDDxp_xia_baseline_average, (int)dvalue);
        xiaGetAcquisitionValues(channel, "baseline_threshold", &dvalue);
        dvalue/= 1000.;  /* Convert to keV */
        setDoubleParam(channel, NDDxp_xia_baseline_threshold, dvalue);
        xiaGetAcquisitionValues(channel, "maxwidth", &dvalue);
        setDoubleParam(channel, NDDxp_xia_maxwidth, dvalue);
        if (this->deviceType != NDDxp_xMAP) {
           xiaGetAcquisitionValues(channel, "baseline_cut", &dvalue);
            setDoubleParam(channel, NDDxp_xia_baseline_cut, dvalue);
           xiaGetAcquisitionValues(channel, "enable_baseline_cut", &dvalue);
            setIntegerParam(channel, NDDxp_xia_enable_baseline_cut, (int)dvalue);
        }
        xiaGetAcquisitionValues(channel, "adc_percent_rule", &dvalue);
        setDoubleParam(channel, NDDxp_xia_adc_percent_rule, dvalue);
        if (this->deviceType == NDDxp_xMAP) {
            xiaGetAcquisitionValues(channel, "dynamic_range", &dvalue);
        } else dvalue = 0.;
        setDoubleParam(channel, NDDxp_xia_dynamic_range, dvalue);
        xiaGetAcquisitionValues(channel, "calibration_energy", &dvalue);
        setDoubleParam(channel, NDDxp_xia_calibration_energy, dvalue);
        xiaGetAcquisitionValues(channel, "mca_bin_width", &mcaBinWidth);
        setDoubleParam(channel, NDDxp_xia_mca_bin_width, mcaBinWidth);
        xiaGetAcquisitionValues(channel, "number_mca_channels", &numMcaChannels);
        setIntegerParam(channel, mcaNumChannels, (int)numMcaChannels);
        /* Compute emax from mcaBinWidth and mcaNumChannels, convert eV to keV */
        emax = numMcaChannels * mcaBinWidth / 1000.;
        setDoubleParam(channel, NDDxpMaxEnergy, emax);
        xiaGetAcquisitionValues(channel, "detector_polarity", &dvalue);
        setIntegerParam(channel, NDDxp_xia_detector_polarity, (int)dvalue);
        xiaGetAcquisitionValues(channel, "decay_time", &dvalue);
        setDoubleParam(channel, NDDxp_xia_decay_time, dvalue);
        xiaGetAcquisitionValues(channel, "reset_delay", &dvalue);
        setDoubleParam(channel, NDDxp_xia_reset_delay, dvalue);

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
    if ((this->deviceType == NDDxp_xMAP) && (channel==DXP_ALL)) {
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
                "%s::%s Got MCA spectrum channel:%d ptr:%p",
                driverName, functionName, channel, pMcaRaw[addr]);

        if (arrayCallbacks)
        {
            /* Allocate a buffer for callback
             * -and just telling it where to find the data in the pMcaRaw buffer... */
            this->pRaw = this->pNDArrayPool->alloc(1, &nChannels, dataType, 0, (void*)(pMcaRaw[addr]) );
            this->pRaw->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
            this->pRaw->uniqueId = spectrumCounter;
            this->unlock();
            doCallbacksGenericPointer(this->pRaw, NDDxpArrayData, addr);
            this->lock();
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
    if (this->deviceType == NDDxp_xMAP) chStep = XMAP_NCHANS_MODULE;

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
    paramStatus |= getDoubleParam(NDDxp_mbytes_received, &mbytesRead);
    MBbufSize = (double)((itemsInArray)*sizeof(epicsUInt16)) / (double)MEGABYTE;

    epicsTimeGetCurrent(&now);
    bufferCounter++;
    buf = this->currentBuf[channel];

    CALLHANDEL( xiaGetRunData(channel, "buffer_len", (void*)&bufLen) , "buffer_len")
    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s buffer length: %d Getting data...",
            driverName, functionName, bufLen);

    /* the buffer is full so read it out */
    CALLHANDEL( xiaGetRunData(channel, NDDxpBuffers[buf].buffer, (void*)data), "NDDxpBuffers[buf].buffer" )
    epicsTimeGetCurrent(&after);
    paramStatus |= getIntegerParam(NDDxpArrayCallbacks, &arrayCallbacks);

    readoutTime = epicsTimeDiffInSeconds(&after, &now);
    readoutBurstSpeed = MBbufSize / readoutTime;
    mbytesRead += MBbufSize;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
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
        this->unlock();
        doCallbacksGenericPointer(this->pRaw, NDDxpArrayData, channel);
        this->lock();
        this->pRaw->release();

        setDoubleParam(NDDxp_mbytes_received, mbytesRead);
        setDoubleParam(NDDxp_read_speed, readoutBurstSpeed);
        callParamCallbacks();
    }

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s Done reading! Ch=%d bufchar=%s\n",
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
        asynPrint(    pasynUser, ASYN_TRACE_FLOW, "%s:%s: (mca) drvInfo=%s, param=%d\n",
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
            asynPrint(    pasynUser, ASYN_TRACE_FLOW, "%s:%s: (NDDxp) drvInfo=%s, param=%d\n",
                        driverName, functionName, drvInfo, param);
        }
    }

    if (status != asynSuccess)
    {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "%s::%s ERROR did not find reason %s in either mcaCommands nor NDDxpParams table!\n",
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
    getIntegerParam(addr, NDDxp_start_mode, &resume);
    getIntegerParam(addr, mcaAcquiring, &acquiring);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s ch=%d acquiring=%d resume=%d\n",
            driverName, functionName, channel, acquiring, resume);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    /* make sure we use buffer A to start with */
    //for (firstCh=0; firstCh < this->nChannels; firstCh += XMAP_NCHANS_MODULE) this->currentBuf[firstCh] = 0;

    // do xiaStart command
    CALLHANDEL( xiaStartRun(channel, resume), "xiaStartRun()" )

    setIntegerParam(addr, NDDxp_start_mode, 1); /* reset the resume flag */
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

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "acquisition task started!\n");
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
                    "%s::%s [%s]: waiting for acquire to start\n", driverName, functionName, this->portName);

            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "Waiting for acquisition to start!\n");
            this->cmdStartEvent->wait();
            this->lock();
            asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s::%s [%s]: started! (mode=%d)\n", driverName, functionName, this->portName, mode);
        }
        epicsTimeGetCurrent(&start);

        /* In this loop we only read the acquisition status to minimise overhead.
         * If a transition from acquiring to done is detected then we read the statistics
         * and the data. */
        this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
        getIntegerParam(this->nChannels, mcaAcquiring, &acquiring);
        getIntegerParam(NDDxp_xmap_mode, &mode);
        /*asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s prevAcquiring=%d acquiring=%d mode=%d\n",
                driverName, functionName, prevAcquiring, acquiring, mode);*/
        if (mode == NDDxp_normal && (!acquiring))
        {
            /* There must have just been a transition from acquiring to not acquiring */
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s Detected acquisition stop! Now reading statistics\n",
                    driverName, functionName);
            this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s Detected acquisition stop! Now reading data\n",
                    driverName, functionName);
            this->getMcaData(this->pasynUserSelf, DXP_ALL);
            //printf("acquisition task, detected stop, called getAcquisitionStatistics and getMcaData\n");
        } else if (mode == NDDxp_mapping)
        {
            this->pollMappingMode();
        }

        /* Do callbacks for all channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s Doing callbacks\n",
                driverName, functionName);
        for (i=0; i<=this->nChannels; i++) callParamCallbacks(i, i);
        paramStatus |= getDoubleParam(NDDxpPollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleeptime = pollTime - dtmp;
        if (sleeptime > 0.0)
        {
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s::%s Sleeping for %f seconds\n",
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

    if (this->deviceType == NDDxp_xMAP) chStep = XMAP_NCHANS_MODULE;
    /* Step through all the first channels on all the boards in the system if
     * the device is an XMAP, otherwise just step through each individual channel. */
    for (ch=0; ch < this->nChannels; ch+=chStep)
    {
        buf = this->currentBuf[ch];

        CALLHANDEL( xiaGetRunData(ch, "current_pixel", (void*)&currentPixel) , "current_pixel" )
        setIntegerParam(ch, NDDxp_current_pixel, (int)currentPixel);
        callParamCallbacks(ch, ch);
        CALLHANDEL( xiaGetRunData(ch, NDDxpBuffers[buf].bufFull, (void*)&isfull), "NDDxpBuffers[buf].bufFull" )
        asynPrint(pasynUser, ASYN_TRACE_FLOW, "%s::%s %s isfull=%d\n",
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

    asynPrint( pasynUser, ASYN_TRACE_ERROR, "### NDDxp: XIA ERROR: %d (%s)\n", xiastatus, xiacmd );
    return asynError;
}

void NDDxp::shutdown()
{
    int status;
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, "%s: shutting down...\n", driverName);
    this->polling = 0;
    epicsThreadSleep(1.0);
    status = xiaExit();
    if (status == XIA_SUCCESS)
    {
        printf("### XIA XMAP closed down successfully. You can now exit the iocshell ###\n");
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
    if (strcmp(module_type, "xmap") == 0) return(NDDxp_xMAP);
    if (strcmp(module_type, "dxpx10p") == 0) return(NDDxp_Saturn);
    if (strcmp(module_type, "dxp4c2x") == 0) return(NDDxp_4C2X);
    return(-1);
}

