/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2016 XIA LLC
 * All rights reserved
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the
 *     above copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *   * Neither the name of XIA LLC
 *     nor the names of its contributors may be used to endorse
 *     or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <math.h>

#include "xerxes_errors.h"

#include "xia_common.h"
#include "xia_assert.h"
#include "xia_system.h"
#include "xia_psl.h"
#include "xia_handel.h"

#include "handel_constants.h"
#include "handel_errors.h"

#include "md_generic.h"

#include "psl_common.h"
#include "psldef.h"
#include "psl_stj.h"

#include "fdd.h"

#include "stj.h"


PSL_EXPORT int PSL_API stj_PSLInit(PSLFuncs *funcs);

PSL_STATIC int psl__UpdateRawParamAcqValue(int detChan, char *name,
                                           void *value, XiaDefaults *defs);
PSL_STATIC int psl__CalculateGain(int modChan, XiaDefaults *defs, Module *m,
                                  Detector *det, parameter_t *BANKnADCGAIN);
PSL_STATIC int psl__UpdateGain(int detChan, int modChan, XiaDefaults *defs,
                               Module *m, Detector *det);
PSL_STATIC double psl__GetClockTick(void);
PSL_STATIC int psl__GetFiPPIName(int modChan, double pt, FirmwareSet *fs,
                                 char *detType, char *name, char *rawName);
PSL_STATIC int psl__GetDSPName(int modChan, double pt, FirmwareSet *fs,
                               char *detType, char *name, char *rawName);
PSL_STATIC int psl__UpdateFilterParams(int detChan, int modChan, double pt,
                                       XiaDefaults *defs, FirmwareSet *fs,
                                       Module *m, Detector *det);
PSL_STATIC int psl__UpdateTrigFilterParams(int detChan, XiaDefaults *defs);
PSL_STATIC int psl__DoTrace(int detChan, short type, double *info);
PSL_STATIC int psl__GetBufferFull(int detChan, char buf, boolean_t *is_full);
PSL_STATIC int psl__IsMapping(int detChan, unsigned short allowed,
                              boolean_t *isMapping);
PSL_STATIC int psl__GetBuffer(int detChan, char buf, unsigned long *data,
                              XiaDefaults *defs, Module *m);
PSL_STATIC int psl__SetRegisterBit(int detChan, char *reg, int bit,
                                   boolean_t overwrite);
PSL_STATIC int psl__ClearRegisterBit(int detChan, char *reg, int bit);
PSL_STATIC int psl__CheckRegisterBit(int detChan, char *reg, int bit, boolean_t *isSet);
PSL_STATIC int psl__ClearBuffer(int detChan, char buf, boolean_t waitForEmpty);
PSL_STATIC int psl__UpdateParams(int detChan, unsigned short type, int modChan,
                                 char *name, void *value, char *detType,
                                 XiaDefaults *defs, Module *m,
                                 Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__GetStatisticsBlock(int detChan, unsigned long *stats);
PSL_STATIC int psl__ExtractELivetime(int modChan, unsigned long *stats,
                                     double *eLT);
PSL_STATIC int psl__ExtractRealtime(int modChan, unsigned long *stats,
                                    double *rt);
PSL_STATIC int psl__ExtractTLivetime(int modChan, unsigned long *stats,
                                     double *tLT);
PSL_STATIC int psl__ExtractTriggers(int modChan, unsigned long *stats,
                                    double *trigs);
PSL_STATIC int psl__ExtractEvents(int modChan, unsigned long *stats,
                                  double *evts);
PSL_STATIC int psl__ExtractOverflows(int modChan, unsigned long *stats,
                                     double *overs);
PSL_STATIC int psl__ExtractUnderflows(int modChan, unsigned long *stats,
                                      double *unders);
PSL_STATIC int psl__SwitchFirmware(int detChan, double type, int modChan,
                                   double pt, FirmwareSet *fs, Module *m);
PSL_STATIC int psl__WakeDSP(int detChan);
PSL_STATIC unsigned long psl__GetMCAPixelBlockSize(XiaDefaults *defs,
                                                   Module *m);
PSL_STATIC unsigned long psl__GetSCAPixelBlockSize(XiaDefaults *defs,
                                                   Module *m);
PSL_STATIC int psl__RevertDefault(char *name, double *value);
PSL_STATIC int psl__ClearRegisterBit(int detChan, char *reg, int bit);
PSL_STATIC int psl__SetInputNC(int detChan);
PSL_STATIC int psl__SetDigitalGain(int detChan, double digitalGain);

/* DSP Parameter Data Types */
PSL_STATIC int psl__GetParamValues(int detChan, void *value);

/* Firmware downloaders */
PSL_STATIC int psl__DownloadFiPPIA(int detChan, char *file, char *rawFile,
                                   Module *m);
PSL_STATIC int psl__DownloadFiPPIADSPNoWake(int detChan, char *file,
                                            char *rawFile, Module *m);
PSL_STATIC int psl__DownloadDSP(int detChan, char *file, char *rawFile,
                                Module *m);

/* Board Operations */
PSL_STATIC int pslApply(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int psl__SetBufferDone(int detChan, char *name, XiaDefaults *defs,
                                  void *value);
PSL_STATIC int psl__MapPixelNext(int detChan, char *name, XiaDefaults *defs,
                                 void *value);
PSL_STATIC int psl__GetMCR(int detChan, char *name, XiaDefaults *defs,
                           void *value);
PSL_STATIC int psl__GetMFR(int detChan, char *name, XiaDefaults *defs,
                           void *value);
PSL_STATIC int psl__GetCSR(int detChan, char *name, XiaDefaults *defs,
                           void *value);
PSL_STATIC int psl__GetCVR(int detChan, char *name, XiaDefaults *defs,
                           void *value);
PSL_STATIC int psl__GetSVR(int detChan, char *name, XiaDefaults *defs,
                           void *value);

/* Gain Operations */
PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det,
                                  int modChan, Module *m, XiaDefaults *defs,
                                  void *value);
PSL_STATIC int psl__DigitalGainCalibrate(int detChan,  Detector *det,
                                         int modChan, Module *m, XiaDefaults *defs,
                                         void *value);

/* Special Run data */
PSL_STATIC int pslGetADCTraceLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetADCTrace(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int psl__GetBiasScanTraceLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int psl__GetBiasScanTrace(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int psl__GetBiasScanNoise(int detChan, void *value, XiaDefaults *defs);

/* Special Runs */
PSL_STATIC int psl__AdjustOffsets(int detChan, void *value, XiaDefaults *defs, Detector *det);
PSL_STATIC int psl__BeginBiasScan(int detChan, void *value, XiaDefaults *defs, Detector *det);
PSL_STATIC int psl__EndBiasScan(int detChan, void *value, XiaDefaults *defs, Detector *det);
PSL_STATIC int psl__SetBiasDac(int detChan, void *value, XiaDefaults *defs, Detector *det);
PSL_STATIC int psl__ScaleDigitalGain(int detChan, void *value, XiaDefaults *defs, Detector *det);

/* Run data */
PSL_STATIC int pslGetMCALength(int detChan, void *value, XiaDefaults *defs,
                               Module *m);
PSL_STATIC int psl__GetMCA(int detChan, void *value, XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetRealtime(int detChan, void *value, XiaDefaults *defs,
                                Module *m);
PSL_STATIC int psl__GetTotalEvents(int detChan, void *value, XiaDefaults *defs,
                                   Module *m);
PSL_STATIC int psl__GetMCAEvents(int detChan, void *value, XiaDefaults *defs,
                                 Module *m);
PSL_STATIC int psl__GetTLivetime(int detChan, void *value, XiaDefaults *defs,
                                 Module *m);
PSL_STATIC int psl__GetICR(int detChan, void *value, XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetOCR(int detChan, void *value, XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetRunActive(int detChan, void *value, XiaDefaults *defs,
                                 Module *m);
PSL_STATIC int psl__GetBufferFullA(int detChan, void *value, XiaDefaults *defs,
                                   Module *m);
PSL_STATIC int psl__GetBufferFullB(int detChan, void *value, XiaDefaults *defs,
                                   Module *m);
PSL_STATIC int psl__GetBufferLen(int detChan, void *value, XiaDefaults *defs,
                                 Module *m);
PSL_STATIC int psl__GetBufferA(int detChan, void *value, XiaDefaults *defs,
                               Module *m);
PSL_STATIC int psl__GetBufferB(int detChan, void *value, XiaDefaults *defs,
                               Module *m);
PSL_STATIC int psl__GetCurrentPixel(int detChan, void *value, XiaDefaults *defs,
                                    Module *m);
PSL_STATIC int psl__GetBufferOverrun(int detChan, void *value,
                                     XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetELivetime(int detChan, void *value, XiaDefaults *defs,
                                 Module *m);
PSL_STATIC int psl__GetModuleStatistics(int detChan, void *value,
                                        XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetModuleStatistics2(int detChan, void *value,
                                         XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetTriggers(int detChan, void *value,
                                XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetUnderflows(int detChan, void *value,
                                  XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetOverflows(int detChan, void *value,
                                 XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetModuleMCA(int detChan, void *value,
                                 XiaDefaults * defs, Module *m);
PSL_STATIC int psl__GetListBufferLenA(int detChan, void *value,
                                      XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetListBufferLenB(int detChan, void *value,
                                      XiaDefaults *defs, Module *m);
PSL_STATIC int psl__GetListBufferLen(int detChan, char buf, unsigned long *len);


/* Acquisition Values */
PSL_STATIC int psl__SetTThresh(int detChan, int modChan, char *name, void *value, char *detType,
                               XiaDefaults *defs, Module *m, Detector *det,
                               FirmwareSet *fs);
PSL_STATIC int psl__SetEThresh(int detChan, int modChan, char *name, void *value, char *detType,
                               XiaDefaults *defs, Module *m, Detector *det,
                               FirmwareSet *fs);
PSL_STATIC int psl__SetCalibEV(int detChan, int modChan, char *name, void *value, char *detType,
                               XiaDefaults *defs, Module *m, Detector *det,
                               FirmwareSet *fs);
PSL_STATIC int psl__SetMCABinWidth(int detChan, int modChan, char *name, void *value, char *detType,
                                   XiaDefaults *defs, Module *m, Detector *det,
                                   FirmwareSet *fs);
PSL_STATIC int psl__SetDynamicRng(int detChan, int modChan, char *name, void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs);
PSL_STATIC int psl__SetPreampGain(int detChan, int modChan, char *name, void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs);
PSL_STATIC int psl__SynchPreampGain(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs);
PSL_STATIC int psl__SetNumMCAChans(int detChan, int modChan, char *name, void *value, char *detType,
                                   XiaDefaults *defs, Module *m, Detector *det,
                                   FirmwareSet *fs);
PSL_STATIC int psl__SetPolarity(int detChan, int modChan, char *name, void *value, char *detType,
                                XiaDefaults *defs, Module *m, Detector *det,
                                FirmwareSet *fs);
PSL_STATIC int psl__SynchPolarity(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs);
PSL_STATIC int psl__SetResetDelay(int detChan, int modChan, char *name, void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs);
PSL_STATIC int psl__SynchResetDelay(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs);
PSL_STATIC int psl__SetPeakingTime(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetGapTime(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__GetGapTime(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int psl__SetTrigPeakingTime(int detChan, int modChan, char *name, void *value,
                                       char *detType, XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetTrigGapTime(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPresetType(int detChan, int modChan, char *name, void *value,
                                  char *detType, XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPresetValue(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetMappingMode(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPeakSampleOffset(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPeakIntervalOffset(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetMinGapTime(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetMaxWidth(int detChan, int modChan, char *name,
                                void *value, char *detType,
                                XiaDefaults *defs, Module *m,
                                Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetDecayTime(int detChan, int modChan, char *name,
                                 void *value, char *detType,
                                 XiaDefaults *defs, Module *m, Detector *det,
                                 FirmwareSet *fs);
PSL_STATIC int psl__SynchDecayTime(int detChan, int det_chan, Module *m,
                                   Detector *det, XiaDefaults *defs);
PSL_STATIC int psl__SetPreampType(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs);
PSL_STATIC int psl__SynchPreampType(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs);
PSL_STATIC int psl__SetPeakMode(int detChan, int modChan, char *name,
                                void *value, char *detType,
                                XiaDefaults *defs, Module *m,
                                Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetTraceTriggerEnable(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetTraceTriggerType(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetTraceTriggerPosition(int detChan, int modChan, char *name,
                                            void *value, char *detType,
                                            XiaDefaults *defs, Module *m,
                                            Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasScanStartOffset(int detChan, int modChan, char *name,
                                           void *value, char *detType,
                                           XiaDefaults *defs, Module *m,
                                           Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasScanSteps(int detChan, int modChan, char *name,
                                     void *value, char *detType,
                                     XiaDefaults *defs, Module *m,
                                     Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasScanStepSize(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasDacZero(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__GetBiasDacZero(int detChan, void *value,
                                   XiaDefaults *defs);
PSL_STATIC int psl__SetBiasDacSetZero(int detChan, int modChan, char *name,
                                      void *value, char *detType,
                                      XiaDefaults *defs, Module *m,
                                      Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__GetBiasDacSetZero(int detChan, void *value,
                                      XiaDefaults *defs);
PSL_STATIC int psl__SetBiasScanWaitTime(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasSetDac(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBaseAvg(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetBiasDacEnable(int detChan, int modChan, char *name,
                                     void *value, char *detType,
                                     XiaDefaults *defs, Module *m,
                                     Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetListModeVariant(int detChan, int modChan, char *name,
                                       void *value, char *detType,
                                       XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetNumMapPixels(int detChan, int modChan, char *name,
                                    void *value, char *detType,
                                    XiaDefaults *defs, Module *m,
                                    Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetNumMapPtsBuffer(int detChan, int modChan, char *name,
                                       void *value, char *detType,
                                       XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__GetNumMapPtsBuffer(int detChan, void *value,
                                       XiaDefaults *defs);
PSL_STATIC int psl__SetSetPmtTriggerMode(int detChan, int modChan, char *name,
                                         void *value, char *detType,
                                         XiaDefaults *defs, Module *m,
                                         Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPmtDynodeThreshold(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPmtDynodeSumThreshold(int detChan, int modChan, char *name,
                                             void *value, char *detType,
                                             XiaDefaults *defs, Module *m,
                                             Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPmtMultiLen(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);
PSL_STATIC int psl__SetPmtMultiReq(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs);


/* These are the DSP parameter data types for pslGetParamData(). */
static ParamData_t PARAM_DATA[] =
{
    {   "values", psl__GetParamValues
    },
};

/* These are the allowed firmware types to download */
static FirmwareDownloader_t FIRMWARE[] =
{
    {   "fippi_a",             psl__DownloadFiPPIA
    },
    {"fippi_a_dsp_no_wake", psl__DownloadFiPPIADSPNoWake},
    {"dsp",                 psl__DownloadDSP},
};

/* These are the allowed trace types */
static SpecialRun traceTypes[] =
{
    {"adc_trace", 									NULL},
    {"adc_average", 								NULL},
    {"fast_filter", 								NULL},
    {"baseline_samples", 						NULL},
    {"baseline_average", 						NULL},
    {"subtracted_baseline_samples", NULL},
    {"scaled_baseline_samples", 		NULL},
    {"variance", 										NULL},
    {"raw_energy", 									NULL},
    {"subtracted_raw_energy", 			NULL},
    {"scaled_energy", 							NULL},
};

/* These are the allowed special runs */
static Stj_SpecialRun specialRun[] =
{
    {"adjust_offsets",        psl__AdjustOffsets},
    {"begin_bias_scan",       psl__BeginBiasScan},
    {"end_bias_scan",         psl__EndBiasScan},
    {"set_bias_dac",          psl__SetBiasDac},
    {"scale_digital_gain",    psl__ScaleDigitalGain}
};


/* These are the allowed special run data types */
static SpecialRunData specialRunData[] =
{
    {"adc_trace_length",        pslGetADCTraceLen},
    {"adc_trace",               pslGetADCTrace},
    {"bias_scan_trace_length",  psl__GetBiasScanTraceLen},
    {"bias_scan_trace",         psl__GetBiasScanTrace},
    {"bias_scan_noise_length",  psl__GetBiasScanTraceLen},
    {"bias_scan_noise",         psl__GetBiasScanNoise},
};

/* These are the allowed bord operations for this hardware */
static BoardOperation boardOps[] =
{
    {"apply",              pslApply},
    {"mapping_pixel_next", psl__MapPixelNext},
    {"buffer_done",        psl__SetBufferDone},
    {"get_mcr",            psl__GetMCR},
    {"get_mfr",            psl__GetMFR},
    {"get_csr",            psl__GetCSR},
    {"get_cvr",            psl__GetCVR},
    {"get_svr",            psl__GetSVR}
};

/* These are the allowed gain operations for this hardware */
static GainOperation gainOps[] =
{
    {"calibrate",          psl__GainCalibrate},
    {"scale_digital_gain", psl__DigitalGainCalibrate},
};

/* These are the allowed run data types */
static RunData runData[] =
{
    { "mca_length",           pslGetMCALength},
    { "mca",                  psl__GetMCA},
    { "runtime",              psl__GetRealtime},
    { "realtime",             psl__GetRealtime},
    { "events_in_run",        psl__GetTotalEvents},
    { "trigger_livetime",     psl__GetTLivetime},
    { "input_count_rate",     psl__GetICR},
    { "output_count_rate",    psl__GetOCR},
    { "run_active",           psl__GetRunActive},
    { "buffer_full_a",        psl__GetBufferFullA},
    { "buffer_full_b",        psl__GetBufferFullB},
    { "buffer_len",           psl__GetBufferLen},
    { "buffer_a",             psl__GetBufferA},
    { "buffer_b",             psl__GetBufferB},
    { "current_pixel",        psl__GetCurrentPixel},
    { "buffer_overrun",       psl__GetBufferOverrun},
    { "livetime",             psl__GetELivetime},
    { "module_statistics",    psl__GetModuleStatistics},
    { "module_mca",           psl__GetModuleMCA},
    { "energy_livetime",      psl__GetELivetime},
    { "module_statistics_2",  psl__GetModuleStatistics2},
    { "triggers",             psl__GetTriggers},
    { "underflows",           psl__GetUnderflows},
    { "overflows",            psl__GetOverflows},
    { "list_buffer_len_a",    psl__GetListBufferLenA },
    { "list_buffer_len_b",    psl__GetListBufferLenB },
    { "mca_events",           psl__GetMCAEvents },
    { "total_output_events",  psl__GetTotalEvents }
};

/* Acquisition values */
static AcquisitionValue_t ACQ_VALUES[] =
{
    {   "peaking_time",         TRUE_, FALSE_, STJ_UPDATE_NEVER, 1.0,
        psl__SetPeakingTime,     NULL, NULL
    },

    {   "dynamic_range",        TRUE_, FALSE_, STJ_UPDATE_NEVER, 5000.0,
        psl__SetDynamicRng,      NULL,  NULL
    },

    {   "peak_sample_offset",    FALSE_, FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetPeakSampleOffset, NULL, NULL
    },

    {   "peak_interval_offset",  FALSE_, FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetPeakIntervalOffset, NULL, NULL
    },

    {   "minimum_gap_time",      TRUE_, FALSE_, STJ_UPDATE_NEVER, 0.060,
        psl__SetMinGapTime,      NULL, NULL
    },

    {   "preamp_gain",          TRUE_, TRUE_, STJ_UPDATE_NEVER, 100.0,
        psl__SetPreampGain,      NULL,  psl__SynchPreampGain
    },

    {   "number_mca_channels",  TRUE_, FALSE_, STJ_UPDATE_NEVER, 2048.0,
        psl__SetNumMCAChans,     NULL, NULL
    },

    {   "calibration_energy",   TRUE_, FALSE_, STJ_UPDATE_NEVER, 5900.0,
        psl__SetCalibEV,         NULL,     NULL
    },

    {   "preset_type",          TRUE_,  FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetPresetType,      NULL,  NULL
    },

    {   "preset_value",         TRUE_,  FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetPresetValue,     NULL, NULL
    },

    /* Due to the use of STRNEQ() in pslSetAcquisitionValues(),
    * num_map_pixels_per_buffer must be listed before num_map_pixels.
    */
    {   "num_map_pixels_per_buffer", TRUE_, FALSE_, STJ_UPDATE_MAPPING, 0.0,
        psl__SetNumMapPtsBuffer, psl__GetNumMapPtsBuffer, NULL
    },

    {   "num_map_pixels",       TRUE_, FALSE_, STJ_UPDATE_MAPPING, 0.0,
        psl__SetNumMapPixels,    NULL, NULL
    },

    {   "preamp_type",           TRUE_, TRUE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetPreampType,      NULL, psl__SynchPreampType
    },

    {   "decay_time",            TRUE_, TRUE_, STJ_UPDATE_NEVER, 10.0,
        psl__SetDecayTime,       NULL, psl__SynchDecayTime
    },

    {   "mca_bin_width",        TRUE_, FALSE_, STJ_UPDATE_NEVER, 1.0,
        psl__SetMCABinWidth,     NULL, NULL
    },

    {   "detector_polarity",    TRUE_, TRUE_, STJ_UPDATE_NEVER, 1.0,
        psl__SetPolarity,        NULL,    psl__SynchPolarity
    },

    {   "reset_delay",          TRUE_, TRUE_, STJ_UPDATE_NEVER, 10.0,
        psl__SetResetDelay,      NULL,  psl__SynchResetDelay
    },

    {   "gap_time",             TRUE_, FALSE_, STJ_UPDATE_NEVER, .240,
        psl__SetGapTime,         psl__GetGapTime,     NULL
    },

    {   "trigger_peaking_time", TRUE_, FALSE_, STJ_UPDATE_NEVER, .100,
        psl__SetTrigPeakingTime, NULL, NULL
    },

    {   "trigger_gap_time",     TRUE_, FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetTrigGapTime,     NULL, NULL
    },

    {   "trigger_threshold",    TRUE_, FALSE_, STJ_UPDATE_NEVER, 1000.0,
        psl__SetTThresh,         NULL,     NULL
    },

    {   "energy_threshold",    TRUE_, FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetEThresh,         NULL,     NULL
    },

    {   "baseline_average",     TRUE_, FALSE_, STJ_UPDATE_NEVER, 256.0,
        psl__SetBaseAvg,         NULL,     NULL
    },

    {   "maxwidth",              TRUE_, FALSE_, STJ_UPDATE_NEVER, 1.000,
        psl__SetMaxWidth,        NULL, NULL
    },

    {   "peak_mode",           TRUE_, FALSE_, STJ_UPDATE_NEVER,  1.0,
        psl__SetPeakMode,      NULL, NULL
    },

    {   "trace_trigger_enable",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetTraceTriggerEnable,      NULL, NULL
    },

    {   "trace_trigger_type",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetTraceTriggerType,      NULL, NULL
    },

    {   "trace_trigger_position",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetTraceTriggerPosition,      NULL, NULL
    },

    {   "mapping_mode",          TRUE_, FALSE_, STJ_UPDATE_NEVER, 0.0,
        psl__SetMappingMode,     NULL, NULL
    },

    {   "bias_scan_start_offset",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetBiasScanStartOffset,      NULL, NULL
    },

    {   "bias_scan_steps",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  4000.0,
        psl__SetBiasScanSteps,      NULL, NULL
    },

    {   "bias_scan_step_size",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  1.0,
        psl__SetBiasScanStepSize,      NULL, NULL
    },

    {   "bias_scan_wait_time",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  10.0,
        psl__SetBiasScanWaitTime,      NULL, NULL
    },

    {   "bias_dac_set_zero",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetBiasDacSetZero,      psl__GetBiasDacSetZero, NULL
    },

    {   "bias_dac_zero",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetBiasDacZero,      psl__GetBiasDacZero, NULL
    },

    {   "bias_set_dac",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetBiasSetDac,      NULL, NULL
    },

    {   "bias_dac_enable",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  1.0,
        psl__SetBiasDacEnable,      NULL, NULL
    },


    {   "list_mode_variant", TRUE_, FALSE_, STJ_UPDATE_MAPPING,
        XIA_LIST_MODE_PMT, psl__SetListModeVariant, NULL,
        NULL
    },

    /* PMT specific acquisition values will work for STJ as well */
    {   "pmt_trigger_mode",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetSetPmtTriggerMode,      NULL, NULL
    },

    {   "pmt_dynode_threshold",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  3000.0,
        psl__SetPmtDynodeThreshold,      NULL, NULL
    },

    {   "pmt_dynode_sum_threshold",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetPmtDynodeSumThreshold,   NULL, NULL
    },

    {   "pmt_multiplicity_length",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetPmtMultiLen,      NULL, NULL
    },

    {   "pmt_multiplicity_requirement",  TRUE_, FALSE_, STJ_UPDATE_NEVER,  0.0,
        psl__SetPmtMultiReq,      NULL, NULL
    },
};


#define SCA_LIMIT_STR_LEN 3
#define DATA_MEMORY_STR_LEN 18


/*
 * Initializes the PSL functions for the Stj hardware.
 */
PSL_EXPORT int PSL_API stj_PSLInit(PSLFuncs *funcs)
{
    funcs->validateDefaults     	= pslValidateDefaults;
    funcs->validateModule       	= pslValidateModule;
    funcs->downloadFirmware     	= pslDownloadFirmware;
    funcs->setAcquisitionValues 	= pslSetAcquisitionValues;
    funcs->getAcquisitionValues 	= pslGetAcquisitionValues;
    funcs->gainOperation        	= pslGainOperation;
    funcs->gainCalibrate        	= pslGainCalibrate;
    funcs->startRun             	= pslStartRun;
    funcs->stopRun          			= pslStopRun;
    funcs->getRunData      				= pslGetRunData;
    funcs->doSpecialRun      			= pslDoSpecialRun;
    funcs->getSpecialRunData 			= pslGetSpecialRunData;
    funcs->getDefaultAlias     		= pslGetDefaultAlias;
    funcs->getParameter        		= pslGetParameter;
    funcs->setParameter      			= pslSetParameter;
    funcs->moduleSetup            = pslModuleSetup;
    funcs->userSetup            	= pslUserSetup;
    funcs->canRemoveName        	= pslCanRemoveName;
    funcs->getNumDefaults       	= pslGetNumDefaults;
    funcs->getNumParams         	= pslGetNumParams;
    funcs->getParamData         	= pslGetParamData;
    funcs->getParamName         	= pslGetParamName;
    funcs->boardOperation       	= pslBoardOperation;
    funcs->freeSCAs             	= pslDestroySCAs;
    funcs->unHook               	= pslUnHook;

    stj_psl_md_alloc = utils->funcs->dxp_md_alloc;
    stj_psl_md_free  = utils->funcs->dxp_md_free;

    return XIA_SUCCESS;
}


/*
 * Validate that the module is correctly configured for the Stj
 * hardware.
 */
PSL_STATIC int pslValidateModule(Module *module)
{
    UNUSED(module);

    return XIA_SUCCESS;
}


/*
 * Validate that the defined defaults are correct for the Stj hardware.
 */
PSL_STATIC int pslValidateDefaults(XiaDefaults *defaults)
{
    UNUSED(defaults);

    return XIA_SUCCESS;
}


/*
 * Download the specified firmware to the hardware.
 */
PSL_STATIC int pslDownloadFirmware(int detChan, char *type, char *file,
                                   Module *m, char *rawFile, XiaDefaults *defs)
{
    int status;
    int i;

    UNUSED(defs);


    ASSERT(type    != NULL);
    ASSERT(file    != NULL);
    ASSERT(m       != NULL);
    ASSERT(rawFile != NULL);


    for (i = 0; i < N_ELEMS(FIRMWARE); i++) {
        if (STREQ(type, FIRMWARE[i].name)) {

            status = FIRMWARE[i].fn(detChan, file, rawFile, m);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error downloading firmware '%s' to detChan %d",
                        type, detChan);
                pslLogError("pslDownloadFirmware", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown firmware type '%s' requested for download to "
            "detChan %d", type, detChan);
    pslLogError("pslDownloadFirmware", info_string, XIA_UNKNOWN_FIRM);
    return XIA_UNKNOWN_FIRM;
}


/*
 * The master routine used to set the specified acquisition value.
 *
 * This routine decodes the specified acquisition value and dispatches
 * the appropriate information to the routine responsible for modifying
 * adding/updating the acquisition value.
 *
 * - detChan The channel to set the acquisition value for.
 * - name The name of the acquisition value to set.
 * - value Pointer to the acquisition value data.
 * - defaults Pointer to the defaults assigned to this detChan.
 * - firmwareSet Pointer to the firmware set assigned to this detChan.
 * - currentFirmware Pointer to the current firmware assigned to this detChan.
 * - detectorType The detector preamplifier type: "RC" or "RESET".
 * - detector Pointer to the detector associated with this detChan.
 * - detector_chan The preamplifier on the detector assigned to this detChan.
 * - m Pointer to the module detChan is a member of.
 * - modChan Logical module channel.
 */
PSL_STATIC int pslSetAcquisitionValues(int detChan, char *name, void *value,
                                       XiaDefaults *defaults,
                                       FirmwareSet *firmwareSet,
                                       CurrentFirmware *currentFirmware,
                                       char *detectorType, Detector *detector,
                                       int detector_chan, Module *m,
                                       int modChan)
{
    int i;
    int status;
    int error_status;

    double original_value = 0.0;

    UNUSED(currentFirmware);
    UNUSED(detector_chan);


    ASSERT(name        != NULL);
    ASSERT(value       != NULL);
    ASSERT(defaults    != NULL);
    ASSERT(firmwareSet != NULL);


    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (STRNEQ(name, ACQ_VALUES[i].name)) {

            /* Cache the current value in case we need to rollback. */
            status = pslGetDefault(name, (void *)&original_value, defaults);
            ASSERT(status == XIA_SUCCESS);

            status = ACQ_VALUES[i].setFN(detChan, modChan, name, value, detectorType,
                                         defaults, m, detector, firmwareSet);

            if (status != XIA_SUCCESS) {
                /* Some acquisition values have to call pslSetDefault() before they
                * can process the acquisition value. So, to be safe, we need to
                * roll the acquisition value back.
                *
                * NOTE: We don't try and reset the value completely by calling
                * pslSetAcquisitionValues() again as that could cause infinite recursion.
                * We need to make it clear in the manual that the user should try and
                * set the value again after an error. In practice, this may not be
                * enough and we may have to try and call pslSetAcquisitionValues()
                * again, but we will cross that bridge when we get to it.
                */
                error_status = pslSetDefault(name, (void *)&original_value, defaults);
                ASSERT(error_status == XIA_SUCCESS);

                sprintf(info_string, "'%s' reverted to %0.6f", name, original_value);
                pslLogInfo("pslSetAcquisitionValues", info_string);

                sprintf(info_string, "Error setting '%s' to %0.6f for detChan %d",
                        name, *((double *)value), detChan);
                pslLogError("pslSetAcquisitionValues", info_string, status);
                return status;
            }

            status = pslSetDefault(name, value, defaults);
            /* It is an "impossible" event for this routine to fail */
            ASSERT(status == XIA_SUCCESS);

            return XIA_SUCCESS;
        }
    }

    /* Is it possibly a raw DSP parameter? */
    if (pslIsUpperCase(name)) {
        status = psl__UpdateRawParamAcqValue(detChan, name, value, defaults);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting DSP parameter '%s' as an "
                    "acquisition value for detChan %d.", name, detChan);
            pslLogError("pslSetAcquisitionValues", info_string, status);
            return status;
        }

        return XIA_SUCCESS;
    }

    sprintf(info_string, "Unknown acquisition value '%s' for detChan %d", name,
            detChan);
    pslLogError("pslSetAcquisitionValues", info_string, XIA_UNKNOWN_VALUE);
    return XIA_UNKNOWN_VALUE;
}


/*
 * Gets the current value of the requested acquisition value.
 */
PSL_STATIC int pslGetAcquisitionValues(int detChan, char *name, void *value,
                                       XiaDefaults *defaults)
{
    int i;
    int status;


    ASSERT(name     != NULL);
    ASSERT(value    != NULL);
    ASSERT(defaults != NULL);


    /* Preload the returned value with what is currently in the defaults list
    * and then allow the individual acquisition values to update it if necessary.
    */
    status = pslGetDefault(name, value, defaults);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting acquisition value '%s' for detChan %d",
                name, detChan);
        pslLogError("pslGetAcquisitionValues", info_string, status);
        return status;
    }

    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (STRNEQ(name, ACQ_VALUES[i].name)) {

            /* If the get function is not impelement just use the current values */
            if (ACQ_VALUES[i].getFN == NULL) {
                return XIA_SUCCESS;
            }

            status = ACQ_VALUES[i].getFN(detChan, value, defaults);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error updating '%s' for detChan %d", name, detChan);
                pslLogError("pslGetAcquisitionValues", info_string, status);
                return status;
            }

            /* By definition, these updated values are not meant to be written
            * to the defaults list since doing so may corrupt the intent of the
            * current setting. For instance, if you have an acquisition value
            * where -1.0 means "maximize", then you always want to keep it at -1.0
            * even though -1.0 doesn't tell the user what the actual value
            * on the hardware is.
            */
        }
    }

    return XIA_SUCCESS;
}


/*
 * Wrapper function for pslGainCalibrate
 */
PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det,
                                  int modChan, Module *m, XiaDefaults *defs,
                                  void *value)
{
    double *deltaGain  = (double *)value;
    return pslGainCalibrate(detChan, det, modChan, m, defs, *deltaGain);
}


/*
 * Scale the analog gain according to given scale factor
 */
PSL_STATIC int pslGainCalibrate(int detChan, Detector *det,
                                int modChan, Module *m, XiaDefaults *defs,
                                double deltaGain)
{
    int status;

    double preampGain = 0.0;

    ASSERT(defs != NULL);
    ASSERT(m != NULL);
    ASSERT(det != NULL);

    if (deltaGain <= 0) {
        sprintf(info_string, "Invalid gain scale factor %0.3f for "
                "detChan %d", deltaGain, detChan);
        pslLogError("pslGainCalibrate", info_string, XIA_GAIN_SCALE);
        return XIA_GAIN_SCALE;
    }

    status = pslGetDefault("preamp_gain", (void *)&preampGain, defs);
    ASSERT(status == XIA_SUCCESS);

    sprintf(info_string, "Scaling analog gain for detChan %d "
            "preamp_gain = %0.3f, deltaGain = %0.3f", detChan, preampGain, deltaGain);
    pslLogDebug("pslGainCalibrate", info_string);

    preampGain *= 1.0 / deltaGain;

    status = psl__SetPreampGain(detChan, modChan, NULL, (void *)&preampGain, "",
                                defs, m, det, NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the preamplifier gain to %0.3f for "
                "detChan %d", preampGain, detChan);
        pslLogError("pslGainCalibrate", info_string, status);
        return status;
    }

    status = pslSetDefault("preamp_gain", (void *)&preampGain, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


/*
 * Wrapper function for digital gain calbiration psl__ScaleDigitalGain.
 */
PSL_STATIC int psl__DigitalGainCalibrate(int detChan,  Detector *det,
                                         int modChan, Module *m, XiaDefaults *defs,
                                         void *value)
{
    UNUSED(m);
    UNUSED(modChan);

    return psl__ScaleDigitalGain(detChan, value,  defs, det);
}


/*
 * Calibrates the digital gain using the specified delta.
 *
 * Adjusts the digital gain by the inverse of the specified delta but
 * doesn't change the associated analog gain.
 *
 * NOTE this operation shouldn't be classified as a special run strictly
 * but we need to use the special run calls to access the detector value.
 */
PSL_STATIC int psl__ScaleDigitalGain(int detChan, void *value, XiaDefaults *defs,
                                     Detector *det)
{
    int status;

    parameter_t MCAGAIN = 0;
    parameter_t MCAGAINEXP = 0;

    double digitalGain = 0.0;
    double preampGain = 0.0;
    double deltaGain = *(double *) value;

    ASSERT(defs != NULL);

    if (deltaGain <= 0) {
        sprintf(info_string, "Invalid gain scale factor %0.3f for "
                "detChan %d", deltaGain, detChan);
        pslLogError("psl__ScaleDigitalGain", info_string, XIA_GAIN_SCALE);
        return XIA_GAIN_SCALE;
    }

    /* Update the preamp_gain value without triggering a recalculation of
     *  the analog gain.
     */
    preampGain = det->gain[detChan] / deltaGain;

    sprintf(info_string, "Scaling preamp gain for detChan %d to = %0.3f",
            detChan, preampGain);
    pslLogDebug("psl__ScaleDigitalGain", info_string);

    det->gain[detChan] = preampGain;
    status = pslSetDefault("preamp_gain", (void *)&preampGain, defs);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetParameter(detChan, "MCAGAINEXP", &MCAGAINEXP);

    if (status != XIA_SUCCESS) {
        pslLogError("psl__ScaleDigitalGain", "Error getting DSP parameter MCAGAINEXP", status);
        return status;
    }

    status = pslGetParameter(detChan, "MCAGAIN", &MCAGAIN);

    if (status != XIA_SUCCESS) {
        pslLogError("psl__ScaleDigitalGain", "Error getting DSP parameter MCAGAIN", status);
        return status;
    }

    digitalGain =  ((double)MCAGAIN / 32768.0) * pow(2.0, (double)MCAGAINEXP);

    sprintf(info_string, "Scaling digital gain for detChan %d "
            "old digital gain = %0.3f, deltaGain = %0.3f", detChan, digitalGain, deltaGain);
    pslLogDebug("psl__ScaleDigitalGain", info_string);

    digitalGain *= deltaGain;

    status = psl__SetDigitalGain(detChan, digitalGain);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting digital gain "
                "to %0.3f for detChan %d", digitalGain, detChan);
        pslLogError("pslGainCalibrate", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Starts a run on the specified channel.
 *
 * On the STJ hardware, starting a run on a single is treated as a broadcast
 * to all of the channels.
 */
PSL_STATIC int pslStartRun(int detChan, unsigned short resume,
                           XiaDefaults *defaults, Module *m)
{
    int statusX;
    unsigned short ignored_gate   = 0;

    UNUSED(defaults);
    UNUSED(m);

    statusX = dxp_start_one_run(&detChan, &ignored_gate, &resume);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error starting run on detChan = %d", detChan);
        pslLogError("pslStartRun", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Stops a run on the specified channel.
 *
 * On the Stj hardware, stopping a run on a single channel is treated as a
 * broadcast to all of the channels.
 */
PSL_STATIC int pslStopRun(int detChan, Module *m)
{
    int statusX;

    UNUSED(m);


    statusX = dxp_stop_one_run(&detChan);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping run on detChan = %d", detChan);
        pslLogError("pslStopRun", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Get the specified acquisition run data from the hardware.
 */
PSL_STATIC int pslGetRunData(int detChan, char *name, void *value,
                             XiaDefaults *defaults, Module *m)
{
    int i;
    int status;


    ASSERT(name     != NULL);
    ASSERT(value    != NULL);
    ASSERT(defaults != NULL);
    ASSERT(m        != NULL);


    if (STREQ(name, "livetime")) {
        pslLogWarning("pslGetRunData", "'livetime' is deprecated as a run data "
                      "type. Use 'trigger_livetime' or 'energy_livetime' "
                      "instead.");

    } else if (STREQ(name, "events_in_run")) {
        pslLogWarning("pslGetRunData", "'events_in_run' is deprecated as a run "
                      "data type. Use 'mca_events' or 'total_output_events' "
                      "instead.");
    }

    for (i = 0; i < N_ELEMS(runData); i++) {
        if (STREQ(name, runData[i].name)) {

            status = runData[i].fn(detChan, value, defaults, m);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error getting run data '%s' for detChan %d", name,
                        detChan);
                pslLogError("pslGetRunData", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown run data '%s' for detChan %d", name, detChan);
    pslLogError("pslGetRunData", info_string, XIA_BAD_NAME);

    return XIA_BAD_NAME;
}


/*
 * Performs the requested special run.
 */
PSL_STATIC int pslDoSpecialRun(int detChan, char *name, void *info,
                               XiaDefaults *defaults, Detector *detector,
                               int detector_chan)
{
    int status;
    int i;
    short specialRunType = 0;

    UNUSED(detector);
    UNUSED(detector_chan);
    UNUSED(defaults);

    ASSERT(name != NULL);

    /* Check for match in trace type first */
    for (i = 0; i < N_ELEMS(traceTypes); i++) {
        if (STREQ(traceTypes[i].name, name)) {

            specialRunType = (short) i;
            status = psl__DoTrace(detChan, specialRunType, (double *)info);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error doing trace run '%s' type %hd on detChan %d",
                        name, specialRunType, detChan);
                pslLogError("pslDoSpecialRun", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    /* Try to match special run  */
    for (i = 0; i < N_ELEMS(specialRun); i++) {
        if (STREQ(specialRun[i].name, name)) {

            status = specialRun[i].fn(detChan, info, defaults, detector);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error doing special run '%s' type %hd on detChan %d",
                        name, specialRunType, detChan);
                pslLogError("pslDoSpecialRun", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown special run '%s' for detChan %d", name, detChan);
    pslLogError("pslDoSpecialRun", info_string, XIA_BAD_NAME);

    return XIA_BAD_NAME;
}


/*
 * Get the specified special run data from the hardware.
 */
PSL_STATIC int pslGetSpecialRunData(int detChan, char *name, void *value,
                                    XiaDefaults *defaults)
{
    int status;
    int i;


    ASSERT(name     != NULL);
    ASSERT(value    != NULL);
    ASSERT(defaults != NULL);


    for (i = 0; i < N_ELEMS(specialRunData); i++) {
        if (STREQ(specialRunData[i].name, name)) {

            status = specialRunData[i].fn(detChan, value, defaults);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error getting special run data '%s' for "
                        "detChan %d", name, detChan);
                pslLogError("pslGetSpecialRunData", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown special run data type '%s' for detChan %d",
            name, detChan);
    pslLogError("pslGetSpecialRunData", info_string, XIA_BAD_NAME);

    return XIA_BAD_NAME;
}


/*
 * Returns a list of the "default" defaults.
 */
PSL_STATIC int pslGetDefaultAlias(char *alias, char **names, double *values)
{
    int i;
    int def_idx;

    char *aliasName = "defaults_stj";


    ASSERT(alias  != NULL);
    ASSERT(names  != NULL);
    ASSERT(values != NULL);


    for (i = 0, def_idx = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (ACQ_VALUES[i].isDefault) {

            strcpy(names[def_idx], ACQ_VALUES[i].name);
            values[def_idx] = ACQ_VALUES[i].def;
            def_idx++;
        }
    }

    strcpy(alias, aliasName);

    return XIA_SUCCESS;
}


/*
 * Get the value of the specified DSP parameter from the hardware.
 */
PSL_STATIC int pslGetParameter(int detChan, const char *name,
                               unsigned short *value)
{
    int statusX;


    ASSERT(name != NULL);
    ASSERT(value != NULL);


    statusX = dxp_get_one_dspsymbol(&detChan, (char *)name, value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading %s for detChan %d", name, detChan);
        pslLogError("pslGetParameter", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Set the specified DSP parameter on the hardware.
 */
PSL_STATIC int pslSetParameter(int detChan, const char *name,
                               unsigned short value)
{
    int statusX;

#ifdef XIA_PARAM_DEBUG
    parameter_t debugValue;
#endif /* XIA_PARAM_DEBUG */


    ASSERT(name != NULL);


    statusX = dxp_set_one_dspsymbol(&detChan, (char *)name, &value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting %s to %#hx for detChan %d", name, value,
                detChan);
        pslLogError("pslSetParameter", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

#ifdef XIA_PARAM_DEBUG
    /* XIA_PARAM_DEBUG mode reads back the parameter we just wrote to
    * verify that it was actually set. This debugging directive will really
    * slow down an application, so please only turn it on if you need it.
    */
    sprintf(info_string, "XIA_PARAM_DEBUG: '%s' = %#hx, detChan = %d",
            name, value, detChan);
    pslLogDebug("pslSetParameter", info_string);

    statusX = dxp_get_one_dspsymbol(&detChan, (char *)name, &debugValue);
    ASSERT(statusX == DXP_SUCCESS);

    if (debugValue != value) {
        sprintf(info_string, "XIA_PARAM_DEBUG: Wrote %#hx to '%s', read back "
                "%#x for detChan %d", value, name, debugValue, detChan);
        pslLogError("pslSetParameter", info_string, XIA_PARAM_DEBUG_MISMATCH);
        return XIA_PARAM_DEBUG_MISMATCH;
    }
#endif /* XIA_PARAM_DEBUG */

    return XIA_SUCCESS;
}


/*
 * Setup per-module settings, this is done after all the
 * acquisition values are set up.
 */
PSL_STATIC int pslModuleSetup(int detChan, XiaDefaults *defaults,
                              Module *m)
{
    int status;

    UNUSED(m);

    sprintf(info_string, "Applying per module setting for the module that "
            "includes detChan %d.", detChan);
    pslLogDebug("pslModuleSetup", info_string);

    status = pslApply(detChan, NULL, defaults, NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error applying acquisition values for module "
                "that includes detChan %d", detChan);
        pslLogError("pslModuleSetup", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets all of the acquisition values to their initial setting and
 * configures the filter parameters.
 */
PSL_STATIC int pslUserSetup(int detChan, XiaDefaults *defaults,
                            FirmwareSet *firmwareSet,
                            CurrentFirmware *currentFirmware,
                            char *detectorType, Detector *detector,
                            int detector_chan, Module *m, int modChan)
{
    int i;
    int status;

    XiaDaqEntry *entry = NULL;

    ASSERT(defaults);
    ASSERT(m);
    ASSERT(firmwareSet);
    ASSERT(currentFirmware);
    ASSERT(detectorType);
    ASSERT(detector);
    ASSERT((modChan >= 0) && (modChan < 32));

    /* Initialize input to No Connection for each module */
    if (!m->isSetup) {
        sprintf(info_string, "Setting input to NC for detChan %d.", detChan);
        pslLogDebug("pslUserSetup", info_string);

        status = psl__SetInputNC(detChan);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting the input LEMO to the No "
                    "Connection state for the module that includes detChan %d.",
                    detChan);
            pslLogError("pslUserSetup", info_string, status);
            return status;
        }
    }

    entry = defaults->entry;
    ASSERT(entry != NULL);

    /* Some acquisition values require synchronization with another data
    * structure in the program prior to setting the initial acquisition
    * value.
    */
    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (ACQ_VALUES[i].isSynch) {
            status = ACQ_VALUES[i].synchFN(detChan, detector_chan, m, detector,
                                           defaults);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error synchronizing '%s' for detChan %d",
                        ACQ_VALUES[i].name, detChan);
                pslLogError("pslUserSetup", info_string, status);
                return status;
            }
        }
    }

    while (entry != NULL) {

        sprintf(info_string, "Setting '%s' to %0.6f for detChan %d",
                entry->name, entry->data, detChan);
        pslLogDebug("pslUserSetup", info_string);

        status = pslSetAcquisitionValues(detChan, entry->name, (void *)&entry->data,
                                         defaults, firmwareSet, currentFirmware,
                                         detectorType, detector, detector_chan, m,
                                         modChan);


        /* Try to roll back to default acquisition value if settings in
        *  configuration file is out of range.
        */
        if (status == XIA_BAD_VALUE) {

            status = psl__RevertDefault(entry->name, &entry->data);
            ASSERT(status == XIA_SUCCESS);

            sprintf(info_string, "Reset '%s' to default value %0.6f for detChan %d",
                    entry->name, entry->data, detChan);
            pslLogWarning("pslUserSetup", info_string);

            status = pslSetAcquisitionValues(detChan, entry->name,
                                             (void *)&entry->data,
                                             defaults, firmwareSet, currentFirmware,
                                             detectorType, detector, detector_chan, m,
                                             modChan);
        }

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting '%s' to %0.6f for detChan %d",
                    entry->name, entry->data, detChan);
            pslLogError("pslUserSetup", info_string, status);
            return status;
        }

        entry = entry->next;
    }

    return XIA_SUCCESS;
}

/*
 * Revert the acquisition value to program defaults
 */
PSL_STATIC int psl__RevertDefault(char *name, double *value)
{
    int i;

    for (i = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (STRNEQ(name, ACQ_VALUES[i].name)) {
            *value = ACQ_VALUES[i].def;
            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown acquisition value '%s'", name);
    pslLogError("psl__RevertDefault", info_string, XIA_UNKNOWN_VALUE);
    return XIA_UNKNOWN_VALUE;
}


/*
 * Checks if the specified name is a require acquisition value or not.
 */
PSL_STATIC boolean_t pslCanRemoveName(char *name)
{
    UNUSED(name);

    return TRUE_;
}


/*
 * Returns the number of "default" defaults.
 */
PSL_STATIC unsigned int pslGetNumDefaults(void)
{
    int i;

    unsigned int n = 0;


    for (i = 0, n = 0; i < N_ELEMS(ACQ_VALUES); i++) {
        if (ACQ_VALUES[i].isDefault) {
            n++;
        }
    }

    return n;
}


/*
 * Get the number of DSP parameters defined for the given channel.
 */
PSL_STATIC int pslGetNumParams(int detChan, unsigned short *numParams)
{
    int statusX;


    ASSERT(numParams != NULL);


    statusX = dxp_max_symbols(&detChan, numParams);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting the number of DSP parameters for "
                "detChan %d", detChan);
        pslLogError("pslGetNumParams", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the requested parameter data.
 */
PSL_STATIC int PSL_API pslGetParamData(int detChan, char *name, void *value)
{
    int i;
    int status;


    ASSERT(name != NULL);
    ASSERT(value != NULL);


    for (i = 0; i < N_ELEMS(PARAM_DATA); i++) {
        if (STREQ(name, PARAM_DATA[i].name)) {

            status = PARAM_DATA[i].fn(detChan, value);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error getting parameter data '%s' for detChan %d",
                        PARAM_DATA[i].name, detChan);
                pslLogError("pslGetParamData", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown parameter data type '%s' for detChan %d",
            name, detChan);
    pslLogError("pslGetParamData", info_string, XIA_UNKNOWN_PARAM_DATA);
    return XIA_UNKNOWN_PARAM_DATA;
}


/*
 * Helper routine for VB-type applications that are calling Handel.
 * Returns the name of the parameter listed at index. Ordinarily, one would
 * call pslGetParams(), which returns an array of all of the DSP parameter
 * names, but not all languages can interface to that type of an argument.
 *
 * name must be at least MAX_DSP_PARAM_NAME_LEN characters long.
 */
PSL_STATIC int pslGetParamName(int detChan, unsigned short index, char *name)
{
    int statusX;


    ASSERT(name != NULL);


    statusX = dxp_symbolname_by_index(&detChan, &index, name);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting parameter located at index %hu for "
                "detChan %d", index, detChan);
        pslLogError("pslGetParamName", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Perform the specified gain operation to the hardware.
 */
PSL_STATIC int pslGainOperation(int detChan, char *name, void *value,
                                Detector *det, int modChan, Module *m, XiaDefaults *defs)
{
    int status;
    int i;

    ASSERT(name  != NULL);
    ASSERT(value != NULL);
    ASSERT(defs  != NULL);
    ASSERT(det   != NULL);
    ASSERT(m     != NULL);

    for (i = 0; i < N_ELEMS(gainOps); i++) {
        if (STREQ(name, gainOps[i].name)) {

            status = gainOps[i].fn(detChan, det, modChan, m, defs, value);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error doing gain operation '%s' for detChan %d",
                        name, detChan);
                pslLogError("pslGainOperation", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown gain operation '%s' for detChan %d", name,
            detChan);
    pslLogError("pslGainOperation", info_string, XIA_BAD_NAME);

    return XIA_BAD_NAME;
}


/*
 * Perform the specified board operation to the hardware.
 */
PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value,
                                 XiaDefaults *defs)
{
    int status;
    int i;


    ASSERT(name  != NULL);
    ASSERT(value != NULL);
    ASSERT(defs  != NULL);


    for (i = 0; i < N_ELEMS(boardOps); i++) {
        if (STREQ(name, boardOps[i].name)) {

            status = boardOps[i].fn(detChan, name, defs, value);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error doing board operation '%s' for detChan %d",
                        name, detChan);
                pslLogError("pslBoardOperation", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown board operation '%s' for detChan %d", name,
            detChan);
    pslLogError("pslBoardOperation", info_string, XIA_BAD_NAME);

    return XIA_BAD_NAME;
}


/*
 * Cleans up any resources required by the communication protocol.
 *
 * Handel only passes in detChans that are actual channels, not channel sets.
 */
PSL_STATIC int pslUnHook(int detChan)
{
    int statusX;


    statusX = dxp_exit(&detChan);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error shutting down detChan = %d", detChan);
        pslLogError("pslUnHook", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the ADC Trace length from the hardware
 */
PSL_STATIC int pslGetADCTraceLen(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    parameter_t TRACELEN = 0;

    UNUSED(defs);


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "TRACELEN", &TRACELEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading TRACELEN from detChan %d", detChan);
        pslLogError("pslGetADCTraceLen", info_string, status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)TRACELEN;

    return XIA_SUCCESS;
}


/*
 * Get the ADC trace from the board.
 *
 * Getting the data stops the control task. If you do an ADC trace special
 * run then you are required to read the data out to properly stop the run.
 */
PSL_STATIC int pslGetADCTrace(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    short type = STJ_CT_ADC;

    UNUSED(defs);

    ASSERT(value != NULL);

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping control task run on detChan %d",
                detChan);
        pslLogError("pslGetADCTrace", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    statusX = dxp_get_control_task_data(&detChan, &type, value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading ADC trace data for detChan %d", detChan);
        pslLogError("pslGetADCTrace", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Applies the current board settings.
 *
 * Performst he special apply run via. Xerxes. See dxp_do_apply() in stj.c
 * for more information.
 */
PSL_STATIC int pslApply(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    short task = STJ_CT_APPLY;

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    statusX = dxp_start_control_task(&detChan, &task, NULL, NULL);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error starting 'apply' control task for detChan %d",
                detChan);
        pslLogError("pslApply", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping 'apply' control task for detChan %d",
                detChan);
        pslLogError("pslApply", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the current MCA spectrum length to the user
 */
PSL_STATIC int pslGetMCALength(int detChan, void *value, XiaDefaults *defs,
                               Module *m)
{
    int statusX;

    unsigned int mcaLen = 0;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value != NULL);


    statusX = dxp_nspec(&detChan, &mcaLen);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting spectrum length for detChan %d",
                detChan);
        pslLogError("pslGetMCALength", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    sprintf(info_string, "MCA length = %u", mcaLen);
    pslLogDebug("pslGetMCALength", info_string);

    *((unsigned long *)value) = (unsigned long)mcaLen;

    return XIA_SUCCESS;
}


/*
 * Get the MCA spectrum
 */
PSL_STATIC int psl__GetMCA(int detChan, void *value, XiaDefaults *defs, Module *m)
{
    int statusX;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value != NULL);


    statusX = dxp_readout_detector_run(&detChan, NULL, NULL,
                                       (unsigned long *)value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading MCA spectrum for detChan %d", detChan);
        pslLogError("psl__GetMCA", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Set the trigger threshold.
 */
PSL_STATIC int psl__SetTThresh(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs)
{
    int status;
    int bank;

    double coeff = 0.0;
    double fscale;
    double preampGain, analogGain;
    double *thresh  = (double *)value;

    char BANKnDSPNAME[20];

    parameter_t THRESHOLD = 0;
    parameter_t FASTLEN = 0;
    parameter_t FSCALE = 0;
    parameter_t BANKnADCGAIN = 0;

    UNUSED(det);
    UNUSED(m);
    UNUSED(fs);
    UNUSED(detType);
    UNUSED(name);

    ASSERT(value != NULL);
    ASSERT(defs != NULL);

    status = pslGetDefault("preamp_gain", (void *)&preampGain, defs);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetParameter(detChan, "FASTLEN", &FASTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting FASTLEN for detChan %d", detChan);
        pslLogError("psl__SetTThresh", info_string, status);
        return status;
    }

    /* This function can be called right after FASTLEN has changed, and the DSP
     * parameter FSCALE may not have been updated yet, to avoid inconsistency,
     * calculate FSCALE manually instead of reading it out from the DSP
     */
    fscale = floor(log((double)FASTLEN / 8.0) / log(2.0));
    FSCALE = (fscale < 0) ? 0 : (parameter_t)fscale;

    bank = (int) modChan / STJ_CHANNELS_PER_BANK;
    ASSERT(bank < 4);

    sprintf(BANKnDSPNAME, "BANK%dADCGAIN", bank);
    status = pslGetParameter(detChan, BANKnDSPNAME, &BANKnADCGAIN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting %s for detChan %d", BANKnDSPNAME, detChan);
        pslLogError("psl__SetTThresh", info_string, status);
        return status;
    }

    analogGain = pow(10.0, (double)(BANKnADCGAIN - 11) / 20.0);

    sprintf(info_string, "detChan %d Bank %s analogGain = %0.3f, preampGain = %0.3f, "
            "FASTLEN = %hu, FSCALE = %hu",
            detChan, BANKnDSPNAME, analogGain, preampGain, FASTLEN, FSCALE);
    pslLogDebug("psl__SetTThresh", info_string);

    coeff = 1953.13 / ((double)FASTLEN * pow(2.0, 0 - FSCALE) * preampGain * analogGain);

    THRESHOLD = (parameter_t)ROUND(*thresh / coeff);

    if (THRESHOLD > 4095) {
        sprintf(info_string, "Resetting calculated threshold from  %hu to  %hu",
                THRESHOLD, 4095);
        pslLogDebug("psl__SetTThresh", info_string);
        THRESHOLD = 4095;
    }

    sprintf(info_string, "detChan %d, thresh = %0.3f, coeff = %0.3f, THRESHOLD = %hu",
            detChan, *thresh, coeff, THRESHOLD);
    pslLogDebug("psl__SetTThresh", info_string);

    status = pslSetParameter(detChan, "THRESHOLD", THRESHOLD);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting trigger_threshold to %0.3f for detChan %d",
                *thresh, detChan);
        pslLogError("psl__SetTThresh", info_string, status);
        return status;
    }

    /* Re-calculate the threshold based on the rounded value of THRESHOLD and
    * pass it back to the user.
    */
    *thresh = (double)THRESHOLD * coeff;

    return XIA_SUCCESS;
}

/*
 * Sets the energy threshold.
 */
PSL_STATIC int psl__SetEThresh(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs)
{
    int status;
    int bank;

    double coeff = 0.0;
    double preampGain, analogGain;
    double *thresh  = (double *)value;

    char BANKnDSPNAME[20];

    parameter_t SLOWTHRESH = 0;
    parameter_t SLOWLEN = 0;
    parameter_t ESCALE = 0;
    parameter_t BANKnADCGAIN = 0;

    UNUSED(det);
    UNUSED(m);
    UNUSED(fs);
    UNUSED(detType);
    UNUSED(name);

    ASSERT(value != NULL);
    ASSERT(defs != NULL);

    status = pslGetDefault("preamp_gain", (void *)&preampGain, defs);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting SLOWLEN for detChan %d", detChan);
        pslLogError("psl__SetEThresh", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "ESCALE", &ESCALE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting ESCALE for detChan %d", detChan);
        pslLogError("psl__SetEThresh", info_string, status);
        return status;
    }

    bank = (int) modChan / STJ_CHANNELS_PER_BANK;
    ASSERT(bank < 4);

    sprintf(BANKnDSPNAME, "BANK%dADCGAIN", bank);
    status = pslGetParameter(detChan, BANKnDSPNAME, &BANKnADCGAIN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting %s for detChan %d", BANKnDSPNAME, detChan);
        pslLogError("psl__SetEThresh", info_string, status);
        return status;
    }

    analogGain = pow(10.0, (double)(BANKnADCGAIN - 11) / 20.0);

    sprintf(info_string, "detChan %d, Bank %s analogGain = %0.3f, preampGain = %0.3f",
            detChan, BANKnDSPNAME, analogGain, preampGain);
    pslLogDebug("psl__SetEThresh", info_string);

    coeff = 488.28 / ((double)SLOWLEN * pow(2.0, 0 - ESCALE) * preampGain * analogGain);

    SLOWTHRESH = (parameter_t)ROUND(*thresh / coeff);

    if (SLOWTHRESH > 4095) {
        sprintf(info_string, "Resetting calculated threshold from  %hu to  %hu",
                SLOWTHRESH, 4095);
        pslLogDebug("psl__SetEThresh", info_string);
        SLOWTHRESH = 4095;
    }

    sprintf(info_string, "detChan %d, thresh = %0.3f, coeff = %0.3f, SLOWTHRESH = %hu",
            detChan, *thresh, coeff, SLOWTHRESH);
    pslLogDebug("psl__SetEThresh", info_string);

    status = pslSetParameter(detChan, "SLOWTHRESH", SLOWTHRESH);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting energy_threshold to %0.3f for detChan %d",
                *thresh, detChan);
        pslLogError("psl__SetEThresh", info_string, status);
        return status;
    }

    /* Re-calculate the threshold based on the rounded value of SLOWTHRESH and
    * pass it back to the user.
    */
    *thresh = (double)SLOWTHRESH * coeff;

    return XIA_SUCCESS;
}


/*
 * Set the baseline average.
 *
 * Must be a power of 2, but if it isn't then it is silently rounded.
 */
PSL_STATIC int psl__SetBaseAvg(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t BLFILTERLEN = 0;
    double *len  = (double *)value;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);

    ASSERT(value != NULL);

    if (*len < 0 || *len > MAX_BLFILTERLEN) {
        *len = MAX_BLFILTERLEN;
    }

    BLFILTERLEN = (parameter_t)pow(2, ROUND(log(*len) / log(2.0)));

    status = pslSetParameter(detChan, "BLFILTERLEN", BLFILTERLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting baseline average to %.0f for "
                "detChan %d", *len, detChan);
        pslLogError("psl__SetBaseAvg", info_string, status);
        return status;
    }

    *len = (double)BLFILTERLEN;
    return XIA_SUCCESS;
}


/*
 * Sets the calibration energy.
 *
 * Forces a recalculation of the gain.
 */
PSL_STATIC int psl__SetCalibEV(int detChan, int modChan, char *name, void *value,
                               char *detType, XiaDefaults *defs, Module *m,
                               Detector *det, FirmwareSet *fs)
{
    int status;

    UNUSED(detChan);
    UNUSED(fs);
    UNUSED(detType);
    UNUSED(name);

    ASSERT(value != NULL);
    ASSERT(defs != NULL);

    /* The calibration energy will be updated in the defaults list after this
    * routine runs, but we need to update it earlier so that the gain routines
    * can use it.
    */
    status = pslSetDefault("calibration_energy", value, defs);
    /* It is impossible for this routine to fail */
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain for detChan %d", detChan);
        pslLogError("psl__SetCalibEV", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Calculates the digital gain and analog gain
 */
PSL_STATIC int psl__CalculateGain(int modChan, XiaDefaults *defs, Module *m,
                                  Detector *det, parameter_t *BANKnADCGAIN)
{
    int bank;
    int status;
    int chan;

    double analogGain    = 0.0;
    double mcaBinWidth   = 0.0;
    double preampGain    = 0.0;
    double dynamicRange  = 0.0;

    double digitalGain = 0.0;

    ASSERT(m != NULL);
    ASSERT(defs != NULL);
    ASSERT(det != NULL);
    ASSERT(BANKnADCGAIN != NULL);

    /* Assume that dynamic range and MCA bin width is same for all channels
     * on the bank
     */
    status = pslGetDefault("dynamic_range", (void *)&dynamicRange, defs);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetDefault("mca_bin_width", (void *)&mcaBinWidth, defs);
    ASSERT(status == XIA_SUCCESS);

    /* Analog gain needs to be the same for each bank
     * This is calculated by averaging the analog gain for all channels in the bank
     */
    bank = (int) modChan / STJ_CHANNELS_PER_BANK;
    ASSERT(bank < 4);

    for (chan = bank * STJ_CHANNELS_PER_BANK;
            chan < (bank + 1) * STJ_CHANNELS_PER_BANK; chan++) {
        preampGain = det->gain[m->detector_chan[chan]];
        analogGain += (800.0 / (dynamicRange * preampGain / 1000.0));
    }

    analogGain /= STJ_CHANNELS_PER_BANK;

    sprintf(info_string, "Average analog gain for bank %d, modChan %d, %0.3f"
            , bank, modChan, analogGain);
    pslLogDebug("psl__CalculateGain", info_string);

    if (analogGain < 0.5 || analogGain > 17.74) {
        sprintf(info_string, "Calculated analog gain %0.3f is out of range "
                "(%0.3f - %0.3f).", analogGain, 0.5, 17.74);
        pslLogError("psl__CalculateGain", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    *BANKnADCGAIN = (parameter_t)ROUND(log10(analogGain) * 20.0 + 11);

    /* Recalculate analogGain based on rounded off value */
    analogGain = pow(10.0, (*BANKnADCGAIN - 11.0) / 20.0);

    sprintf(info_string, "Gain setting: dynamic range = %0.3f, preampGain = %0.3f"
            ", mcaBinWidth = %0.3f, analog gain = %0.3f, BANKnADCGAIN = %d",
            dynamicRange, preampGain, mcaBinWidth, analogGain, *BANKnADCGAIN );
    pslLogDebug("psl__CalculateGain", info_string);

    /* Since modifying preamp_gain will change the per bank average
     * Digital gain has to be recalculated and reset for all channels in the bank.
     * This is not very efficient but it's the only way to ensure accurate setting
     */
    for (chan = bank * STJ_CHANNELS_PER_BANK;
            chan < (bank + 1) * STJ_CHANNELS_PER_BANK; chan++) {

        if (m->channels[chan] < 0) continue;

        preampGain = det->gain[m->detector_chan[chan]];

        /* Digital gain is adjusted per channel to compensate for the variation
         * in analogGain. In this calculation preamp gain is scaled to V/keV, and
         * MCA bin with is scaled to keV/bin
         */
        digitalGain = 1000000 / (preampGain * analogGain * mcaBinWidth * STJ_ADC_PER_EV);

        status = psl__SetDigitalGain(m->channels[chan], digitalGain);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting new digital gain value for detChan %d",
                    m->channels[chan]);
            pslLogError("psl__CalculateGain", info_string, status);
            return status;
        }
    }

    return XIA_SUCCESS;
}


/*
 * Updates the current gain setting based on the current acquisition
 * values.
 */
PSL_STATIC int psl__UpdateGain(int detChan, int modChan, XiaDefaults *defs,
                               Module *m, Detector *det)
{
    int status;
    int bank;

    parameter_t BANKnADCGAIN  = 0;

    double triggerThreshold = 0.0;
    double energyThreshold = 0.0;

    char BANKnDSPNAME[20];

    ASSERT(defs != NULL);
    ASSERT(m != NULL);
    ASSERT(det != NULL);

    status = psl__CalculateGain(modChan, defs, m, det, &BANKnADCGAIN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error calculating new gain values for detChan %d",
                detChan);
        pslLogError("psl__UpdateGain", info_string, status);
        return status;
    }

    bank = (int) modChan / STJ_CHANNELS_PER_BANK;
    ASSERT(bank < 4);

    sprintf(BANKnDSPNAME, "BANK%dADCGAIN", bank);
    status = pslSetParameter(detChan, BANKnDSPNAME, BANKnADCGAIN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting %s for detChan %d", BANKnDSPNAME,
                detChan);
        pslLogError("psl__UpdateGain", info_string, status);
        return status;
    }

    /* Need to update the threshold which is dependent on gain. */
    status = pslGetDefault("trigger_threshold", (void *)&triggerThreshold, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__SetTThresh(detChan, modChan, NULL, (void *)&triggerThreshold,
                             NULL, defs, m, det, NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating trigger threshold due to a change in "
                "gain for detChan %d", detChan);
        pslLogError("psl__UpdateGain", info_string, status);
        return status;
    }

    status = pslSetDefault("trigger_threshold", (void *)&triggerThreshold, defs);
    ASSERT(status == XIA_SUCCESS);

    /* Update energy threshold from new gain values */
    status = pslGetDefault("energy_threshold", (void *)&energyThreshold, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__SetEThresh(detChan, modChan, NULL, (void *)&energyThreshold, NULL, defs, m, det,
                             NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating energy threshold due to a change in "
                "gain for detChan %d", detChan);
        pslLogError("psl__UpdateGain", info_string, status);
        return status;
    }

    status = pslSetDefault("energy_threshold", (void *)&energyThreshold, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}

/*
 * Sets the digital gain parameters.
 */
PSL_STATIC int psl__SetDigitalGain(int detChan, double digitalGain)
{
    int status;
    int exp;

    parameter_t MCAGAIN       = 0;
    parameter_t MCAGAINEXP    = 0;

    /* Scale MCAGAIN by 1/2 here so that it's within 0x2000 - 0x4000 */
    MCAGAIN = (parameter_t) (frexp(digitalGain, &exp) * 16384);
    MCAGAINEXP = (parameter_t)(exp + 1);

    sprintf(info_string, "DetChan %d: digital gain = %0.3f, "
            "MCAGAIN = %#hx, MCAGAINEXP = %#hx",
            detChan, digitalGain, MCAGAIN, MCAGAINEXP);
    pslLogDebug("psl__SetDigitalGain", info_string);

    if (exp < -8 || exp > 7) {
        sprintf(info_string, "Calculated digital gain exponent %d is out of range "
                "(-8, 7).", exp);
        pslLogError("psl__SetDigitalGain", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    status = pslSetParameter(detChan, "MCAGAINEXP", MCAGAINEXP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting digital gain "
                "to %0.3f for detChan %d", digitalGain, detChan);
        pslLogError("psl__SetDigitalGain", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "MCAGAIN", MCAGAIN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting digital gain "
                "to %0.3f for detChan %d", digitalGain, detChan);
        pslLogError("psl__SetDigitalGain", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Sets the MCA bin width, also known as eV/bin.
 *
 * Changing this value forces a recalculation of the gain.
 */
PSL_STATIC int psl__SetMCABinWidth(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    UNUSED(fs);
    UNUSED(detType);
    UNUSED(name);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    /* The MCA bin width will be updated in the defaults list after this
    * routine runs, but we need to update it earlier so that the gain routines
    * can use it.
    */
    status = pslSetDefault("mca_bin_width", value, defs);
    /* It is impossible for this routine to fail */
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain for detChan %d", detChan);
        pslLogError("psl__SetMCABinWidth", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the dynamic range composite value
 *
 * The Dynamic Range acquisition value is the energy of an x-ray that would
 * generate a pulse that spans 40% of the full-scale ADC input (1638 codes).
 */
PSL_STATIC int psl__SetDynamicRng(int detChan, int modChan, char *name, void *value,
                                  char *detType, XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs)
{
    int status;

    UNUSED(fs);
    UNUSED(detType);
    UNUSED(name);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);
    ASSERT(m != NULL);
    ASSERT(det != NULL);


    status = pslSetDefault("dynamic_range", value, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain for detChan %d", detChan);
        pslLogError("psl__SetDynamicRng", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Set the preamplifier gain
 */
PSL_STATIC int psl__SetPreampGain(int detChan, int modChan, char *name, void *value,
                                  char *detType, XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs)
{
    int status;

    double preampGain = 0.0;

    UNUSED(name);
    UNUSED(fs);
    UNUSED(detType);
    UNUSED(detType);

    ASSERT(value != NULL);
    ASSERT(det != NULL);
    ASSERT(defs != NULL);
    ASSERT(m != NULL);

    preampGain = *((double *)value);

    /* Update the Detector configuration */
    det->gain[m->detector_chan[modChan]] = preampGain;

    /* The det->gain value is used for updating the gain */
    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain while setting preamplifier gain "
                "for detChan %d", detChan);
        pslLogError("psl__SetPreampGain", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Synchronizes the preamplifier gain in the Detector configuration
 * with the preamp_gain acquisition value.
 *
 * Handel assumes that the preamplifier gain specified in the Detector
 * configuration is correct and uses it to set the the acquisition value
 * preamp_gain.
 *
 * This routine does not cause the gain to be recalculated.
 */
PSL_STATIC int psl__SynchPreampGain(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs)
{
    int status;

    double preampGain = 0.0;

    UNUSED(m);


    ASSERT(det != NULL);
    ASSERT(defs != NULL);


    preampGain = det->gain[det_chan];

    status = pslSetDefault("preamp_gain", (void *)&preampGain, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error synchronizing preamplifier gain for detChan %d",
                detChan);
        pslLogError("psl__SynchPreampGain", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Set the number of MCA channels.
 */
PSL_STATIC int psl__SetNumMCAChans(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m, Detector *det,
                                   FirmwareSet *fs)
{
    int status;

    parameter_t MCALIMLO = 0;
    parameter_t MCALIMHI = 0;

    double *mcaChans;
    int nMCAChans;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(detType);


    ASSERT(value != NULL);

    mcaChans = (double *)value;
    nMCAChans = (int) *mcaChans;

    if ((nMCAChans > MAX_MCA_CHANNELS) || (nMCAChans < MIN_MCA_CHANNELS)) {
        nMCAChans = (nMCAChans > MAX_MCA_CHANNELS) ? MAX_MCA_CHANNELS
                    : MIN_MCA_CHANNELS;
        sprintf(info_string, "The number of MCA channels specified by the user "
                "'%f' is not in the allowed range (%d, %d) for detChan %d, "
                "it was reset to %d", *mcaChans, MIN_MCA_CHANNELS,
                MAX_MCA_CHANNELS, detChan, nMCAChans);
        pslLogWarning("psl__SetNumMCAChans", info_string);
    }

    /* Only allow multiples of STJ_MEMORY_BLOCK_SIZE */
    if (nMCAChans % STJ_MEMORY_BLOCK_SIZE != 0) {
        nMCAChans -= nMCAChans % STJ_MEMORY_BLOCK_SIZE;
        sprintf(info_string, "The number of MCA channels specified by the user "
                "'%f' is not a multiple of %d for detChan %d, it was reset to %d",
                *mcaChans, STJ_MEMORY_BLOCK_SIZE, detChan, nMCAChans);
        pslLogWarning("psl__SetNumMCAChans", info_string);
    }

    /* In case mcaChans was updated, the value is passed back here */
    *mcaChans = (double) nMCAChans;

    status = pslGetParameter(detChan, "MCALIMLO", &MCALIMLO);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting MCA low limit while setting the number "
                "of MCA channels for detChan %d", detChan);
        pslLogError("psl__SetNumMCAChans", info_string, status);
        return status;
    }

    /* By convention, we always have an extra channel in the spectrum. That is
    * why there is no "- 1" in the following equation:
    */
    MCALIMHI  = (parameter_t)(MCALIMLO + nMCAChans);

    status = pslSetParameter(detChan, "MCALIMHI", MCALIMHI);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting upper MCA limit while setting the number "
                "of MCA channels for detChan %d", detChan);
        pslLogError("psl__SetNumMCAChans", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPolarity(int detChan, int modChan, char *name, void *value,
                                char *detType, XiaDefaults *defs, Module *m,
                                Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t POLARITY = 0;

    UNUSED(name);
    UNUSED(defs);
    UNUSED(fs);
    UNUSED(detType);


    ASSERT(value != NULL);


    POLARITY = (parameter_t)(*((double *)value));

    if ((POLARITY != 1) && (POLARITY != 0)) {
        sprintf(info_string, "User specified polarity '%hu' is not within the "
                "valid range (0,1) for detChan %d", POLARITY, detChan);
        pslLogError("psl__SetPolarity", info_string, XIA_POL_OOR);
        return XIA_POL_OOR;
    }

    sprintf(info_string, "Setting the POLARITY = %d for detChan %d", POLARITY, detChan);
    pslLogInfo("psl__SetPolarity", info_string);

    status = pslSetParameter(detChan, "POLARITY", POLARITY);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the polarity for detChan %d", detChan);
        pslLogError("psl__SetPolarity", info_string, status);
        return status;
    }

    /* Update the Detector configuration */
    det->polarity[m->detector_chan[modChan]] = POLARITY;

    return XIA_SUCCESS;
}


/*
 * Synchronize the detector polarity in the Detector configuration
 * with the detector_polarity acquisition value.
 *
 * Handel assumes that the detector polarity specified in the Detector
 * configuration is correct and uses it to set the acquisition value
 * detector_polarity.
 */
PSL_STATIC int psl__SynchPolarity(int detChan, int det_chan, Module *m,
                                  Detector *det, XiaDefaults *defs)
{
    int status;

    double pol = 0.0;

    UNUSED(m);


    ASSERT(det != NULL);
    ASSERT(defs != NULL);


    pol = (double)(det->polarity[det_chan]);

    status = pslSetDefault("detector_polarity", (void *)&pol, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error synchronizing detector polarity for detChan %d",
                detChan);
        pslLogError("psl__SynchPolarity", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the hardware clock tick.
 *
 * This value may be based on a DSP parameter, or it may simply come from
 * a constant in psl_stj.h.
 *
 * The returned value is in seconds.
 */
PSL_STATIC double psl__GetClockTick(void)
{
    return 1.0 / DEFAULT_CLOCK_SPEED;
}


/*
 * Set the reset delay interval.
 */
PSL_STATIC int psl__SetResetDelay(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs)
{
    int status;

    double tick = 0.0;

    double *resetDelay = NULL;

    parameter_t RESETINT = 0;

    UNUSED(name);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(fs);
    UNUSED(detType);


    ASSERT(value != NULL);
    ASSERT(det != NULL);


    /* Since this routine can be called (and will) be called for all modules
    * and configurations, we'll want to skip this step if the detector is not
    * reset-type.
    */
    if (det->type != XIA_DET_RESET) {
        sprintf(info_string, "Skipping setting reset delay: detChan %d is not a "
                "reset-type detector", detChan);
        pslLogInfo("psl__SetResetDelay", info_string);
        return XIA_SUCCESS;
    }

    /* This is in microseconds and it needs to be converted to seconds */
    resetDelay = (double *)value;

    /* Update the Detector configuration */
    det->typeValue[m->detector_chan[modChan]] = *resetDelay;

    *resetDelay = *resetDelay / 1.0e6;

    /* XXX Check limits here */


    tick = psl__GetClockTick();

    RESETINT = (parameter_t)ROUND(*resetDelay / tick);

    status = pslSetParameter(detChan, "RESETINT", RESETINT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting reset delay to %f microseconds for "
                "detChan %d", *resetDelay, detChan);
        pslLogError("psl__SetResetDelay", info_string, status);
        return status;
    }

    /* We have to refresh the value the user passed in since it may be different
    * due to the rounding that was necessary to convert the value to a DSP
    * parameter.
    */
    *resetDelay = (double)RESETINT * tick * 1.0e6;

    return XIA_SUCCESS;
}


/*
 * Synchronize the detector reset delay in the Detector configuration
 * with the reset_delay acquisition value.
 *
 * Handel assumes that the detector reset delay specified in the Detector
 * configuration is correct and uses it to set the acquisition value
 * reset_delay.
 */
PSL_STATIC int psl__SynchResetDelay(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs)
{
    int status;

    double resetDelay = 0.0;

    UNUSED(m);


    ASSERT(det != NULL);


    /* Since this routine can be called (and will) be called for all modules
    * and configurations, we'll want to skip this step if the detector is not
    * reset-type.
    */
    if (det->type != XIA_DET_RESET) {
        sprintf(info_string, "Skipping reset delay synch: detChan %d is not a "
                "reset-type detector", detChan);
        pslLogInfo("psl__SynchResetDelay", info_string);
        return XIA_SUCCESS;
    }

    resetDelay = det->typeValue[det_chan];

    status = pslSetDefault("reset_delay", (void *)&resetDelay, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error synchronizing the reset delay for detChan %d",
                detChan);
        pslLogError("psl__SynchResetDelay", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the runtime for the specified channel.
 */
PSL_STATIC int psl__GetRealtime(int detChan, void *value, XiaDefaults *defs,
                                Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);


    ASSERT(m != NULL);
    ASSERT(value != NULL);


    status = pslGetModChan(detChan, m, &modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting modChan for module '%s' from detChan %d",
                m->alias, detChan);
        pslLogError("psl__GetRealtime", info_string, status);
        return status;
    }

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d", detChan);
        pslLogError("psl__GetRealtime", info_string, status);
        return status;
    }

    status = psl__ExtractRealtime(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting energy livetime for detChan %d",
                detChan);
        pslLogError("psl__GetRealtime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the events in run for the specified channel.
 *
 * This only returns the lower 32-bits of the events in run. For the complete
 * 64-bit value, see psl__GetModuleStatistics().
 */
PSL_STATIC int psl__GetTotalEvents(int detChan, void *value, XiaDefaults *defs,
                                   Module *m)
{
    int status;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    double mcaEvts;
    double unders;
    double overs;

    unsigned int modChan;

    UNUSED(defs);


    ASSERT(value);
    ASSERT(m);


    status = pslGetModChan(detChan, m, &modChan);
    /* Impossible for this to fail in a system properly configured by Handel. */
    ASSERT(status == XIA_SUCCESS);

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d.",
                detChan);
        pslLogError("psl__GetTotalEvents", info_string, status);
        return status;
    }

    status = psl__ExtractEvents(modChan, stats, &mcaEvts);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the MCA event count from the "
                "statistics block for detChan %d.", detChan);
        pslLogError("psl__GetTotalEvents", info_string, status);
        return status;
    }

    status = psl__ExtractUnderflows(modChan, stats, &unders);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the underflow event count from the "
                "statistics block for detChan %d.", detChan);
        pslLogError("psl__GetTotalEvents", info_string, status);
        return status;
    }

    status = psl__ExtractOverflows(modChan, stats, &overs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the overflow event count from the "
                "statistics block for detChan %d.", detChan);
        pslLogError("psl__GetTotalEvents", info_string, status);
        return status;
    }

    /* This routine explicitly returns a 32-bit integral value. */
    *((unsigned long *)value) = (unsigned long)(mcaEvts + unders + overs);

    return XIA_SUCCESS;
}


/*
 * Get the trigger livetime for the specified channel.
 */
PSL_STATIC int psl__GetTLivetime(int detChan, void *value, XiaDefaults *defs,
                                 Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);


    ASSERT(value);
    ASSERT(m);


    status = pslGetModChan(detChan, m, &modChan);
    /* Impossible for this to fail in a system properly configured by Handel. */
    ASSERT(status == XIA_SUCCESS);

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d.",
                detChan);
        pslLogError("psl__GetTLivetime", info_string, status);
        return status;
    }

    status = psl__ExtractTLivetime(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the trigger livetime from "
                "the statistics block for detChan %d.", detChan);
        pslLogError("psl__GetTLivetime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the input count rate for the specified channel.
 */
PSL_STATIC int psl__GetICR(int detChan, void *value, XiaDefaults *defs,
                           Module *m)
{
    int status;

    double tlt;
    double trigs;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);


    ASSERT(value);
    ASSERT(m);


    status = pslGetModChan(detChan, m, &modChan);
    /* Impossible for this to fail in a system properly configured by Handel. */
    ASSERT(status == XIA_SUCCESS);

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d.",
                detChan);
        pslLogError("psl__GetICR", info_string, status);
        return status;
    }

    status = psl__ExtractTLivetime(modChan, stats, &tlt);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the trigger livetime from "
                "the statistics block for detChan %d.", detChan);
        pslLogError("psl__GetICR", info_string, status);
        return status;
    }

    status = psl__ExtractTriggers(modChan, stats, &trigs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the trigger count from the "
                "statistics block for detChan %d.", detChan);
        pslLogError("psl__GetICR", info_string, status);
        return status;
    }

    if (tlt > 0.0) {
        *((double *)value) = trigs / tlt;

    } else {
        *((double *)value)= 0.0;
    }

    return XIA_SUCCESS;
}


/*
 * Get the output count rate for the specified channel.
 */
PSL_STATIC int psl__GetOCR(int detChan, void *value, XiaDefaults *defs,
                           Module *m)
{
    int status;

    double rt;
    double mcaEvts;
    double unders;
    double overs;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);


    ASSERT(value);
    ASSERT(m);


    status = pslGetModChan(detChan, m, &modChan);
    /* Impossible for this to fail in a system properly configured by Handel. */
    ASSERT(status == XIA_SUCCESS);

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d.",
                detChan);
        pslLogError("psl__GetOCR", info_string, status);
        return status;
    }

    status = psl__ExtractRealtime(modChan, stats, &rt);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the realtime from "
                "the statistics block for detChan %d.", detChan);
        pslLogError("psl__GetOCR", info_string, status);
        return status;
    }

    status = psl__ExtractEvents(modChan, stats, &mcaEvts);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the MCA event count from the "
                "statistics block for detChan %d.", detChan);
        pslLogError("psl__GetOCR", info_string, status);
        return status;
    }

    status = psl__ExtractUnderflows(modChan, stats, &unders);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the underflow event count from "
                "the statistics block for detChan %d.", detChan);
        pslLogError("psl__GetOCR", info_string, status);
        return status;
    }

    status = psl__ExtractOverflows(modChan, stats, &overs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the overflow event count from "
                "the statistics block for detChan %d.", detChan);
        pslLogError("psl__GetOCR", info_string, status);
        return status;
    }

    if (rt > 0.0) {
        *((double *)value) = (mcaEvts + unders + overs) / rt;

    } else {
        *((double *)value)= 0.0;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the peaking time.
 *
 * Currently, the Stj driver only supports FDD files for firmware. This routine
 * returns an error if no FDD file is defined.
 */
PSL_STATIC int psl__SetPeakingTime(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    double pt   = 0.0;
    double tick = psl__GetClockTick();

    char fippi[MAX_PATH_LEN];
    char rawFippi[MAXFILENAME_LEN];

    parameter_t SLOWLEN    = 0;
    parameter_t DECIMATION = 0;

    UNUSED(name);


    ASSERT(value != NULL);
    ASSERT(det != NULL);
    ASSERT(fs != NULL);
    ASSERT(m != NULL);

    pt = *((double *)value);

    /* The peaking time only needs to be set once per module
     * To avoid redundant calls, check the current gain and don't reset peaking
     * time if it is already set
     */
    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting decimation for detChan %d", detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting slow filter length for detChan %d", detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    if (pt == (double)(SLOWLEN * tick * pow(2.0, (double)DECIMATION)) * 1.0e6) {
        sprintf(info_string, "Peaking time %0.2f is already set on detChan %d",
                pt, detChan);
        pslLogDebug("psl__SetPeakingTime", info_string);
        return XIA_SUCCESS;
    }

    sprintf(info_string, "Setting peaking time = %0.2f for "
            "detChan %d", pt, detChan);
    pslLogDebug("psl__SetPeakingTime", info_string);

    /* The peaking time is validated relative to the defined peaking time
    * ranges in the FDD file.
    */
    status = psl__GetFiPPIName(modChan, pt, fs, detType, fippi, rawFippi);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting FiPPI name at peaking time %0.2f "
                "for detChan = %d", pt, detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    sprintf(info_string, "Preparing to download FiPPI A '%s' to detChan %d",
            rawFippi, detChan);
    pslLogDebug("psl__SetPeakingTime", info_string);

    status = pslDownloadFirmware(detChan, "fippi_a", fippi, m,
                                 rawFippi, NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error downloading FiPPI A '%s' to detChan %d",
                fippi, detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    status = psl__UpdateFilterParams(detChan, modChan, pt, defs, fs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating filter parameters for detChan %d",
                detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    sprintf(info_string, "Filter update complete for peaking time = %0.2f for "
            "detChan %d", pt, detChan);
    pslLogDebug("psl__SetPeakingTime", info_string);

    /* Re-calculate actual peaking time */
    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting slow filter length for detChan %d",
                detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting decimation for detChan %d", detChan);
        pslLogError("psl__SetPeakingTime", info_string, status);
        return status;
    }

    /* Scale this back to microseconds */
    *((double *)value) =
        (double)(SLOWLEN * tick * pow(2.0, (double)DECIMATION)) * 1.0e6;

    return XIA_SUCCESS;
}


/*
 * Get the correct FiPPI file name for specified module channel and
 * peaking time.
 *
 * Currently, the Stj driver only supports FDD files. An error is
 * returned if the Firmware Set does not define an FDD filename.
 *
 * The caller is responsible for allocating memory for name. A good size
 * to use is MAXFILENAME_LEN.
 */
PSL_STATIC int psl__GetFiPPIName(int modChan, double pt, FirmwareSet *fs,
                                 char *detType, char *name, char *rawName)
{
    int status;

    ASSERT(fs != NULL);
    ASSERT(detType != NULL);
    ASSERT(name != NULL);
    ASSERT(rawName != NULL);

    UNUSED(modChan);

    /* Even though the modChan should be used to determine which FiPPI to retrieve,
    * we only support FiPPI A currently.
    */

    status = xiaFddGetAndCacheFirmware(fs, "fippi_a", pt,
                                       detType, name, rawName);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting FiPPI A filename from '%s' with a "
                "peaking time of %0.2f microseconds", fs->filename, pt);
        pslLogError("psl__GetFiPPIName", info_string, status);
        return status;
    }

    sprintf(info_string, "FiPPI %s found for peaking time %0.2f", rawName, pt);
    pslLogInfo("psl__GetFiPPIName", info_string);
    return XIA_SUCCESS;
}


/*
 * Download FiPPI A to the hardware.
 *
 * Only downloads the requested firmware if it doesn't show that the board
 * is running it.
 */
PSL_STATIC int psl__DownloadFiPPIA(int detChan, char *file, char *rawFile,
                                   Module *m)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int modChan = 0;


    ASSERT(file    != NULL);
    ASSERT(rawFile != NULL);
    ASSERT(m       != NULL);

    status = pslGetModChan(detChan, m, &modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting module channel for detChan = %d",
                detChan);
        pslLogError("psl__DownloadFiPPIA", info_string, status);
        return status;
    }

    if (STREQ(rawFile, m->currentFirmware[modChan].currentFiPPI)) {
        sprintf(info_string, "Requested FiPPI '%s' is already running on detChan %d",
                file, detChan);
        pslLogInfo("psl__DownloadFiPPIA", info_string);
        return XIA_SUCCESS;
    }

    statusX = dxp_replace_fpgaconfig(&detChan, "a_and_b", file);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error switching to new FiPPI '%s' for detChan %d",
                file, detChan);
        pslLogError("psl__DownloadFiPPIA", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Since we just downloaded the FiPPI for all 32 channels, set the current
    * firmware for all 32 channels to the new file name. This prevents Handel
    * from thinking that it needs to download the firmware 32 times. When we add
    * support for FiPPI B, this will be reduced to the 2 channels covered by
    * FiPPI A.
    */
    for (i = 0; i < m->number_of_channels; i++) {
        strcpy(m->currentFirmware[i].currentFiPPI, rawFile);
    }

    return XIA_SUCCESS;
}


/*
 * Updates the filter parameters based on the new peaking time.
 *
 * Assumes that an FDD file is being used. It is an unchecked exception to
 * pass in a firmware set that doesn't use one.
 */
PSL_STATIC int psl__UpdateFilterParams(int detChan, int modChan, double pt,
                                       XiaDefaults *defs, FirmwareSet *fs,
                                       Module *m, Detector *det)
{
    int status;
    int i;

    unsigned short nFilter = 0;

    parameter_t DECIMATION = 0;
    parameter_t SLOWLEN    = 0;
    parameter_t SLOWGAP    = 0;
    parameter_t PEAKINT    = 0;
    parameter_t PEAKSAM    = 0;
    parameter_t PEAKMODE   = 0;

    double tick        = psl__GetClockTick();
    double sl          = 0.0;
    double sg          = 0.0;
    double gapTime     = 0.0;
    double psOffset    = 0.0;
    double piOffset    = 0.0;
    double gapMinAtDec = 0.0;
    double peakMode    = 0.0;

    Firmware *current = NULL;
    parameter_t filter[2];

    char psStr[20];
    char piStr[22];

    ASSERT(fs != NULL);
    ASSERT(fs->filename != NULL);

    UNUSED(modChan);
    UNUSED(m);
    UNUSED(det);

    /* Fill the filter information in here using the FirmwareSet */
    current = fs->firmware;

    while (current != NULL) {
        if ((pt >= current->min_ptime) && (pt <= current->max_ptime)) {

            nFilter = current->numFilter;

            sprintf(info_string, "Filter info nfilter = %d for peaking time (%0.2f, "
                    "%0.2f)", nFilter, current->min_ptime, current->max_ptime);
            pslLogDebug("psl__UpdateFilterParams", info_string);

            for (i = 0; i < current->numFilter; i++) {
                filter[i] = current->filterInfo[i];
            }
            break;
        }
        current = getListNext(current);
    }

    if (nFilter != 2) {
        sprintf(info_string, "Number of filter parameters (%hu) in '%s' does "
                "not match the number required for the Stj hardware (%d).", nFilter,
                fs->filename, 2);
        pslLogError("psl__UpdateFilterParams", info_string, XIA_N_FILTER_BAD);
        return XIA_N_FILTER_BAD;
    }

    /* Calculate SLOWLEN */
    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting decimation for slow filter length "
                "calculation for detChan %d", detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    /* Scale tick to microseconds */
    tick *= 1.0e6;

    sprintf(info_string, "DECIMATION = %hu, tick = %0.6f, pt = %0.2f",
            DECIMATION, tick, pt);
    pslLogDebug("psl__UpdateFilterParams", info_string);

    sl = pt / (tick * pow(2.0, (double)DECIMATION));
    SLOWLEN = (parameter_t)ROUND(sl);

    if (SLOWLEN < MIN_SLOWLEN || SLOWLEN > MAX_SLOWLEN) {
        sprintf(info_string, "Calculated slow filter length (%hu) is not in the "
                "allowed range (%d, %d) for detChan %d", SLOWLEN, MIN_SLOWLEN,
                MAX_SLOWLEN, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, XIA_SLOWLEN_OOR);
        return XIA_SLOWLEN_OOR;
    }

    /* Calculate SLOWGAP */
    status = pslGetDefault("minimum_gap_time", (void *)&gapTime, defs);
    ASSERT(status == XIA_SUCCESS);

    /* Remember, per #544, that the gap_time is the *minimum* gap time. At
    * decimations > 0, we'll probably end up with SLOWGAP = 3.
    */
    if (DECIMATION != 0) {
        gapMinAtDec = tick * pow(2.0, (double)DECIMATION) * 3.0;
        gapTime     = MAX(gapMinAtDec, gapTime);
    }

    sg = gapTime / (tick * pow(2.0, (double)DECIMATION));
    SLOWGAP = (parameter_t)ROUND(sg);

    sprintf(info_string, "Calculated SLOWGAP = %hu", SLOWGAP);
    pslLogDebug("psl__UpdateFilterParams", info_string);

    if (SLOWGAP > MAX_SLOWGAP) {
        sprintf(info_string, "Calculated slow filter gap length (%hu) is not in "
                "the allowed range(%d, %d) for detChan %d", SLOWGAP, MIN_SLOWGAP,
                MAX_SLOWGAP, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, XIA_SLOWGAP_OOR);
        return XIA_SLOWGAP_OOR;
    }

    if ((SLOWLEN + SLOWGAP) > MAX_SLOWFILTER) {
        sprintf(info_string, "Total slow filter length (%hu) is larger then the "
                "maximum allowed size (%d) for detChan %d",
                (unsigned short)SLOWLEN + SLOWGAP, MAX_SLOWFILTER, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, XIA_SLOWGAP_OOR);
        return XIA_SLOWGAP_OOR;
    }

    status = pslSetParameter(detChan, "SLOWLEN", SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting slow filter length to %hu for "
                "detChan %d", SLOWLEN, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "SLOWGAP", SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting slow filter gap to %hu for detChan %d",
                SLOWGAP, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    /* Calculate other filter parameters from the filter info in the FDD file.
    * For the Stj, we interpret the filter data as:
    *
    * filter[0] = PEAKINT offset
    * filter[1] = PEAKSAM offset
    */

    /* Use custom peak interval time if available. */
    sprintf(piStr, "peak_interval_offset%hu", DECIMATION);

    status = pslGetDefault(piStr, (void *)&piOffset, defs);

    if (status == XIA_SUCCESS) {
        PEAKINT = (parameter_t)(SLOWLEN + SLOWGAP +
                                (parameter_t)(piOffset / (tick * pow(2.0, DECIMATION))));

    } else {
        PEAKINT = (parameter_t)(SLOWLEN + SLOWGAP + filter[0]);
    }

    status = pslSetParameter(detChan, "PEAKINT", PEAKINT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting peak interval to %hu for detChan %d",
                PEAKINT, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    status = pslGetDefault("peak_mode", (void *)&peakMode, defs);
    ASSERT(status == XIA_SUCCESS);

    PEAKMODE = (parameter_t) peakMode;
    status = pslSetParameter(detChan, "PEAKMODE", PEAKMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting engery filter peak mode to %0.2f for "
                "detChan %d", peakMode, detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    /* No need to set PEAKSAM if PEAKMODE is XIA_PEAK_SENSING_MODE */
    if (PEAKMODE != XIA_PEAK_SENSING_MODE) {
        /* If the user has defined a custom peak sampling value at this decimation
        * then it will override the value from the FDD file.
        */
        sprintf(psStr, "peak_sample_offset%hu", DECIMATION);

        status = pslGetDefault(psStr, (void *)&psOffset, defs);

        if (status == XIA_SUCCESS) {
            PEAKSAM = (parameter_t)(SLOWLEN + SLOWGAP -
                                    (parameter_t)(psOffset / (tick * pow(2.0, DECIMATION))));
        } else {
            PEAKSAM = (parameter_t)(SLOWLEN + SLOWGAP - filter[1]);
        }

        status = pslSetParameter(detChan, "PEAKSAM", PEAKSAM);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting peak sample to %hu for detChan %d",
                    PEAKSAM, detChan);
            pslLogError("psl__UpdateFilterParams", info_string, status);
            return status;
        }
    }

    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain for detChan %d", detChan);
        pslLogError("psl__UpdateFilterParams", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Set the slow filter gap time.
 */
PSL_STATIC int psl__SetGapTime(int detChan, int modChan, char *name,
                               void *value, char *detType, XiaDefaults *defs,
                               Module *m, Detector *det, FirmwareSet *fs)
{
    UNUSED(detChan);
    UNUSED(modChan);
    UNUSED(name);
    UNUSED(value);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    return XIA_SUCCESS;
}


/*
 * Get the slow filter gap time.
 */
PSL_STATIC int psl__GetGapTime(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    parameter_t SLOWGAP    = 0;
    parameter_t DECIMATION = 0;

    double tick = psl__GetClockTick();

    UNUSED(defs);


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "SLOWGAP", &SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting slow filter gap time for detChan %d",
                detChan);
        pslLogError("psl__GetGapTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting decimation for detChan %d", detChan);
        pslLogError("psl__GetGapTime", info_string, status);
        return status;
    }

    /* Scale to microseconds */
    tick *= 1.0e6;

    *((double *)value) = tick * (double)SLOWGAP * pow(2.0, (double)DECIMATION);

    return XIA_SUCCESS;
}


/*
 * Set the trigger filter peaking time
 */
PSL_STATIC int psl__SetTrigPeakingTime(int detChan, int modChan, char *name, void *value,
                                       char *detType, XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs)
{
    int status;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    status = pslSetDefault("trigger_peaking_time", value, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateTrigFilterParams(detChan, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating trigger filter parameters for detChan %d",
                detChan);
        pslLogError("psl__SetTrigPeakingTime", info_string, status);
        return status;
    }

    /* Updating FASTLEN should trigger a recalculation of gain and THRESHOLD */
    status = psl__UpdateGain(detChan, modChan, defs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating gain for detChan %d", detChan);
        pslLogError("psl__SetTrigPeakingTime", info_string, status);
        return status;
    }

    /* The peaking time may have changed, so update it for the user here. */
    status = pslGetDefault("trigger_peaking_time", value, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


/*
 * Update the trigger filter parameters.
 */
PSL_STATIC int psl__UpdateTrigFilterParams(int detChan, XiaDefaults *defs)
{
    int status;

    double trigPT = 0.0;
    double trigGT = 0.0;
    double tick   = psl__GetClockTick();
    double fl     = 0.0;
    double fg     = 0.0;

    parameter_t FASTLEN = 0;
    parameter_t FASTGAP = 0;

    ASSERT(defs != NULL);


    status = pslGetDefault("trigger_peaking_time", (void *)&trigPT, defs);
    ASSERT(status == XIA_SUCCESS);
    status = pslGetDefault("trigger_gap_time", (void *)&trigGT, defs);
    ASSERT(status == XIA_SUCCESS);

    /* Scale tick to microseconds */
    tick *= 1.0e6;

    fl = trigPT / tick;
    FASTLEN = (parameter_t)ROUND(fl);

    if (FASTLEN < MIN_FASTLEN || FASTLEN > MAX_FASTLEN) {
        sprintf(info_string, "Calculated trigger filter length (%hu) is not in the "
                "allowed range (%d, %d) for detChan %d", FASTLEN, MIN_FASTLEN,
                MAX_FASTLEN, detChan);
        pslLogError("psl__UpdateTrigFilterParams", info_string, XIA_FASTLEN_OOR);
        return XIA_FASTLEN_OOR;
    }

    fg = trigGT / tick;
    FASTGAP = (parameter_t)ROUND(fg);

    sprintf(info_string, "trigGT = %0.2f, fg = %0.2f, FASTGAP = %hu", trigGT, fg,
            FASTGAP);
    pslLogDebug("psl__UpdateTrigFilterParams", info_string);

    /* Don't worry too much about the limits on this. Just make sure that it
    * works with FASTLEN.
    */
    if ((FASTLEN + FASTGAP) > MAX_FASTFILTER) {
        sprintf(info_string, "Total fast filter length (%hu) is larger then the "
                "maximum allowed size (%d) for detChan %d",
                (unsigned short)FASTLEN + FASTGAP, MAX_FASTFILTER, detChan);
        pslLogWarning("psl__UpdateTrigFilterParams", info_string);

        FASTGAP = (parameter_t)(MAX_FASTFILTER - FASTLEN);
        ASSERT(FASTGAP >= MIN_FASTGAP);

        sprintf(info_string, "Recalculated fast filter gap is %hu for detChan %d",
                FASTGAP, detChan);
        pslLogInfo("psl__UpdateTrigFilterParams", info_string);
    }

    status = pslSetParameter(detChan, "FASTLEN", FASTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting fast filter length for detChan %d",
                detChan);
        pslLogError("psl__UpdateTrigFilterParams", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "FASTGAP", FASTGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting fast filter gap for detChan %d",
                detChan);
        pslLogError("psl__UpdateTrigFilterParams", info_string, status);
        return status;
    }

    sprintf(info_string, "Set FASTLEN = %hu, FASTGAP = %hu for channel %d",
            FASTLEN, FASTGAP, detChan);
    pslLogDebug("psl__UpdateTrigFilterParams", info_string);

    /* Recompute acquisition values based on -- potentially -- rounded DSP
    * parameter values.
    */
    fl = (double)FASTLEN * tick;
    status = pslSetDefault("trigger_peaking_time", (void *)&fl, defs);
    ASSERT(status == XIA_SUCCESS);

    fg = (double)FASTGAP * tick;
    status = pslSetDefault("trigger_gap_time", (void *)&fg, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


/*
 * Sets the trigger filter gap time.
 */
PSL_STATIC int psl__SetTrigGapTime(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    status = pslSetDefault("trigger_gap_time", value, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateTrigFilterParams(detChan, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating trigger filter parameters for detChan %d",
                detChan);
        pslLogError("psl__SetTrigGapTime", info_string, status);
        return status;
    }

    /* The gap time may have changed, so update it for the user here. */
    status = pslGetDefault("trigger_gap_time", value, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


/*
 * Do a generic trace run.
 */
PSL_STATIC int psl__DoTrace(int detChan, short type, double *info)
{
    int statusX;

    short task = STJ_CT_ADC;
    unsigned int infoLen = 3;

    double tick = psl__GetClockTick();

    int intInfo[3];

    ASSERT(info != NULL);

    intInfo[0] = (int)info[0];
    /* The trace interval is passed in as nanoseconds, so it must be scaled to
    * seconds.
    */
    intInfo[1] = (int)ROUND(((info[1] * 1.0e-9) / tick) - 1.0);
    intInfo[2] = (int)type;

    /* Due to the rounding, the trace interval passed in by the user may
    * be slightly different then the actual value written to the DSP. We calculate
    * what the actual value is here and pass it back to the user.
    */
    info[1] = ((double)intInfo[1] + 1.0) * tick;

    sprintf(info_string, "Staring trace run type %d on detChan %d trace wait %d",
            intInfo[2], detChan, intInfo[1]);
    pslLogInfo("psl__DoTrace", info_string);

    statusX = dxp_start_control_task(&detChan, &task, &infoLen, &intInfo[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error starting control task %hd for detChan %d",
                type, detChan);
        pslLogError("psl__DoTrace", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets all of the DSP parameter values for the specified channel.
 */
PSL_STATIC int psl__GetParamValues(int detChan, void *value)
{
    int statusX;


    ASSERT(value != NULL);


    statusX = dxp_readout_detector_run(&detChan, (unsigned short *)value, NULL,
                                       NULL);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting DSP parameter values for detChan %d",
                detChan);
        pslLogError("psl__GetParamValues", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Set the preset run type.
 *
 * The allowed preset run types are defined in handel_constants.h.
 */
PSL_STATIC int psl__SetPresetType(int detChan, int modChan, char *name, void *value,
                                  char *detType, XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs)
{
    int status;

    double presetType = 0.0;

    parameter_t PRESETTYPE = 0;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);


    presetType = *((double *)value);

    /* The constants stored in handel_constants.h also happen to map directly
     * to PRESETTYPE as currently defined.
     */
    if (presetType != XIA_PRESET_NONE &&
            presetType != XIA_PRESET_FIXED_REAL &&
            presetType != XIA_PRESET_FIXED_LIVE &&
            presetType != XIA_PRESET_FIXED_EVENTS &&
            presetType != XIA_PRESET_FIXED_TRIGGERS) {

        sprintf(info_string, "Invalid preset run type specified: %0.1f",
                presetType);
        pslLogError("psl__SetPresetType", info_string, XIA_UNKNOWN_PRESET);
        return XIA_UNKNOWN_PRESET;
    }


    PRESETTYPE = (parameter_t)presetType;

    status = pslSetParameter(detChan, "PRESETTYPE", PRESETTYPE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PRESETTYPE to %hu for detChan %d",
                PRESETTYPE, detChan);
        pslLogError("psl__SetPresetType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Set the preset run value.
 *
 * This value is interpreted differently depending on the preset run type,
 * which means that this value must be set *after* setting the preset type.
 *
 * For fixed realtime/livetime: Specify in seconds.
 * For count-based runs: Specify as counts.
 */
PSL_STATIC int psl__SetPresetValue(int detChan, int modChan, char *name, void *value,
                                   char *detType, XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    double v    = 0.0;
    double len  = 0.0;
    double tick = psl__GetClockTick();

    unsigned long loLen = 0;
    unsigned long hiLen = 0;

    parameter_t PRESETTYPE = 0;
    parameter_t PRESETLEN  = 0;
    parameter_t PRESETLENA = 0;
    parameter_t PRESETLENB = 0;
    parameter_t PRESETLENC = 0;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "PRESETTYPE", &PRESETTYPE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting current preset type for detChan %d",
                detChan);
        pslLogError("psl__SetPresetValue", info_string, status);
        return status;
    }

    v = *((double *)value);

    switch (PRESETTYPE) {

    case 0:
        /* Ignore since this is an indefinite run. */
        return XIA_SUCCESS;

    case 1:
    case 2:
        len = v / (tick * 16.0);
        break;

    case 3:
    case 4:
        len = v;
        break;

    default:
        /* It should be impossible for PRESETTYPE to be out-of-range. */
        FAIL();
        break;
    }

    hiLen = (unsigned long)floor(len / ldexp(1.0, 32));
    loLen = (unsigned long)ROUND(len - ((double)hiLen * ldexp(1.0, 32)));

    sprintf(info_string, "len = %0.0f, hiLen = %#lx, loLen = %#lx",
            len, hiLen, loLen);
    pslLogDebug("psl__SetPresetValue", info_string);

    PRESETLEN  = (parameter_t)(loLen & 0xFFFF);
    PRESETLENA = (parameter_t)(loLen >> 16);
    PRESETLENB = (parameter_t)(hiLen & 0xFFFF);
    PRESETLENC = (parameter_t)(hiLen >> 16);

    status = pslSetParameter(detChan, "PRESETLEN", PRESETLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the preset run length for detChan %d",
                detChan);
        pslLogError("psl__SetPresetValue", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "PRESETLENA", PRESETLENA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the preset run length for detChan %d",
                detChan);
        pslLogError("psl__SetPresetValue", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "PRESETLENB", PRESETLENB);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the preset run length for detChan %d",
                detChan);
        pslLogError("psl__SetPresetValue", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "PRESETLENC", PRESETLENC);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the preset run length for detChan %d",
                detChan);
        pslLogError("psl__SetPresetValue", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the run active status for the hardware.
 */
PSL_STATIC int psl__GetRunActive(int detChan, void *value, XiaDefaults *defs,
                                 Module *m)
{
    int statusX;
    int active;

    UNUSED(m);
    UNUSED(defs);


    ASSERT(value != NULL);

    statusX = dxp_isrunning(&detChan, &active);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting run status for detChan %d", detChan);
        pslLogError("psl__GetRunActive", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((unsigned long *)value) = (unsigned long)active;


    return XIA_SUCCESS;
}

/*
 * Checks to see if Buffer A is full.
 *
 * Requires the mapping mode firmware to be running.
 */
PSL_STATIC int psl__GetBufferFullA(int detChan, void *value, XiaDefaults *defs,
                                   Module *m)
{
    int status;

    boolean_t isFull = FALSE_;

    UNUSED(defs);
    UNUSED(m);


    status = psl__GetBufferFull(detChan, 'a', &isFull);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting status of Buffer A for detChan %d",
                detChan);
        pslLogError("psl__GetBufferFullA", info_string, status);
        return status;
    }

    *((unsigned short *)value) = (unsigned short)isFull;

    return XIA_SUCCESS;
}


/*
 * Checks to see if Buffer B is full.
 *
 * Requires the mapping mode firmware to be running.
 */
PSL_STATIC int psl__GetBufferFullB(int detChan, void *value, XiaDefaults *defs,
                                   Module *m)
{
    int status;

    boolean_t isFull = FALSE_;

    UNUSED(defs);
    UNUSED(m);


    status = psl__GetBufferFull(detChan, 'b', &isFull);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting status of Buffer B for detChan %d",
                detChan);
        pslLogError("psl__GetBufferFullB", info_string, status);
        return status;
    }

    *((unsigned short *)value) = (unsigned short)isFull;

    return XIA_SUCCESS;
}


/*
 * Checks to see if the specified buffer is full or not.
 *
 * Requires the mapping mode firmware to be running.
 */
PSL_STATIC int psl__GetBufferFull(int detChan, char buf, boolean_t *is_full)
{
    int statusX;
    int status;

    unsigned long fullMask = 0;
    unsigned long mfr = 0;

    boolean_t is_mapping = FALSE_;


    ASSERT(buf == 'a' || buf == 'b');
    ASSERT(is_full != NULL);


    status = psl__IsMapping(detChan, MAPPING_ANY, &is_mapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error determining if mapping mode was enabled for "
                "detChan %d", detChan);
        pslLogError("psl__GetBufferFull", info_string, status);
        return status;
    }

    if (!is_mapping) {
        sprintf(info_string, "Mapping mode firmware is currently not running on "
                "detChan %d", detChan);
        pslLogError("psl__GetBufferFull", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    statusX = dxp_read_register(&detChan, "MFR", &mfr);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading buffer '%c' status for detChan %d",
                buf, detChan);
        pslLogError("psl__GetBufferFull", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    switch (buf) {
    case 'a':
        fullMask = 0x2;
        break;
    case 'b':
        fullMask = 0x20;
        break;
    default:
        break;
    }

    *is_full = (boolean_t)((mfr & fullMask) ? TRUE_ : FALSE_);

    return XIA_SUCCESS;
}


/*
 * Queries board to see if it is running in mapping mode or not.
 */
PSL_STATIC int psl__IsMapping(int detChan, unsigned short allowed,
                              boolean_t *isMapping)
{
    parameter_t MAPPINGMODE;
    int status;


    status = pslGetParameter(detChan, "MAPPINGMODE", &MAPPINGMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading MAPPINGMODE for detChan %d",
                detChan);
        pslLogError("psl__IsMapping", info_string, status);
        return status;
    }

    switch (MAPPINGMODE) {
    case MAPPINGMODE_NIL:
        *isMapping = FALSE_;
        break;

    case MAPPINGMODE_MCA:
        *isMapping = (boolean_t)((allowed & MAPPING_MCA) > 0);
        break;

    case MAPPINGMODE_SCA:
        *isMapping = (boolean_t)((allowed & MAPPING_SCA) > 0);
        break;

    case MAPPINGMODE_LIST:
        *isMapping = (boolean_t)((allowed & MAPPING_LIST) > 0);
        break;

    default:
        sprintf(info_string, "MAPPINGMODE %hu for detChan %d is invalid",
                MAPPINGMODE, detChan);
        pslLogError("psl__IsMapping", info_string, XIA_UNKNOWN_MAPPING);
        return XIA_UNKNOWN_MAPPING;

        break;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the total number of scan points when the hardware is run
 * in mapping mode.
 *
 * This parameter is skipped if mapping mode is not currently active.
 *
 * Setting the number of mapping points to 0.0 causes the mapping run to
 * continue indefinitely.
 */
PSL_STATIC int psl__SetNumMapPixels(int detChan, int modChan, char *name,
                                    void *value, char *detType,
                                    XiaDefaults *defs, Module *m,
                                    Detector *det, FirmwareSet *fs)
{
    int status;

    unsigned long NUMPIXELS = 0;

    boolean_t isMapping = FALSE_;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__SetNumMapPixels", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Skipping '%s' since mapping mode is disabled for "
                "detChan %d", name, detChan);
        pslLogInfo("psl__SetNumMapPixels", info_string);
        return XIA_SUCCESS;
    }

    NUMPIXELS = (unsigned long)(*((double *)value));

    status = pslSetParameter(detChan, "NUMPIXELS",
                             (parameter_t)(NUMPIXELS & 0xFFFF));

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the total number of scan points (%lu) "
                "for detChan %d", NUMPIXELS, detChan);
        pslLogError("psl__SetNumMapPixels", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "NUMPIXELSA",
                             (parameter_t)((NUMPIXELS >> 16) & 0xFFFF));

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the total number of scan points (%lu) "
                "for detChan %d", NUMPIXELS, detChan);
        pslLogError("psl__SetNumMapPixels", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the number of scan points that should be in each buffer.
 *
 * This parameter is skipped if mapping mode is not currently active.
 *
 * Also, the value -1.0 means: Use the maximum size for points/buffer given
 * the size of my spectra.
 *
 * All buffer size validation is done by the DSP code.
 */
PSL_STATIC int psl__SetNumMapPtsBuffer(int detChan, int modChan, char *name,
                                       void *value, char *detType,
                                       XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs)
{
    int status;

    boolean_t isMapping = FALSE_;

    double pixperbuf = 0.0;

    parameter_t PIXPERBUF = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__SetNumMapPtsBuffer", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Skipping '%s' since mapping mode is disabled for "
                "detChan %d", name, detChan);
        pslLogInfo("psl__SetNumMapPtsBuffer", info_string);
        return XIA_SUCCESS;
    }

    pixperbuf = *((double *)value);

    /* Tell the DSP to maximize the pixel points per buffer. */
    if (pixperbuf == -1.0) {
        PIXPERBUF = 0;

    } else {
        PIXPERBUF = (parameter_t)pixperbuf;
    }

    status = pslSetParameter(detChan, "PIXPERBUF", PIXPERBUF);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the number of points per buffer for "
                "detChan %d", detChan);
        pslLogError("psl__SetNumMapPtsBuffer", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the number of scan points in each buffer.
 */
PSL_STATIC int psl__GetNumMapPtsBuffer(int detChan, void *value,
                                       XiaDefaults *defs)
{
    int status;

    parameter_t PIXPERBUF = 0;

    UNUSED(defs);


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "PIXPERBUF", &PIXPERBUF);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading number of pixels per buffer from "
                "the hardware for detChan %d", detChan);
        pslLogError("psl__GetNumMapPtsBuffer", info_string, status);
        return status;
    }

    *((double *)value) = (double)PIXPERBUF;

    return XIA_SUCCESS;
}


/*
 * Sets the specified buffer status to "done".
 *
 * Requires mapping firmware.
 *
 * Returns an error if the specified buffer is not 'a' or 'b'.
 */
PSL_STATIC int psl__SetBufferDone(int detChan, char *name, XiaDefaults *defs,
                                  void *value)
{
    int status;

    boolean_t isMapping = FALSE_;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__SetBufferDone", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Mapping mode firmware not running on detChan %d",
                detChan);
        pslLogError("psl__SetBufferDone", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    status = psl__ClearBuffer(detChan, *((char *)value), TRUE_);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting buffer '%c' status to 'done' for "
                "detChan %d", *((char *)value), detChan);
        pslLogError("psl__SetBufferDone", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Calculates the size of the buffer, in 16-bit words, that will be
 * returned by a call to xiaGetRunData("buffer_a" or "buffer_b").
 *
 * Requires mapping firmware.
 */
PSL_STATIC int psl__GetBufferLen(int detChan, void *value, XiaDefaults *defs,
                                 Module *m)
{
    int status;

    boolean_t isMapping = FALSE_;

    parameter_t MAPPINGMODE = 0;
    parameter_t PIXPERBUF = 0;

    unsigned long bufferSize     = 0;
    unsigned long pixelBlockSize = 0;

    ASSERT(defs  != NULL);
    ASSERT(value != NULL);


    status = psl__IsMapping(detChan, MAPPING_MCA | MAPPING_SCA, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__GetBufferLen", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Mapping mode firmware not running on detChan %d",
                detChan);
        pslLogError("psl__GetBufferLen", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    status = pslGetParameter(detChan, "MAPPINGMODE", &MAPPINGMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading the mapping mode for detChan %d",
                detChan);
        pslLogError("psl__GetBufferLen", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "PIXPERBUF", &PIXPERBUF);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading the number of pixel points in the "
                "buffer for detChan %d", detChan);
        pslLogError("psl__GetBufferLen", info_string, status);
        return status;
    }

    if (MAPPINGMODE == 1) {
        pixelBlockSize = psl__GetMCAPixelBlockSize(defs, m);
    } else {
        pixelBlockSize = psl__GetSCAPixelBlockSize(defs, m);
    }

    bufferSize = STJ_MEMORY_BLOCK_SIZE + (PIXPERBUF * pixelBlockSize);
    /* Buffer size better be less then 1M x 16-bits. */
    ASSERT(bufferSize <= 1048576);

    *((unsigned long *)value) = bufferSize;

    return XIA_SUCCESS;
}


/*
 * Calculates the size of each pixel block
 * in 16-bit words, that will be
 * returned by a call to xiaGetRunData("buffer_a" or "buffer_b").
 *
 * Requires mapping firmware.
 */
PSL_STATIC unsigned long psl__GetMCAPixelBlockSize(XiaDefaults *defs,
                                                   Module *m)
{
    int status;
    double mcaLen = 0.0;

    unsigned long pixelBlockSize = 0;

    UNUSED(m);

    status = pslGetDefault("number_mca_channels", (void *)&mcaLen, defs);
    ASSERT(status == XIA_SUCCESS);

    /* This calculation implicitly assumes that all 4 channels are
    * included in the buffer data. Luckily, I think that the notion
    * of a disabled channel is only present in Handel. The hardware
    * assumes that all channels are working and should, consequently,
    * be included.
    */
    pixelBlockSize = (4 * (unsigned long)mcaLen) + STJ_MEMORY_BLOCK_SIZE;

    return pixelBlockSize;

}


/*
 * Calculates the size of the SCA Mapping buffer pixel block
 * in 16-bit words
 * Requires mapping firmware.
 */
PSL_STATIC unsigned long psl__GetSCAPixelBlockSize(XiaDefaults *defs,
                                                   Module *m)
{
    int i;

    double totalSCA = 0;

    unsigned long pixelBlockSize = 0;

    UNUSED(defs);
    ASSERT(m != NULL);

    for (i = 0; i < 4; i++) {
        /* The SCA values here are 32-bit words per SCA. */
        totalSCA +=  (m->ch[i].n_sca * 2);
    }

    pixelBlockSize = STJ_SCA_PIXEL_BLOCK_HEADER_SIZE + (unsigned long)totalSCA;

    return pixelBlockSize;
}


/*
 * Read mapping data from Buffer A.
 *
 * Requires mapping firmware.
 */
PSL_STATIC int psl__GetBufferA(int detChan, void *value, XiaDefaults *defs,
                               Module *m)
{
    int status;


    ASSERT(m    != NULL);
    ASSERT(defs != NULL);


    status = psl__GetBuffer(detChan, 'a', (unsigned long *)value, defs, m);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading Buffer A for detChan =  %d",
                detChan);
        pslLogError("psl__GetBufferA", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Read mapping data from Buffer B.
 *
 * Requires mapping firmware.
 */
PSL_STATIC int psl__GetBufferB(int detChan, void *value, XiaDefaults *defs,
                               Module *m)
{
    int status;


    ASSERT(m    != NULL);
    ASSERT(defs != NULL);


    status = psl__GetBuffer(detChan, 'b', (unsigned long *)value, defs, m);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading Buffer B for detChan =  %d",
                detChan);
        pslLogError("psl__GetBufferB", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the requested buffer from the external memory.
 *
 * Requires mapping firmware.
 *
 * Assumes that the proper amount of memory has been allocated for data.
 */
PSL_STATIC int psl__GetBuffer(int detChan, char buf, unsigned long *data,
                              XiaDefaults *defs, Module *m)
{
    int status;
    int statusX;

    unsigned long len  = 0;
    unsigned long base = 0;

    boolean_t isMCAOrSCA;
    boolean_t isList;

    char memoryStr[36];

    ASSERT(data != NULL);
    ASSERT(buf == 'a' || buf == 'b');


    status = psl__IsMapping(detChan, MAPPING_MCA | MAPPING_SCA, &isMCAOrSCA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d",
                detChan);
        pslLogError("psl__GetBuffer", info_string, status);
        return status;
    }

    status = psl__IsMapping(detChan, MAPPING_LIST, &isList);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d",
                detChan);
        pslLogError("psl__GetBuffer", info_string, status);
        return status;
    }

    if (!isMCAOrSCA && !isList) {
        sprintf(info_string, "Mapping mode firmware not running on detChan %d",
                detChan);
        pslLogError("psl__GetBuffer", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    /* Use "no word packing". Once packing support is included, we can update
     * the memory base here.
     */
    switch (buf) {
    case 'a':
        base = 0x4000000;
        break;

    case 'b':
        base = 0x6000000;
        break;

    default:
        FAIL();
        break;
    }

    if (isMCAOrSCA) {
        status = psl__GetBufferLen(detChan, (void *)&len, defs, m);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting length of buffer '%c' for "
                    "detChan %d", buf, detChan);
            pslLogError("psl__GetBuffer", info_string, status);
            return status;
        }

    } else if (isList) {
        /* The list mode lengths are not a fixed size, unlike the
         * MCA/SCA mode buffer lengths.
         */
        status = psl__GetListBufferLen(detChan, buf, &len);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting the length of list mode "
                    "buffer '%c' for detChan %d.", buf, detChan);
            pslLogError("psl__GetBuffer", info_string, status);
            return status;
        }

    } else {
        FAIL();
    }

    sprintf(memoryStr, "burst_map:%#lx:%lu", base, len);

    statusX = dxp_read_memory(&detChan, memoryStr, data);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading memory for buffer '%c' on detChan %d",
                buf, detChan);
        pslLogError("psl__GetBuffer", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the current mapping point.
 *
 * Requires mapping more firmware.
 */
PSL_STATIC int psl__GetCurrentPixel(int detChan, void *value, XiaDefaults *defs,
                                    Module *m)
{
    int status;

    boolean_t isMapping = FALSE_;

    parameter_t PIXELNUM  = 0;
    parameter_t PIXELNUMA = 0;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value != NULL);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__GetCurrentPixel", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Mapping mode firmware not running on detChan %d",
                detChan);
        pslLogError("psl__GetCurrentPixel", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    status = pslGetParameter(detChan, "PIXELNUM", &PIXELNUM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading current pixel number for detChan %d",
                detChan);
        pslLogError("psl__GetCurrentPixel", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "PIXELNUMA", &PIXELNUMA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading current pixel number for detChan %d",
                detChan);
        pslLogError("psl__GetCurrentPixel", info_string, status);
        return status;
    }

    *((unsigned long *)value) = WORD_TO_LONG(PIXELNUM, PIXELNUMA);

    sprintf(info_string, "Current pixel = %lu for detChan %d",
            *((unsigned long *)value), detChan);
    pslLogDebug("psl__GetCurrentPixel", info_string);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetListBufferLenA(int detChan, void *value,
                                      XiaDefaults *defs, Module *m)
{
    int status;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetListBufferLen(detChan, 'a', (unsigned long *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the length of list mode buffer "
                "A for detChan %d.", detChan);
        pslLogError("psl__GetListBufferLenA", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetListBufferLenB(int detChan, void *value,
                                      XiaDefaults *defs, Module *m)
{
    int status;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetListBufferLen(detChan, 'b', (unsigned long *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the length of list mode buffer "
                "B for detChan %d.", detChan);
        pslLogError("psl__GetListBufferLenB", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetListBufferLen(int detChan, char buf, unsigned long *len)
{
    int status;

    boolean_t isMapping;

    parameter_t lenLow = 0xFFFF;
    parameter_t lenHigh = 0xFFFF;


    status = psl__IsMapping(detChan, MAPPING_LIST, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking if list mode is available for "
                "detChan %d.", detChan);
        pslLogError("psl__GetListBufferLen", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "List mode firmware is not currently loaded for "
                "detChan %d.", detChan);
        pslLogError("psl__GetListBufferLen", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    switch(buf) {
    case 'a':
        status = pslGetParameter(detChan, "LISTBUFALEN", &lenLow);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting low word of list mode "
                    "buffer length for detChan %d.", detChan);
            pslLogError("psl__GetListBufferLen", info_string, status);
            return status;
        }

        status = pslGetParameter(detChan, "LISTBUFALENA", &lenHigh);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting high word of list mode "
                    "buffer length for detChan %d.", detChan);
            pslLogError("psl__GetListBufferLen", info_string, status);
            return status;
        }

        break;

    case 'b':
        status = pslGetParameter(detChan, "LISTBUFBLEN", &lenLow);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting low word of list mode "
                    "buffer length for detChan %d.", detChan);
            pslLogError("psl__GetListBufferLen", info_string, status);
            return status;
        }

        status = pslGetParameter(detChan, "LISTBUFBLENA", &lenHigh);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting high word of list mode "
                    "buffer length for detChan %d.", detChan);
            pslLogError("psl__GetListBufferLen", info_string, status);
            return status;
        }

        break;

    default:
        FAIL();
        break;
    }

    /* Only the bottom 4 bits of the high word should be set. The
     * maximum length of each buffer is 20 bits.
     */
    if ((lenHigh & 0xFFF0) != 0) {
        sprintf(info_string, "The upper word of the list buffer length "
                "stored in the DSP (%#hx) is malformed for detChan %d.",
                lenHigh, detChan);
        pslLogError("psl__GetListBufferLen", info_string, XIA_MALFORMED_LENGTH);
        return XIA_MALFORMED_LENGTH;
    }

    *len = WORD_TO_LONG(lenLow, lenHigh);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetListModeVariant(int detChan, int modChan, char *name,
                                       void *value, char *detType,
                                       XiaDefaults *defs, Module *m,
                                       Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t LISTMODEVAR;

    boolean_t isMapping;

    UNUSED(fs);
    UNUSED(det);
    UNUSED(m);
    UNUSED(detType);
    UNUSED(modChan);
    UNUSED(defs);

    ASSERT(value);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking mapping mode setting for "
                "detChan %d.", detChan);
        pslLogError("psl__SetListModeVariant", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Skipping '%s' since mapping mode is disabled "
                "for detChan %d.", name, detChan);
        pslLogInfo("psl__SetListModeVariant", info_string);
        return XIA_SUCCESS;
    }

    LISTMODEVAR = (parameter_t)(*((double *)value));

    if (LISTMODEVAR > (parameter_t)XIA_LIST_MODE_PMT) {
        sprintf(info_string, "Specified list mode variant (%hu) is invalid "
                "for detChan %d.", LISTMODEVAR, detChan);
        pslLogError("psl__SetListModeVariant", info_string,
                    XIA_UNKNOWN_LIST_MODE_VARIANT);
        return XIA_UNKNOWN_LIST_MODE_VARIANT;
    }

    status = pslSetParameter(detChan, "LISTMODEVAR", LISTMODEVAR);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting list mode variant to %hu "
                "for detChan %d.", LISTMODEVAR, detChan);
        pslLogError("psl__SetListModeVariant", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Advances the mapping point to the next pixel.
 *
 * Requires mapping firmware.
 *
 * Requires mapping point control to be set to HOST, otherwise an
 * error is returned.
 */
PSL_STATIC int psl__MapPixelNext(int detChan, char *name, XiaDefaults *defs,
                                 void *value)
{
    int status;
    int statusX;

    boolean_t isMapping = FALSE_;

    unsigned long mfr = 0;

    UNUSED(name);
    UNUSED(value);
    UNUSED(defs);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking firmware type for detChan %d", detChan);
        pslLogError("psl__MapPixelNext", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Mapping mode firmware not running on detChan %d",
                detChan);
        pslLogError("psl__MapPixelNext", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    /* Set bit 13 to advance the pixel. */
    mfr = 0x2000;

    statusX = dxp_write_register(&detChan, "MFR", &mfr);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error writing Mapping Flag Register for detChan %d",
                detChan);
        pslLogError("psl__MapPixelNext", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the specified bit in the requested register.
 *
 * Uses the read/modify/write idiom to set the register bit, so
 * all of the previous bit states are preserved.
 */
PSL_STATIC int psl__SetRegisterBit(int detChan, char *reg, int bit,
                                   boolean_t overwrite)
{
    int statusX;

    unsigned long val = 0;


    if (!overwrite) {
        statusX = dxp_read_register(&detChan, reg, &val);

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error reading the '%s' for detChan %d",
                    reg, detChan);
            pslLogError("psl__SetRegisterBit", info_string, XIA_XERXES);
            return XIA_XERXES;
        }
    }

    val |= (0x1 << bit);

    statusX = dxp_write_register(&detChan, reg, &val);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %#lx to the '%s' after setting bit %d "
                "for detChan %d", val, reg, bit, detChan);
        pslLogError("psl__SetRegisterBit", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Clears the specified bit in the requested register.
 *
 * Uses the read/modify/write idiom to set the register bit, so
 * all of the previous bit states are preserved.
 */
PSL_STATIC int psl__ClearRegisterBit(int detChan, char *reg, int bit)
{
    int statusX;

    unsigned long val = 0;


    statusX = dxp_read_register(&detChan, reg, &val);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading the '%s' for detChan %d",
                reg, detChan);
        pslLogError("psl__ClearRegisterBit", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    val &= ~(0x1 << bit);

    statusX = dxp_write_register(&detChan, reg, &val);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error writing %#lx to the '%s' after clearing bit %d "
                "for detChan %d", val, reg, bit, detChan);
        pslLogError("psl__ClearRegisterBit", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Clears the requested buffer.
 *
 * Requires mapping firmware. Calling routines do not need to check
 * the firmware type before calling this routine. However, if mapping mode
 * firmware is not being used an error will be returned which the calling
 * routine can trap and ignore.
 *
 * Accepted buffers are 'a' and 'b'.
 */
PSL_STATIC int psl__ClearBuffer(int detChan, char buf, boolean_t waitForEmpty)
{
    int status;
    int done;
    int empty;

    float interval = .001f;
    float timeout = .1f;

    int n_polls = 0;
    int i;

    boolean_t cleared   = FALSE_;

    switch (buf) {
    case 'a':
        done  = STJ_MFR_BUFFER_A_DONE;
        empty = STJ_MFR_BUFFER_A_EMPTY;
        break;
    case 'b':
        done  = STJ_MFR_BUFFER_B_DONE;
        empty = STJ_MFR_BUFFER_B_EMPTY;
        break;
    default:
        sprintf(info_string, "Specified buffer '%c' is not a valid buffer for "
                "detChan %d", buf, detChan);
        pslLogError("psl__ClearBuffer", info_string, XIA_UNKNOWN_BUFFER);
        return XIA_UNKNOWN_BUFFER;
    }

    status = psl__SetRegisterBit(detChan, "MFR", done, TRUE_);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting buffer '%c' to done for detChan %d",
                buf, detChan);
        pslLogError("psl__ClearBuffer", info_string, status);
        return status;
    }

    if (waitForEmpty) {
        n_polls = (int)ROUND(timeout / interval);

        for (i = 0; i < n_polls; i++) {
            status = psl__CheckRegisterBit(detChan, "MFR", empty, &cleared);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error waiting for buffer '%c' to clear on "
                        "detChan %d", buf, detChan);
                pslLogError("psl__ClearBuffer", info_string, status);
                return status;
            }

            if (cleared) return XIA_SUCCESS;
            utils->funcs->dxp_md_wait(&interval);
        }

    }

    sprintf(info_string, "Timeout waiting for buffer '%c' to be set to empty",
            buf);
    pslLogError("psl__ClearBuffer", info_string, XIA_CLRBUFFER_TIMEOUT);

    return XIA_CLRBUFFER_TIMEOUT;
}


/*
 * Checks that the specified bit is set (or not) in the
 * specified register.
 */
PSL_STATIC int psl__CheckRegisterBit(int detChan, char *reg, int bit,
                                     boolean_t *isSet)
{
    int statusX;

    unsigned long val = 0;


    ASSERT(reg != NULL);


    statusX = dxp_read_register(&detChan, reg, &val);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading bit %d of the '%s' for detChan %d",
                bit, reg, detChan);
        pslLogError("psl__CheckRegisterBit", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *isSet = (boolean_t)(val & (1 << bit));

    return XIA_SUCCESS;
}


/*
 * Enables disables mapping mode by switching to the appropriate
 * firmware.
 *
 * Also used to indicate if mapping parameters should be downloaded to
 * the hardware during startup.
 */
PSL_STATIC int psl__SetMappingMode(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    unsigned int i;

    boolean_t enabled = FALSE_;
    boolean_t sca_mapping = FALSE_;

    double mapping_mode = 0.0;
    parameter_t MAPPINGMODE = 0;

    UNUSED(det);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(modChan);
    UNUSED(defs);
    UNUSED(fs);

    ASSERT(value != NULL);
    ASSERT(m != NULL);

    mapping_mode = *((double *)value);

    if ((unsigned short)mapping_mode > MAPPINGMODE_LIST) {
        sprintf(info_string, "Unsupported mapping mode "
                "%hu for detChan %d", (unsigned short)mapping_mode, detChan);
        pslLogError("psl__SetMappingMode", info_string, XIA_UNKNOWN_MAPPING);
        return XIA_UNKNOWN_MAPPING;
    }

    enabled = (boolean_t)(*((double *)value) > 0 ? TRUE_ : FALSE_);
    sca_mapping = (boolean_t)(mapping_mode == MAPPINGMODE_SCA);

    /*
    * Set the MAPPINGMODE DSP parameter
    */
    MAPPINGMODE = (parameter_t)(*((double *)value));

    status = pslSetParameter(detChan, "MAPPINGMODE", MAPPINGMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating mode in the DSP for "
                "detChan %d", detChan);
        pslLogError("psl__SetMappingMode", info_string, status);
        return status;
    }

    if (enabled) {
        /* Write the DSP parameters that are used to fill the mapping buffers. */
        for (i = 0; i < m->number_of_channels; i++) {

            /* Skip if the channel is disabled. */
            if (m->channels[i] != -1) {

                /* If this is the first channel, then set the module number.
                * If the first channel is disabled then this will be a problem.
                */
                if (i == 0) {
                    status = pslSetParameter(m->channels[i], "MODNUM",
                                             (parameter_t)(m->channels[i] / 32));

                    if (status != XIA_SUCCESS) {
                        sprintf(info_string, "Error setting module number for mapping "
                                "buffer on detChan %d", m->channels[i]);
                        pslLogError("psl__SetMappingMode", info_string, status);
                        return status;
                    }
                }

                /* Make SCAMAPMODE default to 1 so that different SCA regions
                 * can be defined for each channel, this may become an
                 * acquisition value in the future.
                 */
                if (i == 0 && sca_mapping) {
                    status = pslSetParameter(m->channels[i], "SCAMAPMODE", 1);

                    if (status != XIA_SUCCESS) {
                        sprintf(info_string, "Error setting SCA mapping mode "
                                "on detChan %d", m->channels[i]);
                        pslLogError("psl__SetMappingMode", info_string, status);
                        return status;
                    }
                }

                status = pslSetParameter(m->channels[i], "DETCHANNEL",
                                         (parameter_t)m->channels[i]);

                if (status != XIA_SUCCESS) {
                    sprintf(info_string, "Error setting detector channel for mapping "
                            "buffer on detChan %d", m->channels[i]);
                    pslLogError("psl__SetMappingMode", info_string, status);
                    return status;
                }

                status = pslSetParameter(m->channels[i], "DETELEMENT",
                                         (parameter_t)m->detector_chan[i]);

                if (status != XIA_SUCCESS) {
                    sprintf(info_string, "Error setting detector element for mapping "
                            "buffer on detChan %d", m->channels[i]);
                    pslLogError("psl__SetMappingMode", info_string, status);
                    return status;
                }
            }
        }
    }

    return XIA_SUCCESS;
}


/*
 * Gets the value of the MCR.
 */
PSL_STATIC int psl__GetMCR(int detChan, char *name, XiaDefaults *defs,
                           void *value)
{
    int statusX;

    UNUSED(defs);
    UNUSED(name);


    statusX = dxp_read_register(&detChan, "MCR", (unsigned long *)value);

    sprintf(info_string, "MCR = %#lx", *((unsigned long *)value));
    pslLogDebug("psl__GetMCR", info_string);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading MCR for detChan %d", detChan);
        pslLogError("psl__GetMCR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the LEMO input to No Connection.
 */
PSL_STATIC int psl__SetInputNC(int detChan)
{
    int status;


    status = psl__ClearRegisterBit(detChan, "MCR", 0);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting No Connection (bit 0) for detChan %d",
                detChan);
        pslLogError("psl__SetInputNC", info_string, status);
        return status;
    }

    status = psl__ClearRegisterBit(detChan, "MCR", 1);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting No Connection (bit 1) for detChan %d",
                detChan);
        pslLogError("psl__SetInputNC", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}



/*
 * Specify an optional peak sample time offset, in microseconds,
 * that overrides the value specified in the FDD file.
 */
PSL_STATIC int psl__SetPeakSampleOffset(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs)
{
    int dec = 0;
    int status;
    int offset;

    parameter_t DECIMATION = 0;

    double pt   = 0.0;
    double tick = psl__GetClockTick() * 1.0e6;

    UNUSED(detType);


    ASSERT(name != NULL);
    ASSERT(defs != NULL);
    ASSERT(value != NULL);
    ASSERT(fs != NULL);
    ASSERT(m != NULL);
    ASSERT(det != NULL);


    /* Get the decimation that this value applies to so we can check
    * if we need to update PEAKSAM.
    */
    sscanf(name, "peak_sample_offset%d", &dec);

    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting currenr decimation for detChan %d",
                detChan);
        pslLogError("psl__SetPeakSampleOffset", info_string, status);
        return status;
    }

    if (dec == (int)DECIMATION) {
        status = pslSetDefault(name, value, defs);
        ASSERT(status == XIA_SUCCESS);

        status = pslGetDefault("peaking_time", (void *)&pt, defs);
        ASSERT(status == XIA_SUCCESS);

        status = psl__UpdateFilterParams(detChan, modChan, pt, defs, fs, m, det);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error updating filter parameter after peak sample "
                    "offset changed to %0.3f for detChan %d", *((double *)value),
                    detChan);
            pslLogError("psl__SetPeakSampleOffset", info_string, status);
            return status;
        }
    }

    /* Calculate the actual offset time in decimated clock ticks. */
    offset = (int)ROUND(*((double *)value) / ldexp(tick, dec));
    *((double *)value) = offset * ldexp(tick, dec);

    return XIA_SUCCESS;
}


/*
 * Checks if a buffer overrun condition has been signaled.
 *
 * A value of 1 indicates a buffer overrun condition, while 0 indicates
 * that the buffer has not been overrun.
 *
 * Requires mapping mode to be enabled.
 */
PSL_STATIC int psl__GetBufferOverrun(int detChan, void *value,
                                     XiaDefaults *defs, Module *m)
{
    int status;
    int statusX;

    boolean_t isMapping = FALSE_;

    unsigned long mfr = 0;

    UNUSED(m);
    UNUSED(defs);


    status = psl__IsMapping(detChan, MAPPING_ANY, &isMapping);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error determining if mapping mode was enabled for "
                "detChan %d", detChan);
        pslLogError("psl__GetBufferOverrun", info_string, status);
        return status;
    }

    if (!isMapping) {
        sprintf(info_string, "Mapping mode firmware is currently not running on "
                "detChan %d", detChan);
        pslLogError("psl__GetBufferOverrun", info_string, XIA_NO_MAPPING);
        return XIA_NO_MAPPING;
    }

    statusX = dxp_read_register(&detChan, "MFR", &mfr);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading Mapping Flag Register for detChan %d",
                detChan);
        pslLogError("psl__GetBufferOverrun", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (mfr & (1 << STJ_MFR_BUFFER_OVERRUN)) {
        *((unsigned short *)value) = 1;
    } else {
        *((unsigned short *)value) = 0;
    }

    return XIA_SUCCESS;
}


/*
 * Get the Mapping Flag Register.
 */
PSL_STATIC int psl__GetMFR(int detChan, char *name, XiaDefaults *defs,
                           void *value)
{
    int statusX;

    UNUSED(defs);
    UNUSED(name);


    statusX = dxp_read_register(&detChan, "MFR", (unsigned long *)value);

    sprintf(info_string, "MFR = %#lx", *((unsigned long *)value));
    pslLogDebug("psl__GetMFR", info_string);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading MFR for detChan %d", detChan);
        pslLogError("psl__GetMFR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;

}


/*
 * Set the minimum gap time for the slow filter.
 */
PSL_STATIC int psl__SetMinGapTime(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs)
{
    int status;

    double pt = 0.0;

    UNUSED(name);
    UNUSED(detType);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    status = pslSetDefault("minimum_gap_time", (void *)value, defs);
    ASSERT(status == XIA_SUCCESS);

    /* It feels a little odd to be pulling the peaking time out here, just to pass
    * it into a function that could it pull it out itself.
    */
    status = pslGetDefault("peaking_time", (void *)&pt, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateFilterParams(detChan, modChan, pt, defs, fs, m, det);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error updating filter parameters after changing the "
                "slow filter minimum gap time for detChan %d", detChan);
        pslLogError("psl__SetMinGapTime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the Control Status Register.
 */
PSL_STATIC int psl__GetCSR(int detChan, char *name, XiaDefaults *defs,
                           void *value)
{
    int statusX;

    UNUSED(defs);
    UNUSED(name);


    statusX = dxp_read_register(&detChan, "CSR", (unsigned long *)value);

    sprintf(info_string, "CSR = %#lx", *((unsigned long *)value));
    pslLogDebug("psl__GetCSR", info_string);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading CSR for detChan %d", detChan);
        pslLogError("psl__GetCSR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Set the peak interval offset for the specified decimation.
 */
PSL_STATIC int psl__SetPeakIntervalOffset(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs)
{
    int dec = 0;
    int status;
    int offset;

    parameter_t DECIMATION = 0;

    double pt   = 0.0;
    double tick = psl__GetClockTick() * 1.0e6;

    UNUSED(detType);


    ASSERT(name != NULL);
    ASSERT(defs != NULL);
    ASSERT(value != NULL);
    ASSERT(fs != NULL);
    ASSERT(m != NULL);
    ASSERT(det != NULL);


    /* Get the decimation that this value applies to so we can check
    * if we need to update PEAKINT.
    */
    sscanf(name, "peak_interval_offset%d", &dec);

    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting currenr decimation for detChan %d",
                detChan);
        pslLogError("psl__SetPeakIntervalOffset", info_string, status);
        return status;
    }

    if (dec == (int)DECIMATION) {
        status = pslSetDefault(name, value, defs);
        ASSERT(status == XIA_SUCCESS);

        status = pslGetDefault("peaking_time", (void *)&pt, defs);
        ASSERT(status == XIA_SUCCESS);

        status = psl__UpdateFilterParams(detChan, modChan, pt, defs, fs, m, det);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error updating filter parameter after peak interval "
                    "offset changed to %0.3f for detChan %d", *((double *)value),
                    detChan);
            pslLogError("psl__SetPeakIntervalOffset", info_string, status);
            return status;
        }
    }

    offset = (int)ROUND(*((double *)value) / ldexp(tick, dec));
    *((double *)value) = offset * ldexp(tick, dec);

    return XIA_SUCCESS;
}


/*
 * Set the maximum width of the trigger filter pile-up inspection
 */
PSL_STATIC int psl__SetMaxWidth(int detChan, int modChan, char *name,
                                void *value, char *detType,
                                XiaDefaults *defs, Module *m,
                                Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t MAXWIDTH;

    /* Scale the tick to microseconds. */
    double tick = psl__GetClockTick() * 1.0e6;

    UNUSED(fs);
    UNUSED(det);
    UNUSED(m);
    UNUSED(defs);
    UNUSED(detType);
    UNUSED(name);
    UNUSED(modChan);


    ASSERT(value != NULL);


    MAXWIDTH = (parameter_t)ROUND(*((double *)value) / tick);

    if (MAXWIDTH < MIN_MAXWIDTH || MAXWIDTH > MAX_MAXWIDTH) {
        sprintf(info_string, "Requested max. width (%0.3f microseconds) is "
                "out-of-range (%0.3f, %0.3f) for detChan %d",
                *((double *)value), MIN_MAXWIDTH * tick, MAX_MAXWIDTH * tick,
                detChan);
        pslLogError("psl__SetMaxWidth", info_string, XIA_MAXWIDTH_OOR);
        return XIA_MAXWIDTH_OOR;
    }

    status = pslSetParameter(detChan, "MAXWIDTH", MAXWIDTH);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting max. width for detChan %d", detChan);
        pslLogError("psl__SetMaxWidth", info_string, status);
        return status;
    }

    *((double *)value) = (double)MAXWIDTH * tick;

    return XIA_SUCCESS;
}


/*
 * Read the CPLD Version Register.
 */
PSL_STATIC int psl__GetCVR(int detChan, char *name, XiaDefaults *defs,
                           void *value)
{
    int statusX;

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_read_register(&detChan, "CVR", (unsigned long *)value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading CVR for detChan %d", detChan);
        pslLogError("psl__GetCVR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Read the System FPGA Version Register.
 */
PSL_STATIC int psl__GetSVR(int detChan, char *name, XiaDefaults *defs,
                           void *value)
{
    int statusX;

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_read_register(&detChan, "SVR", (unsigned long *)value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading SVR for detChan %d", detChan);
        pslLogError("psl__GetSVR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;

}


/*
 * Read the energy livetime from the board.
 */
PSL_STATIC int psl__GetELivetime(int detChan, void *value, XiaDefaults *defs,
                                 Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);
    ASSERT(m != NULL);


    status = pslGetModChan(detChan, m, &modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting modChan for module '%s' from detChan %d",
                m->alias, detChan);
        pslLogError("psl__GetELivetime", info_string, status);
        return status;
    }

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d", detChan);
        pslLogError("psl__GetELivetime", info_string, status);
        return status;
    }

    status = psl__ExtractELivetime(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting energy livetime for detChan %d",
                detChan);
        pslLogError("psl__GetELivetime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Read the statistics block for the specified module from the
 * external memory.
 *
 * Callers are responsible for allocating enough memory for stats.
 */
PSL_STATIC int psl__GetStatisticsBlock(int detChan, unsigned long *stats)
{
    int statusX;

    char mem[MAXITEM_LEN];

    ASSERT(stats != NULL);

    sprintf(mem, "burst:%#x:%d", 0x00, STJ_STATS_BLOCK_SIZE);

    statusX = dxp_read_memory(&detChan, mem, stats);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error burst reading statistics block for detChan %d",
                detChan);
        pslLogError("psl__GetStatisticsBlock", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Elivetime not implemented for STJ
 */
PSL_STATIC int psl__ExtractELivetime(int modChan, unsigned long *stats,
                                     double *eLT)
{
    UNUSED(modChan);
    UNUSED(stats);
    UNUSED(eLT);

    return XIA_SUCCESS;
}


/*
 * Extract the realtime for the specified _module_ channel
 * from the module statistics block.
 */
PSL_STATIC int psl__ExtractRealtime(int modChan, unsigned long *stats,
                                    double *rt)
{
    unsigned long rtOffset;

    double tick = psl__GetClockTick();


    ASSERT(stats != NULL);
    ASSERT(rt    != NULL);
    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);


    rtOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_REALTIME_OFFSET;
    *rt      = pslU64ToDouble(stats + rtOffset) * tick * 16.0;

    return XIA_SUCCESS;
}


/*
 * Returns the statistics for all of the channels on the module
 * that detChan is a part of. value is expected to be a double array
 * with at least 28 elements. They are stored in the following format:
 *
 * [ch0_runtime, ch0_trigger_livetime, ch0_energy_livetime, ch0_triggers,
 *  ch0_events, ch0_icr, ch0_ocr, ..., ch3_runtime, etc.]
 */
PSL_STATIC int psl__GetModuleStatistics(int detChan, void *value,
                                        XiaDefaults * defs, Module *m)
{
    int status;
    int i;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    double *modStats = NULL;

    double tLT;
    double rt;
    double trigs;
    double evts;
    double unders;
    double overs;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value != NULL);


    modStats = (double *)value;

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d", detChan);
        pslLogError("psl__GetModuleStatistics", info_string, status);
        return status;
    }

    for (i = 0; i < 32; i++) {
        status = psl__ExtractRealtime(i, stats, &rt);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting runtime for detChan %d, modChan %d",
                    detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        modStats[i * 7] = rt;

        status = psl__ExtractTLivetime(i, stats, &tLT);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting trigger livetime for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        modStats[(i * 7) + 1] = tLT;

        status = psl__ExtractELivetime(i, stats, &(modStats[(i * 7) + 2]));

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting energy livetime for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        status = psl__ExtractTriggers(i, stats, &trigs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting triggers for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        modStats[(i * 7) + 3] = trigs;

        status = psl__ExtractEvents(i, stats, &evts);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting events for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        modStats[(i * 7) + 4] = evts;

        status = psl__ExtractUnderflows(i, stats, &unders);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting underflows for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        status = psl__ExtractOverflows(i, stats, &overs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting overflows for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics", info_string, status);
            return status;
        }

        if (tLT != 0.0) {
            modStats[(i * 7) + 5] = trigs / tLT;
        } else {
            modStats[(i * 7) + 5] = 0.0;
        }

        if (rt != 0.0) {
            modStats[(i * 7) + 6] = (evts + overs + unders) / rt;
        } else {
            modStats[(i * 7) + 6] = 0.0;
        }
    }

    return XIA_SUCCESS;
}


/*
 * Extracts the trigger livetime for the specified module channel
 * from the statistics block.
 */
PSL_STATIC int psl__ExtractTLivetime(int modChan, unsigned long *stats,
                                     double *tLT)
{
    unsigned long tLTOffset;

    double tick = psl__GetClockTick();


    ASSERT(stats != NULL);
    ASSERT(tLT   != NULL);
    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);


    tLTOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_TLIVETIME_OFFSET;
    *tLT      = pslU64ToDouble(stats + tLTOffset) * tick * 16.0;

    return XIA_SUCCESS;
}


/*
 * Extracts the triggers for the specified module channel from
 * the statistics block.
 */
PSL_STATIC int psl__ExtractTriggers(int modChan, unsigned long *stats,
                                    double *trigs)
{
    unsigned long trigsOffset;


    ASSERT(stats != NULL);
    ASSERT(trigs != NULL);
    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);


    trigsOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_TRIGGERS_OFFSET;
    *trigs      = pslU64ToDouble(stats + trigsOffset);

    return XIA_SUCCESS;
}


/*
 * Extracts the events in run for the specified module channel from
 * the statistics block.
 */
PSL_STATIC int psl__ExtractEvents(int modChan, unsigned long *stats,
                                  double *evts)
{
    unsigned long evtsOffset;


    ASSERT(stats != NULL);
    ASSERT(evts  != NULL);
    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);


    evtsOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_EVENTS_OFFSET;
    *evts      = pslU64ToDouble(stats + evtsOffset);

    return XIA_SUCCESS;
}


/*
 * Reads out the entire MCA block for the module that detChan
 * is located in. This routine is an alternative to reading the MCA
 * our individually for each channel. This routine assumes that all of the
 * channels share the same size MCA.
 */
PSL_STATIC int psl__GetModuleMCA(int detChan, void *value,
                                 XiaDefaults *defs, Module *m)
{
    int statusX;
    int status;

    unsigned long addr;
    unsigned long len;

    double nBins;

    char memStr[36];

    UNUSED(m);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    /* Skip past the initial statistics block. */
    addr = STJ_STATS_BLOCK_SIZE;

    status = pslGetDefault("number_mca_channels", &nBins, defs);
    ASSERT(status == XIA_SUCCESS);

    /* We require that all channels use the same length MCA. */
    len = (unsigned long)(nBins * 32);

    sprintf(memStr, "burst:%#lx:%lu", addr, len);

    statusX = dxp_read_memory(&detChan, memStr, value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading all MCA data for the module containing"
                "detChan %d", detChan);
        pslLogError("psl__GetModuleMCA", info_string, statusX);
        return statusX;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the decay time for RC-type preamplifier.
 */
PSL_STATIC int psl__SetDecayTime(int detChan, int modChan, char *name,
                                 void *value, char *detType,
                                 XiaDefaults *defs, Module *m, Detector *det,
                                 FirmwareSet *fs)
{
    int status;

    double decayTime = 0.0;

    parameter_t RCTAU;
    parameter_t RCTAUFRAC;

    UNUSED(fs);
    UNUSED(defs);
    UNUSED(detType);
    UNUSED(name);


    ASSERT(det != NULL);
    ASSERT(m != NULL);
    ASSERT(value != NULL);


    if (det->type != XIA_DET_RCFEED) {
        sprintf(info_string, "Skipping setting RC decay time: detChan %d is not "
                "a RC-type preamplifier.", detChan);
        pslLogInfo("psl__SetDecayTime", info_string);
        return XIA_SUCCESS;
    }

    decayTime = *((double *)value);

    det->typeValue[m->detector_chan[modChan]] = decayTime;

    RCTAU     = (parameter_t)floor(decayTime);
    RCTAUFRAC = (parameter_t)ROUND((decayTime - (double)RCTAU) * 65536.0);

    status = pslSetParameter(detChan, "RCTAU", RCTAU);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting RCTAU to %#hx for a decay time of "
                "%0.6f microseconds for detChan %d", RCTAU, decayTime, detChan);
        pslLogError("psl__SetDecayTime", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, "RCTAUFRAC", RCTAUFRAC);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting RCTAUFRAC to %#hx for a decay time of "
                "%0.6f microseconds for detChan %d", RCTAUFRAC, decayTime, detChan);
        pslLogError("psl__SetDecayTime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Synchronize the detector decay time in the Detector configuration
 * with the decay_time acquisition value.
 */
PSL_STATIC int psl__SynchDecayTime(int detChan, int det_chan, Module *m,
                                   Detector *det, XiaDefaults *defs)
{
    int status;

    double decayTime;

    UNUSED(m);


    ASSERT(det != NULL);
    ASSERT(defs != NULL);


    decayTime = det->typeValue[det_chan];

    status = pslSetDefault("decay_time", &decayTime, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error synchronizing decay time for detChan %d",
                detChan);
        pslLogError("psl__SynchDecayTime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPreampType(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m, Detector *det,
                                  FirmwareSet *fs)
{
    int status;

    double pt;
    double newPreampType;
    double resetDelay;
    double currentPreampType;

    UNUSED(name);


    ASSERT(value != NULL);
    ASSERT(defs != NULL);
    ASSERT(fs != NULL);
    ASSERT(det != NULL);
    ASSERT(m != NULL);
    ASSERT(detType != NULL);


    status = pslGetDefault("peaking_time", &pt, defs);
    ASSERT(status == XIA_SUCCESS);

    newPreampType = *((double *)value);

    status = pslGetDefault("preamp_type", &currentPreampType, defs);
    ASSERT(status == XIA_SUCCESS);

    if (newPreampType == currentPreampType) {
        sprintf(info_string, "Current preamplifier type is same as requested "
                "preamplifier type. Not switching.");
        pslLogInfo("psl__SetPreampType", info_string);
        return XIA_SUCCESS;
    }

    sprintf(info_string, "Switching preamp type from %d to %d for detChan %d",
            (int) currentPreampType, (int) newPreampType, detChan);
    pslLogInfo("psl__SetPreampType", info_string);

    if (newPreampType == XIA_PREAMP_RESET) {
        status = psl__SwitchFirmware(detChan, newPreampType, modChan, pt, fs,
                                     m);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error switching firmware for detChan %d",
                    detChan);
            pslLogError("psl__SetPreampType", info_string, status);
            return status;
        }

        det->type = XIA_DET_RESET;

        /* Redownload the reset interval. */
        status = pslGetDefault("reset_delay", &resetDelay, defs);
        ASSERT(status == XIA_SUCCESS);

        det->typeValue[m->detector_chan[modChan]] = resetDelay;

        status = pslSetAcquisitionValues(detChan, "reset_delay", &resetDelay,
                                         defs, fs,
                                         &(m->currentFirmware[modChan]),
                                         "RESET", det,
                                         m->detector_chan[modChan], m, modChan);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error updating reset delay to %0.3f after "
                    "switching to reset firmware for detChan %d", resetDelay,
                    detChan);
            pslLogError("psl__SetPreampType", info_string, status);
            return status;
        }

    } else if (newPreampType == XIA_PREAMP_RC) {
        pslLogError("psl__SetPreampType", "RC feedback preamplifiers are not "
                    "currently support with the Stj.",
                    XIA_NOSUPPORTED_PREAMP_TYPE);
        return XIA_NOSUPPORTED_PREAMP_TYPE;

    } else {
        sprintf(info_string, "Unknown preamplifier type (%0.1f) for detChan %d",
                newPreampType, detChan);
        pslLogError("psl__SetPreampType", info_string, XIA_UNKNOWN_PREAMP_TYPE);
        return XIA_UNKNOWN_PREAMP_TYPE;
    }

    /* If we don't update the preamp_type now, we will get stuck in an
     * infinite loop of acquisition value updating.
     */
    status = pslSetDefault("preamp_type", &newPreampType, defs);
    ASSERT(status == XIA_SUCCESS);

    status = pslUserSetup(detChan, defs, fs, &(m->currentFirmware[modChan]),
                          detType, det, m->detector_chan[modChan], m, modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reloading acquisition values after "
                "switching preamplifier types on detChan %d", detChan);
        pslLogError("psl__SetPreampType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Synchronize the detector preamplifier type in the Detector
 * configuration with the preamp_type acquisition value.
 */
PSL_STATIC int psl__SynchPreampType(int detChan, int det_chan, Module *m,
                                    Detector *det, XiaDefaults *defs)
{
    int status;

    double type;

    UNUSED(m);
    UNUSED(det_chan);


    ASSERT(det != NULL);
    ASSERT(defs != NULL);


    switch (det->type) {
    case XIA_DET_RESET:
        type = XIA_PREAMP_RESET;
        break;
    case XIA_DET_RCFEED:
        type = XIA_PREAMP_RC;
        break;
    default:
        FAIL();
        break;
    }

    status = pslSetDefault("preamp_type", &type, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error synchronizing detector preamplifier type for "
                "detChan %d", detChan);
        pslLogError("psl__SynchPreampType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Switches firmware (DSP, FiPPI) to the preamplifer type.
 */
PSL_STATIC int psl__SwitchFirmware(int detChan, double type, int modChan,
                                   double pt, FirmwareSet *fs, Module *m)
{
    int status;

    char fippi[MAX_PATH_LEN];
    char dsp[MAX_PATH_LEN];
    char rawFippi[MAXFILENAME_LEN];
    char rawDSP[MAXFILENAME_LEN];


    ASSERT(fs != NULL);
    ASSERT(m  != NULL);


    switch ((int)type) {

    case (int)XIA_PREAMP_RESET:
        sprintf(info_string, "Switching to reset preamp");
        pslLogDebug("psl__SwitchFirmware", info_string);

        status = psl__GetFiPPIName(modChan, pt, fs, "RESET", fippi, rawFippi);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Unable to get the name of the FiPPI that supports "
                    "reset preamplifiers for peaking time = %0.3f microseconds for "
                    "detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            if (status == XIA_FILEERR) {
                /* Reset status to a more meaningful code */
                status = XIA_NOSUPPORTED_PREAMP_TYPE;
            }
            return status;
        }

        status = psl__GetDSPName(modChan, pt, fs, "RESET", dsp, rawDSP);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Unable to get the DSP that supports reset "
                    "preamplifiers for peaking time = %0.3f microseconds for "
                    "detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = pslDownloadFirmware(detChan, "fippi_a_dsp_no_wake", fippi, m,
                                     rawFippi, NULL);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error downloading new FiPPI for peaking time = "
                    "%0.3f microseconds for detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = pslDownloadFirmware(detChan, "dsp", dsp, m, rawDSP, NULL);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error downloading new DSP for peaking time = "
                    "%0.3f microseconds for detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = psl__WakeDSP(detChan);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error waking new DSP for detChan %d", detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        break;

    case (int)XIA_PREAMP_RC:
        sprintf(info_string, "Switching to RC preamp");
        pslLogDebug("psl__SwitchFirmware", info_string);

        status = psl__GetFiPPIName(modChan, pt, fs, "RC", fippi, rawFippi);
        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Unable to get the name of the FiPPI that supports "
                    "reset preamplifiers for peaking time = %0.3f microseconds for "
                    "detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            if (status == XIA_FILEERR) {
                /* Reset status to a more meaningful code */
                status = XIA_NOSUPPORTED_PREAMP_TYPE;
            }
            return status;
        }

        sprintf(info_string, "Switching to RC fippi: '%s', '%s'", fippi, rawFippi);
        pslLogDebug("psl__SwitchFirmware", info_string);

        status = psl__GetDSPName(modChan, pt, fs, "RC", dsp, rawDSP);

        sprintf(info_string, "Switching to RC DSP: '%s', '%s'", dsp, rawDSP);
        pslLogDebug("psl__SwitchFirmware", info_string);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Unable to get the DSP that supports reset "
                    "preamplifiers for peaking time = %0.3f microseconds for "
                    "detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = pslDownloadFirmware(detChan, "fippi_a_dsp_no_wake", fippi, m,
                                     rawFippi, NULL);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error downloading new FiPPI for peaking time = "
                    "%0.3f microseconds for detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = pslDownloadFirmware(detChan, "dsp", dsp, m, rawDSP, NULL);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error downloading new DSP for peaking time = "
                    "%0.3f microseconds for detChan %d", pt, detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        status = psl__WakeDSP(detChan);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error waking new DSP for detChan %d", detChan);
            pslLogError("psl__SwitchFirmware", info_string, status);
            return status;
        }

        break;

    default:
        FAIL();
        break;
    }

    return XIA_SUCCESS;
}


/*
 * Retrieve the name of the DSP for the requested detector preamplifier
 * type.
 */
PSL_STATIC int psl__GetDSPName(int modChan, double pt, FirmwareSet *fs,
                               char *detType, char *file, char *rawFile)
{
    int status;

    ASSERT(fs != NULL);

    UNUSED(modChan);

    status = xiaFddGetAndCacheFirmware(fs, "system_dsp", pt,
                                       detType, file, rawFile);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the DSP filename from '%s' with a "
                "peaking time of %0.3f microseconds", fs->filename, pt);
        pslLogError("psl__GetDSPName", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Downloads the requested file to FiPPI A, but doesn't wake the
 * DSP up after the download is complete. (Compare with
 * psl__DownloadFiPPIA().)
 */
PSL_STATIC int psl__DownloadFiPPIADSPNoWake(int detChan, char *file,
                                            char *rawFile, Module *m)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int modChan = 0;


    ASSERT(file    != NULL);
    ASSERT(rawFile != NULL);
    ASSERT(m       != NULL);


    status = pslGetModChan(detChan, m, &modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting module channel for detChan = %d",
                detChan);
        pslLogError("psl__DownloadFiPPIADSPNoWake", info_string, status);
        return status;
    }

    if (STREQ(rawFile, m->currentFirmware[modChan].currentFiPPI)) {
        sprintf(info_string, "Requested FiPPI '%s' is already running on detChan %d",
                file, detChan);
        pslLogInfo("psl__DownloadFiPPIADSPNoWake", info_string);
        return XIA_SUCCESS;
    }

    statusX = dxp_replace_fpgaconfig(&detChan, "a_and_b_dsp_no_wake", file);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error switching to new FiPPI '%s' for detChan %d",
                file, detChan);
        pslLogError("psl__DownloadFiPPIADSPNoWake", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Since we just downloaded the FiPPI for all 32 channels, set the current
    * firmware for all 32 channels to the new file name. This prevents Handel
    * from thinking that it needs to download the firmware 32 times. When we add
    * support for FiPPI B, this will be reduced to the 2 channels covered by
    * FiPPI A.
    */
    for (i = 0; i < m->number_of_channels; i++) {
        strcpy(m->currentFirmware[i].currentFiPPI, rawFile);
    }

    return XIA_SUCCESS;
}


/*
 * Downloads the requested DSP code to the hardware.
 */
PSL_STATIC int psl__DownloadDSP(int detChan, char *file, char *rawFile,
                                Module *m)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int modChan;


    ASSERT(file    != NULL);
    ASSERT(rawFile != NULL);
    ASSERT(m       != NULL);


    sprintf(info_string, "Changing DSP to '%s' for detChan %d", file, detChan);
    pslLogDebug("psl__DownloadDSP", info_string);

    status = pslGetModChan(detChan, m, &modChan);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting module channel for detChan = %d",
                detChan);
        pslLogError("psl__DownloadDSP", info_string, status);
        return status;
    }

    if (STREQ(rawFile, m->currentFirmware[modChan].currentDSP)) {
        sprintf(info_string, "Requested DSP '%s' is already running on detChan %d",
                file, detChan);
        pslLogInfo("psl__DownloadDSP", info_string);
        return XIA_SUCCESS;
    }

    statusX = dxp_replace_dspconfig(&detChan, file);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error downloading new DSP '%s' for detChan %d",
                file, detChan);
        pslLogError("psl__DownloadDSP", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Even though the Stj only has a single DSP, we need to update the "DSP"
    * for all of the channels in the module.
    */
    for (i = 0; i < m->number_of_channels; i++) {
        strcpy(m->currentFirmware[i].currentDSP, rawFile);
    }

    return XIA_SUCCESS;
}


/*
 * Extract the OVERFLOWS reported in the statistics block.
 */
PSL_STATIC int psl__ExtractOverflows(int modChan, unsigned long *stats,
                                     double *overs)
{
    unsigned long oversOffset;


    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);
    ASSERT(stats != NULL);
    ASSERT(overs != NULL);


    oversOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_OVERFLOWS_OFFSET;
    *overs = pslU64ToDouble(stats + oversOffset);

    return XIA_SUCCESS;
}


/*
 * Extract the UNDERFLOWS reported in the statistics block.
 */
PSL_STATIC int psl__ExtractUnderflows(int modChan, unsigned long *stats,
                                      double *unders)
{
    unsigned long undersOffset;


    ASSERT(modChan < 32);
    ASSERT(modChan >= 0);
    ASSERT(stats != NULL);
    ASSERT(unders != NULL);


    undersOffset = STJ_STATS_CHAN_OFFSET * modChan + STJ_STATS_UNDERFLOWS_OFFSET;
    *unders = pslU64ToDouble(stats + undersOffset);

    return XIA_SUCCESS;
}


/*
 * Tell the DSP to wake-up.
 */
PSL_STATIC int psl__WakeDSP(int detChan)
{
    int statusX;

    short task = STJ_CT_WAKE_DSP;

    statusX = dxp_start_control_task(&detChan, &task, NULL, NULL);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error starting control task to wake the DSP for "
                "detChan %d", detChan);
        pslLogError("psl__WakeDSP", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    statusX = dxp_stop_control_task(&detChan);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping control task to wake the DSP for "
                "detChan %d", detChan);
        pslLogError("psl__WakeDSP", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Set peak mode for determining the energy from the energy filter output
 *
 * PEAKMODE = 0: XIA_PEAK_SENSING_MODE The largest filter value from a given
 * pulse will be used as the energy.
 *
 * PEAKMODE = 1: XIA_PEAK_SAMPLING_MODE. The energy filter value will be sampled
 * at a specific time determined by the setting of PEAKSAM.
 */
PSL_STATIC int psl__SetPeakMode(int detChan, int modChan, char *name,
                                void *value, char *detType,
                                XiaDefaults *defs, Module *m,
                                Detector *det, FirmwareSet *fs)
{
    int status;

    double peakMode = *((double *)value);
    double pt;

    UNUSED(name);
    UNUSED(detType);


    ASSERT(value != NULL);

    if ((peakMode != XIA_PEAK_SENSING_MODE) &&
            (peakMode != XIA_PEAK_SAMPLING_MODE)) {
        sprintf(info_string, "User specified peak mode %.0f is not within the "
                "valid range (0,1) for detChan %d", peakMode, detChan);
        pslLogError("psl__SetPeakMode", info_string, XIA_PEAKMODE_OOR);
        return XIA_PEAKMODE_OOR;
    }

    status = pslSetDefault("peak_mode", (void *)value, defs);
    ASSERT(status == XIA_SUCCESS);

    /* The actual update is done in psl__UpdateFilterParams
     * to make sure PEAKSAM can be recalculated
     */
    status = pslGetDefault("peaking_time", (void *)&pt, defs);
    ASSERT(status == XIA_SUCCESS);

    status = psl__UpdateFilterParams(detChan, modChan, pt, defs, fs, m, det);


    return XIA_SUCCESS;
}


/*
 * Returns the statistics for all of the channels on the
 * module that detChan is a part of. value is expected to be a
 * double array with at least statsPerChan * m->number_of_channels elements.
 * They are stored in the following format:
 *
 * [ch0_runtime, ch0_trigger_livetime, ch0_energy_livetime,
 *  ch0_triggers, ch0_events, ch0_icr, ch0_ocr, ch0_underflows,
 *  ch0_overflows, ..., ch3_runtime, etc.]
 */
PSL_STATIC int psl__GetModuleStatistics2(int detChan, void *value,
                                         XiaDefaults * defs, Module *m)
{
    int status;
    int i;
    int statsPerChan = 9;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    double *modStats = NULL;

    double tLT;
    double rt;
    double trigs;
    double evts;
    double unders;
    double overs;

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value != NULL);


    modStats = (double *)value;

    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block for detChan %d",
                detChan);
        pslLogError("psl__GetModuleStatistics2", info_string, status);
        return status;
    }

    for (i = 0; i < (int)m->number_of_channels; i++) {
        int chanBase = i * statsPerChan;

        status = psl__ExtractRealtime(i, stats, &rt);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting runtime for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase] = rt;

        status = psl__ExtractTLivetime(i, stats, &tLT);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting trigger livetime for "
                    "detChan %d, modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase + 1] = tLT;

        status = psl__ExtractELivetime(i, stats, &(modStats[chanBase + 2]));

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting energy livetime for "
                    "detChan %d, modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        status = psl__ExtractTriggers(i, stats, &trigs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting triggers for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase + 3] = trigs;

        status = psl__ExtractEvents(i, stats, &evts);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting events for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase + 4] = evts;

        status = psl__ExtractUnderflows(i, stats, &unders);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting underflows for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase + 7] = unders;

        status = psl__ExtractOverflows(i, stats, &overs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error extracting overflows for detChan %d, "
                    "modChan %d", detChan, i);
            pslLogError("psl__GetModuleStatistics2", info_string, status);
            return status;
        }

        modStats[chanBase + 8] = overs;

        if (tLT != 0.0) {
            modStats[chanBase + 5] = trigs / tLT;
        } else {
            modStats[chanBase + 5] = 0.0;
        }

        if (rt != 0.0) {
            modStats[chanBase + 6] = (evts + overs + unders) / rt;
        } else {
            modStats[chanBase + 6] = 0.0;
        }
    }

    return XIA_SUCCESS;
}


/*
 * Returns the # of triggers as a double in value.
 */
PSL_STATIC int psl__GetTriggers(int detChan, void *value,
                                XiaDefaults * defs, Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block while getting "
                "the # of triggers for detChan %d.", detChan);
        pslLogError("psl__GetTriggers", info_string, status);
        return status;
    }

    status = pslGetModChan(detChan, m, &modChan);
    /* Failure to find a modChan in a properly configured system is
     * impossible.
     */
    ASSERT(status == XIA_SUCCESS);

    status = psl__ExtractTriggers(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the # of triggers from the "
                "module statistics block for detChan %d.", detChan);
        pslLogError("psl__GetTriggers", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the # of underflows in value.
 */
PSL_STATIC int psl__GetUnderflows(int detChan, void *value,
                                  XiaDefaults * defs, Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block while getting "
                "the # of underflows for detChan %d.", detChan);
        pslLogError("psl__GetUnderflows", info_string, status);
        return status;
    }

    status = pslGetModChan(detChan, m, &modChan);
    /* Failure to find a modChan in a properly configured system is
     * impossible.
     */
    ASSERT(status == XIA_SUCCESS);

    status = psl__ExtractUnderflows(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the # of underflows from the "
                "module statistics block for detChan %d.", detChan);
        pslLogError("psl__GetUnderflows", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the # of overflows in value.
 */
PSL_STATIC int psl__GetOverflows(int detChan, void *value,
                                 XiaDefaults * defs, Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block while getting "
                "the # of overflows for detChan %d.", detChan);
        pslLogError("psl__GetOverflows", info_string, status);
        return status;
    }

    status = pslGetModChan(detChan, m, &modChan);
    /* Failure to find a modChan in a properly configured system is
     * impossible.
     */
    ASSERT(status == XIA_SUCCESS);

    status = psl__ExtractOverflows(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the # of overflows from the "
                "module statistics block for detChan %d.", detChan);
        pslLogError("psl__GetOverflows", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetMCAEvents(int detChan, void *value, XiaDefaults *defs,
                                 Module *m)
{
    int status;

    unsigned int modChan;

    unsigned long stats[STJ_STATS_BLOCK_SIZE];

    UNUSED(defs);
    UNUSED(m);


    ASSERT(value);


    status = psl__GetStatisticsBlock(detChan, stats);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading statistics block while getting "
                "the # of MCA events for detChan %d.", detChan);
        pslLogError("psl__GetMCAEvents", info_string, status);
        return status;
    }

    status = pslGetModChan(detChan, m, &modChan);
    /* Failure to find a modChan in a properly configured system is
     * impossible.
     */
    ASSERT(status == XIA_SUCCESS);

    status = psl__ExtractEvents(modChan, stats, (double *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error extracting the # of MCA events from the "
                "module statistics block for detChan %d.", detChan);
        pslLogError("psl__GetMCAEvents", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Updates the acquisition value list with the raw DSP parameter
 * specified in name.
 */
PSL_STATIC int psl__UpdateRawParamAcqValue(int detChan, char *name,
                                           void *value, XiaDefaults *defs)
{
    int status;


    ASSERT(name);
    ASSERT(value);
    ASSERT(defs);


    status = pslSetDefault(name, value, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting '%s' to %0.3f as an acquisition "
                "value for detChan %d.", name, *((double *)value), detChan);
        pslLogError("psl__UpdateRawParamAcqValue", info_string, status);
        return status;
    }

    status = pslSetParameter(detChan, name, (parameter_t)*((double *)value));

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the DSP parameter '%s' to %hu "
                "for detChan %d.", name, (parameter_t)*((double *)value),
                detChan);
        pslLogError("psl__UpdateRawParamAcqValue", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetTraceTriggerEnable(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t TRIGENA = 0;
    double trigEnable = 0.0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    trigEnable = *((double *)value);

    if (trigEnable != 0.0 && trigEnable != 1.0) {
        sprintf(info_string, "Trace trigger enable %0f is invalid", trigEnable);
        pslLogError("psl__SetTraceTriggerEnable", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRIGENA = (parameter_t)trigEnable;
    status = pslSetParameter(detChan, "TRIGENA", TRIGENA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting TRIGENA to %hu for detChan %d",
                TRIGENA, detChan);
        pslLogError("psl__SetTraceTriggerEnable", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetTraceTriggerType(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs)
{
    int status;

    double trigType = 0.0;

    parameter_t TRACETRIG = 0;

    UNUSED(name);
    UNUSED(modChan);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);

    ASSERT(value != NULL);

    trigType = *((double *)value);

    /*
     * TRACETRIG is a bit mask defined in TriggerType in handel_constants.h
     */
    if (trigType < 0 || trigType > 255.0) {
        sprintf(info_string, "Trace trigger type %0f is invalid", trigType);
        pslLogError("psl__SetTraceTriggerType", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRACETRIG = (parameter_t)trigType;
    status = pslSetParameter(detChan, "TRACETRIG", TRACETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting TRACETRIG to %hu for detChan %d",
                TRACETRIG, detChan);
        pslLogError("psl__SetTraceTriggerType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetTraceTriggerPosition(int detChan, int modChan, char *name,
                                            void *value, char *detType,
                                            XiaDefaults *defs, Module *m,
                                            Detector *det, FirmwareSet *fs)
{
    int status;
    double trigPosition = 0.0;

    parameter_t TRACEPRETRIG = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(defs);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);


    ASSERT(value != NULL);
    trigPosition = *((double *)value);

    if (trigPosition < 0 || trigPosition > 255.0) {
        sprintf(info_string, "Trace trigger position %0f is out-of-range",
                trigPosition);
        pslLogError("psl__SetTraceTriggerPosition", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRACEPRETRIG = (parameter_t)trigPosition;
    status = pslSetParameter(detChan, "TRACEPRETRIG", TRACEPRETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting trace trigger position to %0f for "
                " detChan %d", trigPosition, detChan);
        pslLogError("psl__SetTraceTriggerPosition", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Do a special run to try to set all 32 channels Offset DACs
 * such that their ADC average = value
 */
PSL_STATIC int psl__AdjustOffsets(int detChan, void *value, XiaDefaults *defs, Detector *det)
{
    int status;
    double adcOffset;

    parameter_t SETOFFADC = 0;

    short task = STJ_CT_ADJUST_OFFSETS;

    UNUSED(value);
    UNUSED(defs);
    UNUSED(det);

    ASSERT(value != NULL);
    adcOffset = *((double *)value);

    if (adcOffset < 0 || adcOffset > 4095.0) {
        sprintf(info_string, "ADC offset %0f is out-of-range", adcOffset);
        pslLogError("psl__AdjustOffsets", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    SETOFFADC = (parameter_t)adcOffset;
    status = pslSetParameter(detChan, "SETOFFADC", SETOFFADC);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting ADC offset to %0f for "
                " detChan %d", adcOffset, detChan);
        pslLogError("psl__AdjustOffsets", info_string, status);
        return status;
    }

    status = dxp_start_control_task(&detChan, &task, NULL, NULL);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting control task %d for detChan %d",
                task, detChan);
        pslLogError("psl__AdjustOffsets", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    status = dxp_stop_control_task(&detChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping control task %d for "
                "detChan %d", task, detChan);
        pslLogError("psl__AdjustOffsets", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Start steeping the STJ Bias DACs for all 32 channels through the
 * user-specified range and acquires the average preamp output voltage
 * value
 *
 * The bias scan run will need to be stopped before other runs can proceed
 */
PSL_STATIC int psl__BeginBiasScan(int detChan, void *value, XiaDefaults *defs, Detector *det)
{
    int status;

    parameter_t WARNING = 0;

    short task = STJ_CT_BIAS_SCAN;

    UNUSED(value);
    UNUSED(defs);
    UNUSED(det);

    status = dxp_start_control_task(&detChan, &task, NULL, NULL);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting control task %d for detChan %d",
                task, detChan);
        pslLogError("psl__BeginBiasScan", info_string, status);
        return status;
    }

    /* Check if analog module is connected */
    status = pslGetParameter(detChan, "WARNING", &WARNING);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting WARNING to %hu for detChan %d",
                WARNING, detChan);
        pslLogError("psl__BeginBiasScan", info_string, status);
        return status;
    }

    if (WARNING == STJ_ANALOG_DISCONNECTED) {
        pslLogWarning("psl__BeginBiasScan", "Analog module is not connected for "
                      " setting bias DAC");
    }


    return XIA_SUCCESS;
}


/*
 * End the STJ bias scan special run, this is just a generic
 *   call to stop current control task.
 */
PSL_STATIC int psl__EndBiasScan(int detChan, void *value, XiaDefaults *defs, Detector *det)
{
    int status;

    UNUSED(value);
    UNUSED(defs);
    UNUSED(det);

    status = dxp_stop_control_task(&detChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping bias scan for "
                "detChan %d", detChan);
        pslLogError("psl__EndBiasScan", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Special run to set STJ bias DACS for all channels in the module
 *
 * Updates the STJDAC parameter value and writes it to the corresponding STJ
 * Bias DAC on the Analog Module.
 */
PSL_STATIC int psl__SetBiasDac(int detChan, void *value, XiaDefaults *defs, Detector *det)
{
    int status;

    parameter_t WARNING = 0;

    short task = STJ_CT_BIAS_SET_DAC;

    UNUSED(value);
    UNUSED(defs);
    UNUSED(det);

    status = dxp_start_control_task(&detChan, &task, NULL, NULL);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error starting control task %d for detChan %d",
                task, detChan);
        pslLogError("psl__SetBiasDac", info_string, status);
        return status;
    }

    /* Check if analog module is connected */
    status = pslGetParameter(detChan, "WARNING", &WARNING);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting WARNING to %hu for detChan %d",
                WARNING, detChan);
        pslLogError("psl__SetBiasDac", info_string, status);
        return status;
    }

    if (WARNING == STJ_ANALOG_DISCONNECTED) {
        pslLogWarning("psl__SetBiasDac", "Analog module is not connected for "
                      " setting bias DAC");
    }

    status = dxp_stop_control_task(&detChan);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping control task for detChan %d", detChan);
        pslLogError("psl__SetBiasDac", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the length of bias scan data from Handel.
 */
PSL_STATIC int psl__GetBiasScanTraceLen(int detChan, void *value,
                                        XiaDefaults *defs)
{
    int status;

    parameter_t STJDACNUM = 0;

    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetParameter(detChan, "STJDACNUM", &STJDACNUM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting STJDACNUM to %hu for detChan %d",
                STJDACNUM, detChan);
        pslLogError("psl__GetBiasScanTraceLen", info_string, status);
        return status;
    }

    *((unsigned long *)value) = STJDACNUM;

    return XIA_SUCCESS;
}


/*
 * Get the bias scan trace from the board.
 */
PSL_STATIC int psl__GetBiasScanTrace(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    int modChan;

    unsigned long addr;

    parameter_t STJDACNUM = 0;
    char memStr[MAXITEM_LEN];

    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetParameter(detChan, "STJDACNUM", &STJDACNUM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting STJDACNUM to %hu for detChan %d",
                STJDACNUM, detChan);
        pslLogError("psl__GetBiasScanTrace", info_string, status);
        return status;
    }

    /* less than optimal way to get the modChan since we don't have it passed in */
    modChan = detChan % 32;
    addr = STJ_BIAS_SCAN_DATA_OFFSET + (modChan * STJ_BIAS_SCAN_DATA_LEN);

    sprintf(memStr, "burst:%#lx:%lu", addr, (unsigned long)STJDACNUM);

    status = dxp_read_memory(&detChan, memStr, value);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading bias scan trace for channel %d", detChan);
        pslLogError("psl__GetBiasScanTrace", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the bias scan noise data from the board.
 */
PSL_STATIC int psl__GetBiasScanNoise(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    int modChan;

    unsigned long addr;
    parameter_t STJDACNUM = 0;

    char memStr[MAXITEM_LEN];

    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetParameter(detChan, "STJDACNUM", &STJDACNUM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting STJDACNUM to %hu for detChan %d",
                STJDACNUM, detChan);
        pslLogError("psl__GetBiasScanTrace", info_string, status);
        return status;
    }

    /* less than optimal way to get the modChan since we don't have it passed in */
    modChan = detChan % 32;
    addr = STJ_BIAS_SCAN_NOISE_OFFSET + (modChan * STJ_BIAS_SCAN_DATA_LEN);

    sprintf(memStr, "burst:%#lx:%lu", addr, (unsigned long)STJDACNUM);

    status = dxp_read_memory(&detChan, memStr, value);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading bias scan noise for channel %d", detChan);
        pslLogError("psl__GetBiasScanNoise", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasScanStartOffset(int detChan, int modChan, char *name,
                                           void *value, char *detType,
                                           XiaDefaults *defs, Module *m,
                                           Detector *det, FirmwareSet *fs)
{
    int status;

    double stjdacstart = 0;
    parameter_t STJDACSTART = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    /* All DAC values are expressed as signed short int */
    stjdacstart = ROUND(*((double *)value) * STJ_DAC_PER_MV);

    if (stjdacstart < STJ_DAC_RANGE_MIN || stjdacstart > STJ_DAC_RANGE_MAX) {
        sprintf(info_string, "Bias scan starting offset %f0 is outside of "
                "range (%hi, %hi)", stjdacstart, STJ_DAC_RANGE_MIN, STJ_DAC_RANGE_MAX);
        pslLogError("psl__SetBiasScanStartOffset", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    STJDACSTART = (parameter_t) ((short) stjdacstart);
    status = pslSetParameter(detChan, "STJDACSTART", STJDACSTART);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting STJDACSTART to %hu for detChan %d",
                STJDACSTART, detChan);
        pslLogError("psl__SetBiasScanStartOffset", info_string, status);
        return status;
    }

    *((double *)value) = stjdacstart / STJ_DAC_PER_MV;
    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasScanSteps(int detChan, int modChan, char *name,
                                     void *value, char *detType,
                                     XiaDefaults *defs, Module *m,
                                     Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t STJDACNUM = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    STJDACNUM = (parameter_t) *((double *)value);

    if (STJDACNUM < 0 || STJDACNUM > 8192) {
        sprintf(info_string, "Bias scan starting offset %hu is outside of "
                "range (%hu, %hu)", STJDACNUM, 0, 8192);
        pslLogError("psl__SetBiasScanSteps", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    status = pslSetParameter(detChan, "STJDACNUM", STJDACNUM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting STJDACNUM to %hu for detChan %d",
                STJDACNUM, detChan);
        pslLogError("psl__SetBiasScanSteps", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasScanStepSize(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs)
{
    int status;

    double stjdacstep = 0;
    parameter_t STJDACSTEP = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    /* All DAC values are expressed as signed short int */
    stjdacstep = ROUND(*((double *)value) * STJ_DAC_PER_MV);

    if (stjdacstep == 0) {
        sprintf(info_string, "Bias scan step size %f0 cannot be 0", stjdacstep);
        pslLogError("psl__SetBiasScanStepSize", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    if (stjdacstep < STJ_DAC_RANGE_MIN || stjdacstep > STJ_DAC_RANGE_MAX) {
        sprintf(info_string, "Bias scan step size %f0 is outside of "
                "range (%hi, %hi)", stjdacstep, STJ_DAC_RANGE_MIN, STJ_DAC_RANGE_MAX);
        pslLogError("psl__SetBiasScanStepSize", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    STJDACSTEP = (parameter_t) ((short) stjdacstep);
    status = pslSetParameter(detChan, "STJDACSTEP", STJDACSTEP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting STJDACSTEP to %hu for detChan %d",
                STJDACSTEP, detChan);
        pslLogError("psl__SetBiasScanStepSize", info_string, status);
        return status;
    }

    *((double *)value) = stjdacstep / STJ_DAC_PER_MV;
    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasDacZero(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    double stjzero = 0;
    parameter_t STJZERO = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    stjzero = ROUND(*((double *)value) * STJ_DAC_PER_MV);

    if (stjzero < STJ_DAC_RANGE_MIN || stjzero > STJ_DAC_RANGE_MAX) {
        sprintf(info_string, "Bias scan zero %f0 is outside of "
                "range (%hi, %hi)", stjzero, STJ_DAC_RANGE_MIN, STJ_DAC_RANGE_MAX);
        pslLogError("psl__SetBiasDacZero", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    STJZERO = (parameter_t) ((short) stjzero);

    status = pslSetParameter(detChan, "STJZERO", STJZERO);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting STJZERO to %hu for detChan %d",
                STJZERO, detChan);
        pslLogError("psl__SetBiasDacZero", info_string, status);
        return status;
    }

    *((double *)value) = stjzero / STJ_DAC_PER_MV;
    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetBiasDacZero(int detChan, void *value,
                                   XiaDefaults *defs)
{
    int status;
    parameter_t STJZERO = 0;

    UNUSED(defs);

    ASSERT(value);

    status = pslGetParameter(detChan, "STJZERO", &STJZERO);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting STJZERO for detChan %d", detChan);
        pslLogError("psl__GetBiasDacZero", info_string, status);
        return status;
    }

    *((double *)value) = ((short) STJZERO) / STJ_DAC_PER_MV;

    status = pslSetDefault("bias_dac_zero", value, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasDacSetZero(int detChan, int modChan, char *name,
                                      void *value, char *detType,
                                      XiaDefaults *defs, Module *m,
                                      Detector *det, FirmwareSet *fs)
{
    int status;

    double setstjzero = 0;
    parameter_t SETSTJZERO = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    setstjzero = ROUND(*((double *)value) * STJ_DAC_PER_MV);

    if (setstjzero < STJ_DAC_RANGE_MIN || setstjzero > STJ_DAC_RANGE_MAX) {
        sprintf(info_string, "Bias scan set zero %f0 is outside of "
                "range (%hi, %hi)", setstjzero, STJ_DAC_RANGE_MIN, STJ_DAC_RANGE_MAX);
        pslLogError("psl__SetBiasDacSetZero", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    SETSTJZERO = (parameter_t) ((short) setstjzero);

    status = pslSetParameter(detChan, "SETSTJZERO", SETSTJZERO);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting SETSTJZERO to %hu for detChan %d",
                SETSTJZERO, detChan);
        pslLogError("psl__SetBiasDacSetZero", info_string, status);
        return status;
    }

    *((double *)value) = setstjzero / STJ_DAC_PER_MV;

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetBiasDacSetZero(int detChan, void *value,
                                      XiaDefaults *defs)
{
    int status;
    parameter_t SETSTJZERO = 0;

    UNUSED(defs);

    ASSERT(value);

    status = pslGetParameter(detChan, "SETSTJZERO", &SETSTJZERO);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting SETSTJZERO for detChan %d", detChan);
        pslLogError("psl__GetBiasDacSetZero", info_string, status);
        return status;
    }

    *((double *)value) = ((short) SETSTJZERO) / STJ_DAC_PER_MV;

    status = pslSetDefault("bias_dac_set_zero", value, defs);
    ASSERT(status == XIA_SUCCESS);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetBiasScanWaitTime(int detChan, int modChan, char *name,
                                        void *value, char *detType,
                                        XiaDefaults *defs, Module *m,
                                        Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t ANLGWAIT = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    ANLGWAIT = (parameter_t) *((double *)value);

    if (ANLGWAIT > 65535) {
        sprintf(info_string, "Bias scan wait time %hu is outside of "
                "range (%hu, %hu)", ANLGWAIT, 0, 65535);
        pslLogError("psl__SetBiasScanWaitTime", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    status = pslSetParameter(detChan, "ANLGWAIT", ANLGWAIT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting ANLGWAIT to %hu for detChan %d",
                ANLGWAIT, detChan);
        pslLogError("psl__SetBiasScanWaitTime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetBiasSetDac(int detChan, int modChan, char *name,
                                  void *value, char *detType,
                                  XiaDefaults *defs, Module *m,
                                  Detector *det, FirmwareSet *fs)
{
    int status;

    double setstjdac = 0;
    parameter_t SETSTJDAC = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);

    setstjdac = ROUND(*((double *)value) * STJ_DAC_PER_MV);

    /* All DAC values are expressed as signed short int */
    if (setstjdac < STJ_DAC_RANGE_MIN || setstjdac > STJ_DAC_RANGE_MAX) {
        sprintf(info_string, "Bias DAC setting %f0 is outside of "
                "range (%hi, %hi)", setstjdac, STJ_DAC_RANGE_MIN, STJ_DAC_RANGE_MAX);
        pslLogError("psl__SetBiasSetDac", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    SETSTJDAC = (parameter_t) ((short) setstjdac);

    status = pslSetParameter(detChan, "SETSTJDAC", SETSTJDAC);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting SETSTJDAC to %hu for detChan %d",
                SETSTJDAC, detChan);
        pslLogError("psl__SetBiasSetDac", info_string, status);
        return status;
    }

    *((double *)value) = setstjdac / STJ_DAC_PER_MV;
    return XIA_SUCCESS;
}

/* Toggle one of the last 8 bits of bank-level parameters BANKnCHANENA
 * according to acquisition value setting for given channel:
 * e.g. BANK1CHANENA=0x00FF --> Channels 8-15 enabled
 */
PSL_STATIC int psl__SetBiasDacEnable(int detChan, int modChan, char *name,
                                     void *value, char *detType,
                                     XiaDefaults *defs, Module *m,
                                     Detector *det, FirmwareSet *fs)
{
    int status;

    parameter_t BANKnCHANENA = 0;
    parameter_t NEWBANKnCHANENA = 0;

    int bankChan = modChan % STJ_CHANNELS_PER_BANK;
    int bank = (int) modChan / STJ_CHANNELS_PER_BANK;

    boolean_t enable = FALSE_;

    char BANKnDSPNAME[20];

    UNUSED(name);
    UNUSED(detType);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);
    UNUSED(m);

    ASSERT(value);
    ASSERT(bank < 4);

    sprintf(BANKnDSPNAME, "BANK%dCHANENA", bank);
    status = pslGetParameter(detChan, BANKnDSPNAME, &BANKnCHANENA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting %s for detChan %d", BANKnDSPNAME,
                detChan);
        pslLogError("psl__SetBiasDacEnable", info_string, status);
        return status;
    }

    NEWBANKnCHANENA = BANKnCHANENA;
    enable = (*((double *)value) != 0.0);

    if (enable) {
        NEWBANKnCHANENA |=  1 << bankChan;
    } else {
        NEWBANKnCHANENA &=  ~(1 << bankChan);
    }

    if (NEWBANKnCHANENA != BANKnCHANENA) {
        status = pslSetParameter(detChan, BANKnDSPNAME, NEWBANKnCHANENA);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting %s for detChan %d", BANKnDSPNAME,
                    detChan);
            pslLogError("psl__SetBiasDacEnable", info_string, status);
            return status;
        }

        sprintf(info_string, "Set %s to %hu for detChan %d.", BANKnDSPNAME,
                NEWBANKnCHANENA, detChan);
        pslLogDebug("psl__SetBiasDacEnable", info_string);
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetSetPmtTriggerMode(int detChan, int modChan, char *name,
                                         void *value, char *detType,
                                         XiaDefaults *defs, Module *m,
                                         Detector *det, FirmwareSet *fs)
{
    int status;
    double trigmode = 0.0;
    parameter_t TRIGMODE = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);
    trigmode = *((double *)value);

    if (trigmode != 0.0 && trigmode != 1.0) {
        sprintf(info_string, "PMT trigger mode %f3 is invalid", trigmode);
        pslLogError("psl__SetSetPmtTriggerMode", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRIGMODE = (parameter_t) trigmode;
    status = pslSetParameter(detChan, "TRIGMODE", TRIGMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting TRIGMODE to %hu for detChan %d",
                TRIGMODE, detChan);
        pslLogError("psl__SetSetPmtTriggerMode", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPmtDynodeThreshold(int detChan, int modChan, char *name,
                                          void *value, char *detType,
                                          XiaDefaults *defs, Module *m,
                                          Detector *det, FirmwareSet *fs)
{
    int status;

    double dynthresh = 0.0;
    parameter_t DYNTHRESH = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);
    dynthresh = *((double *)value);

    /* DYNTHRESH has 14-bit range */
    if (dynthresh >= 32768.0) {
        sprintf(info_string, "PMT dynode trigger threshold %f3 is invalid", dynthresh);
        pslLogError("psl__GetPmtDynodeThreshold", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    DYNTHRESH = (parameter_t) dynthresh;
    status = pslSetParameter(detChan, "DYNTHRESH", DYNTHRESH);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting DYNTHRESH to %hu for detChan %d",
                DYNTHRESH, detChan);
        pslLogError("psl__GetPmtDynodeThreshold", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPmtDynodeSumThreshold(int detChan, int modChan, char *name,
                                             void *value, char *detType,
                                             XiaDefaults *defs, Module *m,
                                             Detector *det, FirmwareSet *fs)
{
    int status;

    double dynsumthresh = 0.0;
    parameter_t DYNSUMTHRESH = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);
    dynsumthresh = *((double *)value);

    /* DYNSUMTHRESH has 16-bit range */
    if (dynsumthresh > 65536.0) {
        sprintf(info_string, "PMT dynode sum threshold %f3 is invalid", dynsumthresh);
        pslLogError("psl__SetPmtDynodeSumThreshold", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    DYNSUMTHRESH = (parameter_t) dynsumthresh;
    status = pslSetParameter(detChan, "DYNSUMTHRESH", DYNSUMTHRESH);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting DYNSUMTHRESH to %hu for detChan %d",
                DYNSUMTHRESH, detChan);
        pslLogError("psl__SetPmtDynodeSumThreshold", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPmtMultiLen(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    double multilen = 0.0;
    parameter_t MULTLEN = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);
    multilen = *((double *)value);

    /* MULTLEN  has 10-bit range */
    if (multilen > 16384.0) {
        sprintf(info_string, "PMT dynode multiplicity Interval %f3 is invalid", multilen);
        pslLogError("psl__SetPmtMultiLen", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    MULTLEN = (parameter_t) multilen;
    status = pslSetParameter(detChan, "MULTLEN", MULTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting MULTLEN to %hu for detChan %d",
                MULTLEN, detChan);
        pslLogError("psl__SetPmtMultiLen", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__SetPmtMultiReq(int detChan, int modChan, char *name,
                                   void *value, char *detType,
                                   XiaDefaults *defs, Module *m,
                                   Detector *det, FirmwareSet *fs)
{
    int status;

    double multreq = 0.0;
    parameter_t MULTREQ = 0;

    UNUSED(modChan);
    UNUSED(name);
    UNUSED(detType);
    UNUSED(m);
    UNUSED(det);
    UNUSED(fs);
    UNUSED(defs);

    ASSERT(value);
    multreq = *((double *)value);

    /* MULTREQ range is 0 - 32 */
    if (multreq > 32.0) {
        sprintf(info_string, "PMT multiplicity Requirement %f3 is invalid", multreq);
        pslLogError("psl__SetPmtMultiReq", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    MULTREQ = (parameter_t) multreq;
    status = pslSetParameter(detChan, "MULTREQ", MULTREQ);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting MULTREQ to %hu for detChan %d",
                MULTREQ, detChan);
        pslLogError("psl__SetPmtMultiReq", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}
