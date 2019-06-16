/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2017 XIA LLC
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
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xia_module.h"
#include "xia_system.h"
#include "xia_psl.h"
#include "handel_errors.h"
#include "handel_generic.h"
#include "handel_constants.h"
#include "xia_handel_structures.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "psl_common.h"

#include "xerxes.h"
#include "xerxes_errors.h"
#include "xerxes_generic.h"

#include "udxp_command.h"
#include "udxp_common.h"
#include "psl_udxp.h"

#ifndef EXCLUDE_XUP
#include "xia_xup.h"
#endif /* EXCLUDE_XUP */

#ifdef XIA_ALPHA
#include "psl_udxp_alpha.h"
#endif /* XIA_ALPHA */

#include "fdd.h"

/* HACK: USB RS232 update
 * Need a modular variable to identify whether the board is USB2 or RS232
 * This only used for I2C bus access where the data bytes need to be different
 * for USB2, we can remove this and the support for RS232 I2C command when
 * we are certain there is no more RS232 alpha boards.
 */
static boolean_t IS_USB = FALSE_;

#ifdef XIA_ALPHA
/* Cache some constants for the Alpha hardware. */
static parameter_t OUTBUFSTART = 0;
static parameter_t OUTBUFLEN   = 0;
static parameter_t EVENTLEN    = 0;
static unsigned short ALPHA_MAX_EVENTS_IN_BUFFER = 0;

static unsigned short ALPHA_NEXT_N_EVENTS[2];
static unsigned long ALPHA_EVENT_COUNT[2];
#endif /* XIA_ALPHA */

/* Prototypes */

/* Exports */
PSL_EXPORT int PSL_API udxp_PSLInit(PSLFuncs *funcs);

/* Verification functions */
PSL_STATIC boolean_t pslIsInterfaceValid(Module *module);
PSL_STATIC boolean_t pslIsNumChannelsValid(Module *module);

PSL_STATIC int pslQueryPreampType(int detChan, unsigned short *type);

/* Helpers */
PSL_STATIC double pslComputeFraction(byte_t word, int nBits);
PSL_STATIC double pslDoubleFromBytes(byte_t *bytes, int size);
PSL_STATIC double pslDoubleFromBytesOffset(byte_t *bytes, int size, int offset);
PSL_STATIC unsigned long pslUlFromBytesOffset(byte_t *bytes, int size, int offset);
PSL_STATIC int pslNumBytesPerPt(int detChan);
PSL_STATIC double pslMinTraceWait(double clock);
PSL_STATIC int pslUpdateFilterParams(int detChan, double *pioffset,
                                     double *psoffset, XiaDefaults *defs);
PSL_STATIC int pslCheckTraceWaitRange(int detChan, double *tracewait, XiaDefaults *defs);
PSL_STATIC int pslGetClockTick(int detChan, XiaDefaults *defs, double *value);
PSL_STATIC int pslSetFilterParam(int detChan, byte_t n, parameter_t value);
PSL_STATIC int pslGetFilterParam(int detChan, byte_t n, parameter_t *value);
PSL_STATIC double pslCalculateBaseGain(unsigned int gainMode,
                                       parameter_t SWGAIN, parameter_t DGAINBASE, signed short DGAINBASEEXP);
PSL_STATIC int pslGetSCADataDirect(int detChan, int numSca, unsigned long addr, double *sca64);
PSL_STATIC int pslGetSCADataCmd(int detChan, int numSca, byte_t cmd, double *sca64);
PSL_STATIC void pslCalculatePeakingTimes(int detChan, int fippi,
      unsigned short ptPerFippi, double baseclock, byte_t *receive, double *pts);
PSL_STATIC int pslReadoutPeakingTimes(int detChan, XiaDefaults *defs,
                                      boolean_t allFippis, double *pts);
PSL_STATIC int pslReadDirectUsbMemory(int detChan, unsigned long address,
                                    unsigned long num_bytes, byte_t *receive);
PSL_STATIC int pslGetMcaDirect(int detChan, int bytesPerBin,
                                    int numMCAChans, unsigned long startAddr,
                                    unsigned long *data);
#ifdef XIA_ALPHA
PSL_STATIC int pslAlphaPulserComputeDAC(unsigned short amplitude,
                                        unsigned short risetime,
                                        unsigned short *dac);
PSL_STATIC int pslAlphaReadFromEventBuffer(int detChan, unsigned short startIdx,
                                           unsigned short nEvt,
                                           unsigned short *buf);
PSL_STATIC int pslUltraDoTiltIO(int detChan, int rw, byte_t reg, byte_t *data);
PSL_STATIC double pslUltraTiltRawToGs(byte_t l, byte_t h);
PSL_STATIC byte_t pslDOWCRC(byte_t *buffer, int len);
#endif /* XIA_ALPHA */

/* DSP Parameter Data Types */
PSL_STATIC int pslGetParamValues(int detChan, void *value);

/* Run Data functions */
PSL_STATIC int pslGetMCALength(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetMCAData(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetLivetime(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetRuntime(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetICR(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetOCR(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetEvents(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetTriggers(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetBaseHistogramLen(int detChan, void *value,
                                      XiaDefaults *defs);
PSL_STATIC int pslGetBaseline(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetRunActive(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetAllStatistics(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSCALength(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetMaxSCALength(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSCAData(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetModuleStatistics(int detChan, void *value, XiaDefaults *defs);

#ifdef XIA_ALPHA
PSL_STATIC int pslGetAlphaBufferNumEvents(int detChan, void *value,
                                          XiaDefaults *defs);
PSL_STATIC int pslGetAlphaEvents(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetAlphaStatistics(int detChan, void *value,
                                     XiaDefaults *defs);
#endif /* XIA_ALPHA */

/* Special Runs */
PSL_STATIC int psl__AdjustOffsets(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int psl__Snapshot(int detChan, void *value, XiaDefaults *defs);

/* Special Run Functions */
PSL_STATIC int pslGetADCTraceLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetADCTrace(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetBaseHistLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetBaseHist(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotMcaLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotMca(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotStatsLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotStats(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotScaLen(int detChan, void *value, XiaDefaults *defs);
PSL_STATIC int pslGetSnapshotSca(int detChan, void *value, XiaDefaults *defs);

PSL_STATIC int pslDoTrace(int detChan, short type, double *info, XiaDefaults *defs);

PSL_STATIC int pslGetBaseClock(int detChan, double *value);
PSL_STATIC int pslCalculateRanges(byte_t nFiPPIs, int ptPerFippi, int bytePerPt,
                                  double BASE_CLOCK, byte_t CLKSET, byte_t *data,
                                  double *ranges);
PSL_STATIC Udxp_AcquisitionValue *pslFindAV(char *name);

PSL_STATIC int pslInvalidateAll(flag_t member, XiaDefaults *defs);

PSL_STATIC int pslSetResetInterval(int detChan, double *value);
PSL_STATIC int pslSetRCTau(int detChan, XiaDefaults *defs, double *value);
PSL_STATIC int pslGetResetInterval(int detChan, double *value);
PSL_STATIC int pslGetRCTau(int detChan, XiaDefaults *defs, double *value);

/* Acquisition Values "Set" Functions */
PSL_STATIC int pslSetParset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetGenset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetEGapTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetClockSpd(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetTPeakTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetTGapTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetBaseLen(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetTThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetBThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetEThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetNumMCA(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetBinWidth(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetFiPPI(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetBytePerBin(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetADCWait(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetGainbase(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPreampPol(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPreampVal(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetFipControl(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetRuntasks(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetGainTrim(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakInt(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakSam(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetMaxWidth(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakMode(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetBFactor(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakingTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakIntOffset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPeakSamOffset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetTriggerType(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetTriggerPosition(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetNumScas(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetSca(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetScaTimeOn(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetScaTimeOff(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetAutoAdjust(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPresetType(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslSetPresetValue(int detChan, char *name, XiaDefaults *defs, void *value);

#ifdef XIA_ALPHA
PSL_STATIC int pslSetAlphaEventLen(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslSetAlphaPreBufferLen(int detChan, char *name, XiaDefaults *defs,
                                       void *value);
PSL_STATIC int pslSetAlphaDACTarget(int detChan, char *name, XiaDefaults *defs,
                                    void *value);
PSL_STATIC int pslSetAlphaDACTolerance(int detChan, char *name, XiaDefaults *defs,
                                       void *value);

PSL_STATIC int pslSetAlphaParam(int detChan, unsigned int idx,
                                unsigned short value);
PSL_STATIC int pslAlphaFreeEvents(int detChan, unsigned short nEvents);
#endif /* XIA_ALPHA */

/* Acquisition Values "Get" Functions */
PSL_STATIC int pslGetParset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetGenset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetEGapTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetTPeakTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetClockSpd(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetTGapTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetBaseLen(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetTThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetBThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetEThresh(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetNumMCA(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetBinWidth(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetFiPPI(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetBytePerBin(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetADCWait(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetGainbase(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPreampPol(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPreampVal(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetFipControl(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetRuntasks(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetGainTrim(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakInt(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakSam(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetMaxWidth(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakMode(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetBFactor(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakingTime(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakIntOffset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPeakSamOffset(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetTriggerType(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetTriggerPosition(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetNumScas(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetSca(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetScaTimeOn(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetScaTimeOff(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetAutoAdjust(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPresetType(int detChan, char *name, XiaDefaults *defs, void *value);
PSL_STATIC int pslGetPresetValue(int detChan, char *name, XiaDefaults *defs, void *value);

#ifdef XIA_ALPHA
PSL_STATIC int pslGetAlphaEventLen(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslGetAlphaPreBufferLen(int detChan, char *name, XiaDefaults *defs,
                                       void *value);
PSL_STATIC int pslGetAlphaDACTarget(int detChan, char *name, XiaDefaults *defs,
                                    void *value);
PSL_STATIC int pslGetAlphaDACTolerance(int detChan, char *name, XiaDefaults *defs,
                                       void *value);

PSL_STATIC int pslGetAlphaParam(int detChan, unsigned int idx,
                                unsigned short *value);
#endif /* XIA_ALPHA */

/* Board Operation Functions */
PSL_STATIC int pslGetSerialNumber(int detChan, char *name, XiaDefaults *defs,
                                  void *value);
PSL_STATIC int pslGetPTRanges(int detChan, char *name, XiaDefaults *defs,
                              void *value);
PSL_STATIC int pslGetNumFiPPIs(int detChan, char *name, XiaDefaults *defs,
                               void *value);
PSL_STATIC int pslGetNumPtPerFiPPI(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslGetPeakingTimes(int detChan, char *name,
                                         XiaDefaults *defs, void *values);
PSL_STATIC int pslGetCurrentPeakingTimes(int detChan, char *name,
                                         XiaDefaults *defs, void *values);
PSL_STATIC int pslGetHistorySector(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslGetTemperature(int detChan, char *name, XiaDefaults *defs,
                                 void *value);
PSL_STATIC int pslApply(int detChan, char *name, XiaDefaults *defs,
                        void *value);
PSL_STATIC int pslSaveParset(int detChan, char *name, XiaDefaults *defs,
                             void *value);
PSL_STATIC int pslSaveGenset(int detChan, char *name, XiaDefaults *defs,
                             void *value);
PSL_STATIC int pslSetPreset(int detChan, char *name, XiaDefaults *defs,
                            void *value);
PSL_STATIC int pslGetBoardInfo(int detChan, char *name, XiaDefaults *defs,
                               void *value);
PSL_STATIC int pslGetPreampType(int detChan, char *name, XiaDefaults *defs,
                                void *value);
PSL_STATIC int pslRecover(int detChan, char *name, XiaDefaults *defs,
                          void *value);
PSL_STATIC int pslGetHardwareStatus(int detChan, char *name, XiaDefaults *defs,
                                    void *value);
PSL_STATIC int pslGetGainMode(int detChan, char *name, XiaDefaults *defs,
                              void *value);
PSL_STATIC int pslGetBoardFeatures(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslPassthrough(int detChan, char *name, XiaDefaults *defs,
                                   void *value);

#ifndef EXCLUDE_XUP
PSL_STATIC int pslQueryStatus(int detChan);
PSL_STATIC int pslDownloadXUP(int detChan, char *name, XiaDefaults *defs,
                              void *value);
PSL_STATIC int pslSetXUPBackupPath(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslCreateMasterParamSet(int detChan, char *name,
                                       XiaDefaults *defs, void *value);
PSL_STATIC int pslCreateBackup(int detChan, char *name, XiaDefaults *defs,
                               void *value);
#endif /* EXCLUDE_XUP */

#ifndef EXCLUDE_USB2
PSL_STATIC int pslGetUdxpCPLDVersion(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslGetUdxpCPLDVariant(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslGetUSBVersion(int detChan, char *name, XiaDefaults *defs,
                                void *value);
#endif /* EXCLUDE_USB2 */

#ifdef XIA_ALPHA
PSL_STATIC int pslSetAlphaExtTrigger(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslGetAlphaHV(int detChan, char *name, XiaDefaults *defs,
                             void *value);
PSL_STATIC int pslSetAlphaHV(int detChan, char *name, XiaDefaults *defs,
                             void *value);
PSL_STATIC int pslGetCPLDVersion(int detChan, char *name, XiaDefaults *defs,
                                 void *value);
PSL_STATIC int pslAlphaPulserDisable(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslAlphaPulserEnable(int detChan, char *name, XiaDefaults *defs,
                                    void *value);
PSL_STATIC int pslAlphaPulserConfig1(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslAlphaPulserConfig2(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslAlphaPulserSetMode(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslAlphaPulserConfigVeto(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslAlphaPulserEnableVeto(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslAlphaPulserDisableVeto(int detChan, char *name,
                                         XiaDefaults *defs, void *value);
PSL_STATIC int pslAlphaPulserStart(int detChan, char *name, XiaDefaults *defs,
                                   void *value);
PSL_STATIC int pslAlphaPulserStop(int detChan, char *name, XiaDefaults *defs,
                                  void *value);
PSL_STATIC int pslAlphaRequestEvents(int detChan, char *name,
                                     XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraTiltInit(int detChan, char *name,
                                XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraTiltGetOutput(int detChan, char *name, XiaDefaults *defs,
                                     void *value);
PSL_STATIC int pslUltraTiltSetThresholds(int detChan, char *name,
                                         XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraTiltEnableInterlock(int detChan, char *name,
                                           XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraTiltIsTriggered(int detChan, char *name,
                                       XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraSetAsClockMaster(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraRenumerateDevice(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraSetElectrodeSize(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraGetElectrodeSize(int detChan, char *name,
                                        XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraMoistureRead(int detChan, char *name,
                                    XiaDefaults *defs, void *value);
PSL_STATIC int pslUltraGetMBID(int detChan, char *name,
                               XiaDefaults *defs, void *value);
#endif /* XIA_ALPHA */

/* Gain Operations */
PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det,
                                  int modChan, Module *m, XiaDefaults *defs,
                                  void *value);
PSL_STATIC int psl__GainTrimCalibrate(int detChan, Detector *det,
                                      int modChan, Module *m, XiaDefaults *defs,
                                      void *value);
/* Globals */

/* When adding a new acquisition value, be sure to add the proper
 * call to pslSetParset/pslSetGenset to invalidate the cached value,
 * as required. Note that the matching function uses STRNEQ and will
 * return the first match, thus, "gain_trim" must precede "gain", etc.
 */
static Udxp_AcquisitionValue acqVals[] = {
    {"parset",              AV_MEM_R_PAR, 0.0, pslSetParset,     pslGetParset},
    {"genset",              AV_MEM_R_GEN, 0.0, pslSetGenset,     pslGetGenset},
    {"clock_speed",         AV_MEM_REQ,   0.0, pslSetClockSpd,   pslGetClockSpd},
    {"energy_gap_time",     AV_MEM_R_PAR, 0.0, pslSetEGapTime,   pslGetEGapTime},
    {"trigger_peak_time",   AV_MEM_R_PAR, 0.0, pslSetTPeakTime,  pslGetTPeakTime},
    {"trigger_gap_time",    AV_MEM_R_PAR, 0.0, pslSetTGapTime,   pslGetTGapTime},
    {"baseline_length",     AV_MEM_R_PAR, 0.0, pslSetBaseLen,    pslGetBaseLen},
    {"trigger_threshold",   AV_MEM_R_PAR, 0.0, pslSetTThresh,    pslGetTThresh},
    {"baseline_threshold",  AV_MEM_R_PAR, 0.0, pslSetBThresh,    pslGetBThresh},
    {"energy_threshold",    AV_MEM_R_PAR, 0.0, pslSetEThresh,    pslGetEThresh},
    {"number_mca_channels", AV_MEM_R_GEN, 0.0, pslSetNumMCA,     pslGetNumMCA},
    {"mca_bin_width",       AV_MEM_R_GEN, 0.0, pslSetBinWidth,   pslGetBinWidth},
    {"fippi",               AV_MEM_R_FIP, 0.0, pslSetFiPPI,      pslGetFiPPI},
    {"bytes_per_bin",       AV_MEM_REQ,   3.0, pslSetBytePerBin, pslGetBytePerBin},
    {"adc_trace_wait",      AV_MEM_R_ADC, 0.0, pslSetADCWait,    pslGetADCWait},
    {"gain_trim",           AV_MEM_R_PAR, 0.0, pslSetGainTrim,   pslGetGainTrim},
    {"gain",                AV_MEM_R_GEN, 0.0, pslSetGainbase,   pslGetGainbase},
    {"polarity",            AV_MEM_R_GLB, 0.0, pslSetPreampPol,  pslGetPreampPol},
    {"preamp_value",        AV_MEM_R_GLB, 0.0, pslSetPreampVal,  pslGetPreampVal},
    {"fipcontrol",          AV_MEM_R_GLB, 0.0, pslSetFipControl, pslGetFipControl},
    {"runtasks",            AV_MEM_R_GLB, 0.0, pslSetRuntasks,   pslGetRuntasks},
    {"peak_interval",       AV_MEM_R_PAR, 0.0, pslSetPeakInt,    pslGetPeakInt},
    {"peak_sample",         AV_MEM_R_PAR, 0.0, pslSetPeakSam,    pslGetPeakSam},
    {"max_width",           AV_MEM_R_PAR, 0.0, pslSetMaxWidth,   pslGetMaxWidth},
    {"peak_mode",           AV_MEM_R_PAR, 0.0, pslSetPeakMode,   pslGetPeakMode},
    {"baseline_factor",     AV_MEM_R_PAR, 0.0, pslSetBFactor,    pslGetBFactor},
    {"peaking_time",        AV_MEM_R_PAR, 0.0, pslSetPeakingTime,pslGetPeakingTime},
    {"peakint_offset",      AV_MEM_R_PAR, 0.0, pslSetPeakIntOffset, pslGetPeakIntOffset},
    {"peaksam_offset",      AV_MEM_R_PAR, 0.0, pslSetPeakSamOffset, pslGetPeakSamOffset},
    {"trace_trigger_type",  AV_MEM_R_PAR, 0.0, pslSetTriggerType,     pslGetTriggerType},
    {"trace_trigger_position",    AV_MEM_R_PAR, 0.0, pslSetTriggerPosition, pslGetTriggerPosition},
    {"number_of_scas",      AV_MEM_R_PAR, 0.0, pslSetNumScas,     pslGetNumScas},
    {"sca_time_on",         AV_MEM_R_PAR, 0.0, pslSetScaTimeOn,   pslGetScaTimeOn},
    {"sca_time_off",        AV_MEM_R_PAR, 0.0, pslSetScaTimeOff,  pslGetScaTimeOff},
    {"sca",                 AV_MEM_R_PAR, 0.0, pslSetSca,         pslGetSca},
    {"auto_adjust_offset",  AV_MEM_R_PAR, 0.0, pslSetAutoAdjust,  pslGetAutoAdjust},
    {"preset_type",         AV_MEM_R_PAR, 0.0, pslSetPresetType,  pslGetPresetType},
    {"preset_value",        AV_MEM_R_PAR, 0.0, pslSetPresetValue, pslGetPresetValue},

#ifdef XIA_ALPHA
    {
        "alpha_event_length",  AV_MEM_R_ALPHA, 0.0, pslSetAlphaEventLen,
        pslGetAlphaEventLen
    },
    {
        "alpha_pre_buf_len",   AV_MEM_R_ALPHA, 0.0, pslSetAlphaPreBufferLen,
        pslGetAlphaPreBufferLen
    },
    {
        "alpha_dac_target",    AV_MEM_R_ALPHA, 0.0, pslSetAlphaDACTarget,
        pslGetAlphaDACTarget
    },
    {
        "alpha_dac_tol",       AV_MEM_R_ALPHA, 0.0, pslSetAlphaDACTolerance,
        pslGetAlphaDACTolerance
    },
#endif /* XIA_ALPHA */
};

#define NUM_ACQ_VALS  (sizeof(acqVals) / sizeof(acqVals[0]))


static Udxp_RunData runData[] = {
    {"mca_length",          pslGetMCALength},
    {"mca",                 pslGetMCAData},
    {"livetime",            pslGetLivetime},
    {"runtime",             pslGetRuntime},
    {"input_count_rate",    pslGetICR},
    {"output_count_rate",   pslGetOCR},
    {"events_in_run",       pslGetEvents},
    {"total_output_events", pslGetEvents}, /* TODO: update and document pending case 15688 */
    {"triggers",            pslGetTriggers},
    {"baseline_length",     pslGetBaseHistogramLen},
    {"baseline",            pslGetBaseline},
    {"run_active",          pslGetRunActive},
    {"all_statistics",      pslGetAllStatistics},
    {"sca_length",          pslGetSCALength},
    {"max_sca_length",      pslGetMaxSCALength},
    {"sca",                 pslGetSCAData},
    {"realtime",            pslGetRuntime},
    {"mca_events",          pslGetEvents},
    {"trigger_livetime",    pslGetLivetime},
    {"module_statistics_2", pslGetModuleStatistics},
#ifdef XIA_ALPHA
    {"alpha_buffer_num_events", pslGetAlphaBufferNumEvents},
    {"alpha_events",            pslGetAlphaEvents},
    {"alpha_statistics",        pslGetAlphaStatistics},
#endif /* XIA_ALPHA */

};

/* These are the allowed trace types */
static SpecialRun traceTypes[] = {
    {"adc_trace",                      NULL},
    {"adc_average",                    NULL},
    {"fast_filter",                    NULL},
    {"raw_intermediate_filter",        NULL},
    {"baseline_samples",               NULL},
    {"baseline_average",               NULL},
    {"scaled_intermediate_filter",     NULL},
    {"raw_slow_filter",                NULL},
    {"scaled_slow_filter",             NULL},
    /* NOTE that the last tracetype (DEBUG_TRACE_TYPE) is used for debugging
     * pslDoTrace does not set the TRACETYPE DSP paramter if this is passed in.
     */
    {"debug",                          NULL},
};

/* These are the allowed special run types */
static SpecialRun specialRun[] = {
    {"adjust_offsets",              psl__AdjustOffsets},
    {"snapshot",                    psl__Snapshot},
};

static SpecialRunData specialRunData[] = {
    {"adc_trace_length",            pslGetADCTraceLen},
    {"adc_trace",                   pslGetADCTrace},
    {"baseline_history_length",     pslGetBaseHistLen},
    {"baseline_history",            pslGetBaseHist},
    {"snapshot_mca_length",         pslGetSnapshotMcaLen},
    {"snapshot_mca",                pslGetSnapshotMca},
    {"snapshot_statistics_length",  pslGetSnapshotStatsLen},
    {"snapshot_statistics",         pslGetSnapshotStats},
    {"snapshot_sca_length",         pslGetSnapshotScaLen},
    {"snapshot_sca",                pslGetSnapshotSca},
};

static BoardOperation boardOps[] = {
    {"get_serial_number",             pslGetSerialNumber},
    {"get_peaking_time_ranges",       pslGetPTRanges},
    {"get_number_of_fippis",          pslGetNumFiPPIs},
    {"get_number_pt_per_fippi",       pslGetNumPtPerFiPPI},
    {"get_peaking_times",             pslGetPeakingTimes},
    {"get_current_peaking_times",     pslGetCurrentPeakingTimes},
    {"get_history_sector",            pslGetHistorySector},
    {"get_temperature",               pslGetTemperature},
    {"apply",                         pslApply},
    {"save_parset",                   pslSaveParset},
    {"save_genset",                   pslSaveGenset},
    {"set_preset",                    pslSetPreset},
    {"get_board_info",                pslGetBoardInfo},
    {"get_preamp_type",               pslGetPreampType},
    {"recover",                       pslRecover},
    {"passthrough",                   pslPassthrough},
#ifndef EXCLUDE_XUP
    {"download_xup",                  pslDownloadXUP},
    {"set_xup_backup_path",           pslSetXUPBackupPath},
    {"create_master_param_set",       pslCreateMasterParamSet},
    {"create_backup",                 pslCreateBackup},
#endif /* EXCLUDE_XUP */
    {"get_hardware_status",           pslGetHardwareStatus},
    {"get_gain_mode",                 pslGetGainMode},
    {"get_board_features",            pslGetBoardFeatures},
#ifndef EXCLUDE_USB2
    {"get_udxp_cpld_version",         pslGetUdxpCPLDVersion},
    {"get_udxp_cpld_variant",         pslGetUdxpCPLDVariant},
    {"get_usb_version",               pslGetUSBVersion},
#endif /* EXCLUDE_USB2 */
#ifdef XIA_ALPHA
    {"set_alpha_ext_trigger",         pslSetAlphaExtTrigger},
    {"get_alpha_hv",                  pslGetAlphaHV},
    {"set_alpha_hv",                  pslSetAlphaHV},
    {"get_alpha_mboard_cpld_version", pslGetCPLDVersion},
    {"alpha_pulser_enable",           pslAlphaPulserEnable},
    {"alpha_pulser_disable",          pslAlphaPulserDisable},
    {"alpha_pulser_config_1",         pslAlphaPulserConfig1},
    {"alpha_pulser_config_2",         pslAlphaPulserConfig2},
    {"alpha_pulser_set_mode",         pslAlphaPulserSetMode},
    {"alpha_pulser_config_veto",      pslAlphaPulserConfigVeto},
    {"alpha_pulser_enable_veto",      pslAlphaPulserEnableVeto},
    {"alpha_pulser_disable_veto",     pslAlphaPulserDisableVeto},
    {"alpha_pulser_start",            pslAlphaPulserStart},
    {"alpha_pulser_stop",             pslAlphaPulserStop},
    {"alpha_request_events",          pslAlphaRequestEvents},
    {"ultra_tilt_initialize",         pslUltraTiltInit},
    {"ultra_tilt_get_output",         pslUltraTiltGetOutput},
    {"ultra_tilt_set_thresholds",     pslUltraTiltSetThresholds},
    {"ultra_tilt_enable_interlock",   pslUltraTiltEnableInterlock},
    {"ultra_tilt_is_triggered",       pslUltraTiltIsTriggered},
    {"ultra_set_as_clock_master",     pslUltraSetAsClockMaster},
    {"ultra_renumerate_device",       pslUltraRenumerateDevice},
    {"ultra_set_electrode_size",      pslUltraSetElectrodeSize},
    {"ultra_get_electrode_size",      pslUltraGetElectrodeSize},
    {"ultra_moisture_read",           pslUltraMoistureRead},
    {"ultra_get_mb_id",               pslUltraGetMBID},
#endif /* XIA_ALPHA */
};

/* These are the allowed gain operations for this hardware */
static GainOperation gainOps[] = {
    {"calibrate",             psl__GainCalibrate},
    {"calibrate_gain_trim",   psl__GainTrimCalibrate},
};

/* These are the DSP parameter data types for pslGetParamData(). */
static ParamData_t PARAM_DATA[] =
{
    {   "values", pslGetParamValues
    },
};

/*
 * This routine takes a PSLFuncs structure and points the function pointers
 * in it at the local udxp "versions" of the functions.
 */
PSL_EXPORT int PSL_API udxp_PSLInit(PSLFuncs *funcs)
{
    funcs->validateDefaults     = pslValidateDefaults;
    funcs->validateModule       = pslValidateModule;
    funcs->downloadFirmware     = pslDownloadFirmware;
    funcs->setAcquisitionValues = pslSetAcquisitionValues;
    funcs->getAcquisitionValues = pslGetAcquisitionValues;
    funcs->gainOperation        = pslGainOperation;
    funcs->gainCalibrate        = pslGainCalibrate;
    funcs->startRun             = pslStartRun;
    funcs->stopRun              = pslStopRun;
    funcs->getRunData           = pslGetRunData;
    funcs->doSpecialRun         = pslDoSpecialRun;
    funcs->getSpecialRunData    = pslGetSpecialRunData;
    funcs->getDefaultAlias      = pslGetDefaultAlias;
    funcs->getParameter         = pslGetParameter;
    funcs->setParameter         = pslSetParameter;
    funcs->moduleSetup          = pslModuleSetup;
    funcs->userSetup            = pslUserSetup;
    funcs->canRemoveName        = pslCanRemoveName;
    funcs->getNumDefaults       = pslGetNumDefaults;
    funcs->getNumParams         = pslGetNumParams;
    funcs->getParamData         = pslGetParamData;
    funcs->getParamName         = pslGetParamName;
    funcs->boardOperation       = pslBoardOperation;
    funcs->freeSCAs             = pslDestroySCAs;
    funcs->unHook               = pslUnHook;

    return XIA_SUCCESS;
}


/*
 * This routine validates module information specific to the dxpx10p
 * product:
 *
 * 1) interface should be of type serial
 * 2) number_of_channels = 1
 */
PSL_STATIC int pslValidateModule(Module *module)
{
    int status;

    if (!pslIsInterfaceValid(module)) {
        status = XIA_MISSING_INTERFACE;
        sprintf(info_string,
                "Wrong interface for module %s",
                module->alias);
        pslLogError("pslValidateModule", info_string, status);
        return status;
    }

    if (!pslIsNumChannelsValid(module)) {
        status = XIA_INVALID_NUMCHANS;
        sprintf(info_string,
                "Wrong number of channels for module %s",
                module->alias);
        pslLogError("pslValidateModule", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * This routine verifies that the interface information
 * for this module is consistent with the specified
 * board type.
 */
PSL_STATIC boolean_t pslIsInterfaceValid(Module *module)
{
    if (module->interface_info->type == XIA_SERIAL) {
        IS_USB = FALSE_;
        return TRUE_;
    }

    if (module->interface_info->type == XIA_USB2) {
        IS_USB = TRUE_; /* HACK for RS232 Alpha support */
        return TRUE_;
    }

    return FALSE_;
}


/*
 * This routine verifies that there is only one
 * channel defined for this board.
 */
PSL_STATIC boolean_t pslIsNumChannelsValid(Module *module)
{
#ifdef XIA_ALPHA
    return module->number_of_channels == 2;
#else
    if (module->number_of_channels != 1) {
        return FALSE_;
    }

    return TRUE_;
#endif
}


/*
 * Required by Handel, but unimplemented for this product
 */
PSL_STATIC int pslValidateDefaults(XiaDefaults *defs)
{
    UNUSED(defs);

    return XIA_SUCCESS;
}


/*
 * This routine handles downloading the requested kind of firmware through
 * XerXes.
 *
 * NOTE: For the microDXP, this routine is actually defunct, as the FiPPI
 * info will be supported in the acquisition values.
 */
PSL_STATIC int pslDownloadFirmware(int detChan, char *type, char *file,
                                   Module *m, char *rawFilename,
                                   XiaDefaults *defs)
{
    int status;
    int statusX;

    unsigned int fippiNum = 0;

    byte_t send[2];
    byte_t receive[3 + RECV_BASE];

    byte_t cmd       = CMD_SET_FIPPI_CONFIG;

    unsigned int lenS      = (unsigned int)(sizeof(send) / sizeof(send[0]));
    unsigned int lenR      = (unsigned int)(sizeof(receive) / sizeof(receive[0]));

    UNUSED(file);
    UNUSED(m);
    UNUSED(rawFilename);
    UNUSED(defs);


    /* The only acceptable type of firmware to download
     * is FiPPI0, FiPPI1 or FiPPI2.
     */
    if (!(STREQ(type, "fippi0") ||
            STREQ(type, "fippi1") ||
            STREQ(type, "fippi2"))) {
        status = XIA_NOSUPPORT_FIRM;
        sprintf(info_string, "%s is not a supported firmware type", type);
        pslLogError("pslDownloadFirmware", info_string, status);
        return status;
    }

    sscanf(type, "fippi%u", &fippiNum);

    sprintf(info_string, "User requested fippi = %u", fippiNum);
    pslLogDebug("pslDownloadFirmware", info_string);

    /* Not sure how to use the CurrentFirmware info. in
     * this context. For now, we can just spend the
     * extra bit of time switching firmware when
     * requested.
     */
    send[0] = (byte_t)0x00;
    send[1] = (byte_t)(fippiNum & 0xFF);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        pslLogError("pslDownloadFirmware", "Error executing command", status);
        return status;
    }

    sprintf(info_string, "Current FiPPI = %u",
            (unsigned int)receive[5]);
    pslLogDebug("pslDownloadFirmware", info_string);

    return XIA_SUCCESS;
}


/*
 * This routine calculates the appropriate DSP parameter(s) from the name and
 * then downloads it/them to the board.
 */
PSL_STATIC int pslSetAcquisitionValues(int detChan, char *name,
                                       void *value,
                                       XiaDefaults *defs,
                                       FirmwareSet *firmwareSet,
                                       CurrentFirmware *currentFirmware,
                                       char *detectorType, Detector *detector,
                                       int detector_chan, Module *m,
                                       int modChan)
{
    int status;

    XiaDaqEntry *e = NULL;
    Udxp_AcquisitionValue *av = NULL;

    ASSERT(name  != NULL);
    ASSERT(value != NULL);
    ASSERT(defs  != NULL);

    UNUSED(firmwareSet);
    UNUSED(currentFirmware);
    UNUSED(detectorType);
    UNUSED(detector);
    UNUSED(detector_chan);
    UNUSED(m);
    UNUSED(modChan);

    av = pslFindAV(name);

    if (av == NULL) {
        sprintf(info_string, "Unknown acquisition value '%s'", name);
        pslLogError("pslSetAcquisitionValues", info_string, XIA_NOT_FOUND);
        return XIA_NOT_FOUND;
    }

    sprintf(info_string, "setting acquisition value '%s' = %0.3f", name, *((double*)value));
    pslLogDebug("pslSetAcquisitionValues", info_string);

    status = av->setFN(detChan, name, defs, value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting '%s' to detChan %d", name, detChan);
        pslLogError("pslSetAcquisitionValues", info_string, status);
        return status;
    }

    e = pslFindEntry(name, defs);

    if (e != NULL) {
        e->data    = *((double *)value);
        e->pending = 0.0;
        e->state   = AV_STATE_SYNCD;
    }

    return XIA_SUCCESS;
}


/*
 * Retrieves the specified acquisition value from either the cache
 *  or the hardware depending on the state of the value
 */
PSL_STATIC int pslGetAcquisitionValues(int detChan, char *name,
                                       void *value,
                                       XiaDefaults *defs)
{
    int status;

    XiaDaqEntry *e = NULL;

    Udxp_AcquisitionValue *av = NULL;


    ASSERT(name  != NULL);
    ASSERT(value != NULL);
    ASSERT(defs  != NULL);

    e = pslFindEntry(name, defs);

    /* If acquisition name can be found in the list of defaults
     * use synced value to avoid re-reading from device
     */
    if (e != NULL && (e->state & AV_STATE_SYNCD)) {
        *((double *)value) = e->data;
        return XIA_SUCCESS;
    }

    av = pslFindAV(name);

    if (av == NULL) {
        sprintf(info_string, "Unknown acquisition value '%s'", name);
        pslLogError("pslGetAcquisitionValues", info_string, XIA_NOT_FOUND);
        return XIA_NOT_FOUND;
    }

    status = av->getFN(detChan, name, defs, value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting '%s' for detChan %d", name, detChan);
        pslLogError("pslGetAcquisitionValues", info_string, status);
        return status;
    }

    sprintf(info_string, "chan %d acquisition value '%s' = %0.3f", detChan, name, *((double*)value));
    pslLogDebug("pslGetAcquisitionValues", info_string);

    /* If the state is MODIFIED then we need to keep that bit and just
     * clear the UNKNOWN bit.
     */
    if (e != NULL) {
        e->state = (flag_t)((e->state | AV_STATE_SYNCD) & ~AV_STATE_UNKNOWN);
        e->data  = *((double *)value);
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GainTrimCalibrate(int detChan, Detector *det,
                                      int modChan, Module *m, XiaDefaults *defs,
                                      void *value)
{
    int status;
    double gain = 0.0;
    double deltaGain  = *((double *)value);

    XiaDaqEntry *e = NULL;

    UNUSED(det);
    UNUSED(modChan);
    UNUSED(m);

    ASSERT(defs != NULL);

    if (deltaGain <= 0) {
        sprintf(info_string, "Invalid gain scale factor %0.3f for "
                "detChan %d", deltaGain, detChan);
        pslLogError("psl__GainTrimCalibrate", info_string, XIA_GAIN_SCALE);
        return XIA_GAIN_SCALE;
    }

    /* This acquisition value must exist */
    status = pslGetAcquisitionValues(detChan, "gain_trim", (void *)&gain, defs);
    ASSERT(status == XIA_SUCCESS);
    gain *= deltaGain;

    /* This is the same routine that pslSetAcquisitionValues() uses to set
    * the acquisition value.
    */
    status = pslSetGainTrim(detChan, NULL, defs, (void *)&gain);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the gain trim to %0.3f for "
                "detChan %d", gain, detChan);
        pslLogError("psl__GainTrimCalibrate", info_string, status);
        return status;
    }

    e = pslFindEntry("gain_trim", defs);
    ASSERT(e != NULL);

    e->data    = gain;
    e->state   = AV_STATE_SYNCD;

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
 * This routine adjusts the gain via. the preamp gain. As the name suggests,
 * this is mostly for situations where you are trying to calibrate the gain
 * with a fixed eV/ADC, which should cover 99% of the situations, if not the
 * full 100%.
 */
PSL_STATIC int pslGainCalibrate(int detChan, Detector *detector,
                                int modChan, Module *m, XiaDefaults *defaults,
                                double deltaGain)
{
    int status;
    double gain = 0.0;

    XiaDaqEntry *e = NULL;

    UNUSED(detector);
    UNUSED(modChan);
    UNUSED(m);

    ASSERT(defaults != NULL);

    if (deltaGain <= 0) {
        sprintf(info_string, "Invalid gain scale factor %0.3f for "
                "detChan %d", deltaGain, detChan);
        pslLogError("pslGainCalibrate", info_string, XIA_GAIN_SCALE);
        return XIA_GAIN_SCALE;
    }

    /* This acquisition value must exist */
    status = pslGetAcquisitionValues(detChan, "gain", (void *)&gain, defaults);
    ASSERT(status == XIA_SUCCESS);
    gain *= deltaGain;

    /* This is the same routine that pslSetAcquisitionValues() uses to set
    * the acquisition value.
    */
    status = pslSetGainbase(detChan, NULL, defaults, (void *)&gain);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting the gain to %0.3f for "
                "detChan %d", gain, detChan);
        pslLogError("pslGainCalibrate", info_string, status);
        return status;
    }

    /* Tag the acquisition value as set */
    e = pslFindEntry("gain", defaults);
    ASSERT(e != NULL);

    e->data    = gain;
    e->state   = AV_STATE_SYNCD;

    return XIA_SUCCESS;
}


/*
 * This routine is responsible for calling the XerXes version of start run.
 * There may be a problem here involving multiple calls to start a run on a
 * module since this routine is iteratively called for each channel. May need
 * a way to start runs that takes the "state" of the module into account.
 */
PSL_STATIC int pslStartRun(int detChan, unsigned short resume, XiaDefaults *defs,
                           Module *m)
{
    int statusX;
    unsigned int modChan;

    DEFINE_CMD(CMD_START_RUN, 1, 3);

    UNUSED(defs);


#ifdef XIA_ALPHA
    memset(ALPHA_NEXT_N_EVENTS, 0, sizeof(unsigned short) * 2);
    memset(ALPHA_EVENT_COUNT, 0, sizeof(unsigned long) * 2);
#endif /* XIA_ALPHA */


    /* Handel and the uDXP have different
     * ideas of what resume = 1 means.
     */
    send[0] = (byte_t)(resume ^ 0x01);

    for (modChan = 0; modChan < m->number_of_channels; modChan++) {
        int c = m->channels[modChan]; /* The detChan of the looped channel. */

        ASSERT(!m->state || m->state->runActive[modChan] == FALSE_);

        statusX = dxp_cmd(&c, &cmd, &lenS, send, &lenR, receive);

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error starting run on detChan %d", c);
            pslLogError("pslStartRun", info_string, XIA_XERXES);
            pslStopRun(detChan, m);
            return XIA_XERXES;
        }

        sprintf(info_string, "Started a run w/ id = %u [%d]",
                (unsigned int)(receive[5] + (receive[6] << 8)), c);
        pslLogInfo("pslStartRun", info_string);
    }

    return XIA_SUCCESS;
}


/*
 * This routine is responsible for calling the XerXes version of stop run.
 * With some hardware, all channels on a given module may be "stopped"
 * together. Not sure if calling stop multiple times do to the detChan
 * iteration procedure will cause problems or not.
 */
PSL_STATIC int pslStopRun(int detChan, Module *m)
{
    int status = XIA_SUCCESS;
    int statusX = XIA_SUCCESS;
    unsigned int modChan;

    unsigned int lenS = 0;
    unsigned int lenR = RECV_BASE + 1;
    /*unsigned int len0 = 0;*/

    byte_t cmd       = CMD_STOP_RUN;

    byte_t receive[RECV_BASE + 1];

    UNUSED(detChan);

    for (modChan = 0; modChan < m->number_of_channels; modChan++) {
        int c = m->channels[modChan]; /* The detChan of the looped channel. */

        statusX = dxp_cmd(&c, &cmd, &lenS, NULL, &lenR, receive);

        if (statusX != DXP_SUCCESS) {
            /* Latch the first error and continue to stop all channels. */
            if (status == XIA_SUCCESS)
                status = statusX;
            sprintf(info_string, "Error stopping run on detChan %d", c);
            pslLogError("pslStopRun", info_string, statusX);
        }
    }

    return status;
}


/*
 * This routine retrieves the specified data from the board. In the case of
 * the uDXP a run does not need to be stopped in order to retrieve the
 * specified information.
 */
PSL_STATIC int pslGetRunData(int detChan, char *name, void *value,
                             XiaDefaults *defs, Module *m)
{
    int status;

    unsigned int i;
    unsigned int numRunData = (unsigned int)(sizeof(runData) /
                                             sizeof(runData[0]));

    UNUSED(m);


    for (i = 0; i < numRunData; i++) {
        if (STREQ(runData[i].name, name)) {
            status = runData[i].fn(detChan, value, defs);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error getting run data %s for detChan %d",
                        name, detChan);
                pslLogError("pslGetRunData", info_string, status);
            }

            return status;
        }
    }

    sprintf(info_string, "Unknown run data type: %s for detChan %d",
            name, detChan);
    pslLogError("pslGetRunData", info_string, XIA_BAD_NAME);
    return XIA_BAD_NAME;
}


/*
 * This routine dispatches calls to the requested special run routine, when
 * that special run is supported by the udxp.
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

    ASSERT(name != NULL);

    /* Check for match in trace type */
    for (i = 0; i < N_ELEMS(traceTypes); i++) {
        if (STREQ(traceTypes[i].name, name)) {

            specialRunType = (short) i;
            status = pslDoTrace(detChan, specialRunType, (double *)info, defaults);

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

            status = specialRun[i].fn(detChan, info, defaults);

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
 * Do a special run to adjust ADC offsets
 */
PSL_STATIC int psl__AdjustOffsets(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    unsigned int SETOFFADC = 0;
    double offset  = *((double *)value);

    DEFINE_CMD(CMD_SET_OFFADC, 3, 4);

    UNUSED(defs);

    ASSERT(value != NULL);

    if (offset > 16383 || offset < 0) {
        sprintf(info_string, "ADC offset %0f is out-of-range (%d, %d)",
                offset, 0, 16383);
        pslLogError("psl__AdjustOffsets", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    SETOFFADC = (unsigned int)offset;

    send[0] = 0x00;
    send[1] = (byte_t)LO_BYTE(SETOFFADC);
    send[2] = (byte_t)HI_BYTE(SETOFFADC);

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting adc offset to %du "
                "for detChan %d", SETOFFADC, detChan);
        pslLogError("psl__AdjustOffsets", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double *)value) = (double)BYTE_TO_WORD(receive[5], receive[6]);

    return XIA_SUCCESS;
}

/*
 * snapshot special run
 * value (double)
 : 0: no action
 * 1: clear spectrum and statistics after taking snapshot.
 */
PSL_STATIC int psl__Snapshot(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    unsigned long features;
    double clearSpectrum  = *((double *)value);

    DEFINE_CMD(CMD_SNAPSHOT, 1, 1);

    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SNAPSHOT)) {
        pslLogError("psl__Snapshot", "Connected device does not support "
                    "'snapshot' special run", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    send[0] = (byte_t)clearSpectrum;

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error taking snapshot for detChan %d", detChan);
        pslLogError("psl__Snapshot", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Check if given tracewait is in range, reset if out of range and log debug
 * message
 */
PSL_STATIC int pslCheckTraceWaitRange(int detChan, double *tracewait, XiaDefaults *defs)
{
    int status;
    double spd = 0;

    unsigned int tracetick;

    ASSERT(defs != NULL);
    ASSERT(tracewait != NULL);

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&spd, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslCheckTraceWaitRange", info_string, status);
        return status;
    }

    if (*tracewait > MAX_TRACEWAIT_US) {
        sprintf(info_string, "Tracewait %0.3fus for detChan = %d is out of range, "
                "reset to %0.3fus", *tracewait, detChan, MAX_TRACEWAIT_US);
        pslLogDebug("pslCheckTraceWaitRange", info_string);
        *tracewait = MAX_TRACEWAIT_US;
    }

    if (*tracewait < pslMinTraceWait(spd)) {
        sprintf(info_string, "Tracewait %0.3fus for detChan = %d is out of range, "
                "reset to %0.3fus", *tracewait,  detChan, pslMinTraceWait(spd));
        pslLogDebug("pslCheckTraceWaitRange", info_string);
        *tracewait = pslMinTraceWait(spd);
    }

    tracetick = (unsigned int)ROUND(*tracewait * spd) - 1;

    /* set actual trace wait to the rounded value */
    *tracewait = ((double)tracetick + 1.0) / spd;

    sprintf(info_string, "tracewait = %.3f, tracetick = %u", *tracewait, tracetick);
    pslLogDebug("pslCheckTraceWaitRange", info_string);

    return XIA_SUCCESS;
}

/*
 * Process trace parameters in preparation for collecting the trace. The actual trace
 * is performed in one of two ways, depending on hardware setup and firmware:
 *   1. Traditional RS-232 command trace trigger and readout in one blocking operation. In this
 *      case we set up the parameters here and quit, waiting for the subsequent call to
 *      xiaGetSpecialRunData(detChan, "adc_trace", data) to perform the trace and return
 *      the data.
 *   2. RS-232 command trace trigger with direct (fast) USB readout as a separate step. This
 *      requires a USB interface connection and suitable PIC firmware. In this case we set up
 *      parameters and trigger the trace here. The trace readout happens in the subsequent call.
 */
PSL_STATIC int pslDoTrace(int detChan, short type, double *info, XiaDefaults *defs)
{
    int status;

    /* The sampling interval comes in nanoseconds but tracewait is stored in us */
    double tracewait_us = info[1] / 1000.0;

    double spd = 0;

    parameter_t TRACEWAIT = 0;
    parameter_t TRACETYPE = (parameter_t) type;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    XiaDaqEntry *e = NULL;

    ASSERT(info != NULL);

    status = pslCheckTraceWaitRange(detChan, &tracewait_us, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking tracewait range for detChan %d", detChan);
        pslLogError("pslDoTrace", info_string, status);
        return status;
    }

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&spd, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslDoTrace", info_string, status);
        return status;
    }

    TRACEWAIT = (parameter_t)ROUND(tracewait_us * spd) - 1;
    status = pslSetParameter(detChan, "TRACEWAIT", TRACEWAIT);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting TRACEWAIT for detChan = %d", detChan);
        pslLogError("pslDoTrace", info_string, status);
        return status;
    }

    /* Sync adc_trace_wait with actual values
     * only needed for backward compatibility in microManager
     */
    e = pslFindEntry("adc_trace_wait", defs);
    ASSERT(e != NULL);

    e->data    = tracewait_us;
    e->state   = AV_STATE_SYNCD;

    /* Pass back the actual value in nanoseconds */
    info[1] = tracewait_us * 1000.0;

    sprintf(info_string, "Set TRACEWAIT = %hu, TRACETYPE = %hu", TRACEWAIT,
            TRACETYPE);
    pslLogInfo("pslDoTrace", info_string);

    if (!isSuper) return XIA_SUCCESS;

    /* The last element of traceTypes is 'debug', which was put in place so that Jack
    * can run traces without changing the current value of the DSP parameter.
    */
    if (TRACETYPE != DEBUG_TRACE_TYPE) {
        status = pslSetParameter(detChan, "TRACETYPE", TRACETYPE);

        if (status != DXP_SUCCESS) {
            sprintf(info_string, "Error setting TRACETYPE for detChan = %d", detChan);
            pslLogError("pslDoTrace", info_string, status);
            return status;
        }
    }

    /* Proceed to trigger the trace collection for direct USB readout, if applicable. This
     * will block until the buffer is filled.
     */
    if (IS_USB && dxp_has_direct_trace_readout(detChan)) {
        DEFINE_CMD(CMD_READ_ADC_TRACE, 3, 1);

        send[0] = (byte_t)LO_BYTE((unsigned int)TRACEWAIT);
        send[1] = (byte_t)HI_BYTE((unsigned int)TRACEWAIT);
        send[2] = 1; /* request direct USB readout */

        status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

        if (status != DXP_SUCCESS) {
            status = XIA_XERXES;
            sprintf(info_string, "Error triggering ADC trace for detChan %d", detChan);
            pslLogError("pslDoTrace", info_string, status);
            return status;
        }
    }

    return XIA_SUCCESS;
}


/*
 * This routine parses out the actual data gathering to other routines.
 */
PSL_STATIC int pslGetSpecialRunData(int detChan, char *name, void *value,
                                    XiaDefaults *defs)
{
    int status;

    unsigned int i;
    unsigned int numSpecRunData = (unsigned int)(sizeof(specialRunData) /
                                                 sizeof(specialRunData[0]));


    for (i = 0; i < numSpecRunData; i++) {
        if (STREQ(specialRunData[i].name, name)) {
            status = specialRunData[i].fn(detChan, value, defs);

            if (status != XIA_SUCCESS) {
                sprintf(info_string,
                        "Error getting special run data %s for detChan %d",
                        name, detChan);
                pslLogError("pslGetSpecialRunData", info_string, status);
            }

            return status;
        }
    }

    status = XIA_BAD_NAME;
    sprintf(info_string,
            "Unknown special run data type: %s for detChan %d",
            name, detChan);
    pslLogError("pslGetSpecialRunData", info_string, status);
    return status;
}


/*
 * A much abused routine that returns both the default alias and
 * the default values
 */
PSL_STATIC int pslGetDefaultAlias(char *alias, char **names,
                                  double *values)
{
    int i;
    /* We need a separate index to use for the names array since
     * there may be acquisition values that aren't required and names
     * only has enough memory for the required values.
     */
    int reqIdx;

    char *defaultsName = "defaults_udxp";


    for (i = 0, reqIdx = 0; i < NUM_ACQ_VALS; i++) {

        if (acqVals[i].member & AV_MEM_REQ) {
            strncpy(names[reqIdx], acqVals[i].name, strlen(acqVals[i].name) + 1);
            values[reqIdx] = acqVals[i].def;
            reqIdx++;
        }
    }

    strcpy(alias, defaultsName);

    return XIA_SUCCESS;
}


/*
 * This routine retrieves the value of the DSP parameter name from detChan.
 */
PSL_STATIC int pslGetParameter(int detChan, const char *name, unsigned short *value)
{
    int statusX;

    ASSERT(name != NULL);
    ASSERT(value != NULL);

    statusX = dxp_get_one_dspsymbol(&detChan, (char *)name, value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading '%s' for detChan %d", name, detChan);
        pslLogError("pslGetParameter", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * This routine sets the value of the DSP parameter name for detChan.
 */
PSL_STATIC int pslSetParameter(int detChan, const char *name, unsigned short value)
{
    int statusX;

    ASSERT(name != NULL);

    statusX = dxp_set_one_dspsymbol(&detChan, (char *)name, &value);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting '%s' to %#hx for detChan %d",
                name, value, detChan);
        pslLogError("pslSetParameter", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Setup per-module settings, this is done after all the
 * acquisition values are set up.
 */
PSL_STATIC int pslModuleSetup(int detChan, XiaDefaults *defaults,
                              Module *m)
{
    UNUSED(m);
    UNUSED(defaults);
    UNUSED(detChan);

    return XIA_SUCCESS;
}


/*
 * The whole point of this routine is to make the PSL layer start things
 * off by calling pslSetAcquistionValues() for the acquisition values it
 * thinks are appropriate for the X10P.
 */
PSL_STATIC int pslUserSetup(int detChan, XiaDefaults *defaults,
                            FirmwareSet *firmwareSet,
                            CurrentFirmware *currentFirmware,
                            char *detectorType, Detector *detector,
                            int detector_chan, Module *m, int modChan)
{
    int status = XIA_SUCCESS;

    UNUSED(defaults);
    UNUSED(firmwareSet);
    UNUSED(currentFirmware);
    UNUSED(detectorType);
    UNUSED(detector);
    UNUSED(detector_chan);
    UNUSED(m);
    UNUSED(modChan);

#ifdef XIA_ALPHA

    /* This block will be called once for each microDXP on the Alpha
     * motherboard, which is a little inefficient, but not a big deal.
     */
    status = pslGetParameter(detChan, "OUTBUFSTART", &OUTBUFSTART);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading start address for the output "
                "buffer for detChan %d", detChan);
        pslLogError("pslUserSetup", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "OUTBUFLEN", &OUTBUFLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading output buffer length for "
                "detChan %d", detChan);
        pslLogError("pslUserSetup", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "EVENTLEN", &EVENTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading event length for detChan %d",
                detChan);
        pslLogError("pslUserSetup", info_string, status);
        return status;
    }

    ALPHA_MAX_EVENTS_IN_BUFFER = (unsigned short)floor((double)OUTBUFLEN /
                                                       (double)EVENTLEN);
#else
    UNUSED(detChan);
#endif /* XIA_ALPHA */

    return status;
}


/*
 * Checks to see if the specified name is on the list of
 * required acquisition values for the Saturn.
 */
PSL_STATIC boolean_t pslCanRemoveName(char *name)
{
    UNUSED(name);

    return FALSE_;
}


/*
 * Return the number of required defaults in the acquisition values
 *  list so that Handel can allocate the proper amount of memory
 */
PSL_STATIC unsigned int pslGetNumDefaults(void)
{
    int i;

    unsigned int nDefs = 0;


    for (i = 0; i < NUM_ACQ_VALS; i++) {
        if (acqVals[i].member & AV_MEM_REQ) {
            nDefs++;
        }
    }

    return nDefs;
}

/*
 * This routine gets the number of DSP parameters
 * for the specified detChan.
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
PSL_STATIC int pslGetParamData(int detChan, char *name, void *value)
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
 * Gets all of the DSP parameter values for the specified channel.
 */
PSL_STATIC int pslGetParamValues(int detChan, void *value)
{
    int statusX;

    ASSERT(value != NULL);

    statusX = dxp_readout_detector_run(&detChan, (unsigned short *)value, NULL,
                                       NULL);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting DSP parameter values for detChan %d",
                detChan);
        pslLogError("pslGetParamValues", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}

/*
 * This routine is mainly a wrapper around dxp_symbolname_by_index()
 * since VB can't pass a string array into a DLL and, therefore,
 * is unable to use pslGetParams() to retrieve the parameters list.
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
 * This routine returns the anticipated length
 * of the MCA spectrum based on the existing
 * acquisition values.
 */
PSL_STATIC int pslGetMCALength(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    double nMCA = 0.0;


    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    status = pslGetAcquisitionValues(detChan, "number_mca_channels", (void *)&nMCA,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting number of MCA channels for detChan %d",
                detChan);
        pslLogError("pslGetMCALength", info_string, status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)nMCA;

    return XIA_SUCCESS;
}


/*
 * This routine returns the MCA spectrum read
 * from the board.
 */
PSL_STATIC int pslGetMCAData(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int dataLen;
    unsigned int lenS = 5;
    unsigned int lenR = 0;

    unsigned long *data = (unsigned long *)value;

    double bytesPerBin = 0.0;
    double numMCAChans = 0.0;
    double mcaLowLim = 0.0;

    byte_t cmd         = CMD_READ_MCA;

    byte_t send[5];

    byte_t *receive = NULL;


    status = pslGetAcquisitionValues(detChan, "bytes_per_bin", (void *)&bytesPerBin, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting bytes per bin for detChan %d", detChan);
        pslLogError("pslGetMCAData", info_string, status);
        return status;
    }

    status = pslGetAcquisitionValues(detChan, "number_mca_channels", (void *)&numMCAChans,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting number of MCA channels for detChan %d", detChan);
        pslLogError("pslGetMCAData", info_string, status);
        return status;
    }

    mcaLowLim = 0.0;

    sprintf(info_string, "bytesPerBin = %.3f, numMCAChans = %.3f, mcaLowLim = %.3f",
            bytesPerBin, numMCAChans, mcaLowLim);
    pslLogDebug("pslGetMCAData", info_string);

    if (IS_USB && dxp_has_direct_mca_readout(detChan)) {
        status = pslGetMcaDirect(detChan, (int)bytesPerBin,
                                (int)numMCAChans, 0x2000, data);
        return status;
    }

    send[0] = LO_BYTE((unsigned short)mcaLowLim);
    send[1] = HI_BYTE((unsigned short)mcaLowLim);
    send[2] = LO_BYTE((unsigned short)numMCAChans);
    send[3] = HI_BYTE((unsigned short)numMCAChans);
    send[4] = (byte_t)bytesPerBin;

    dataLen = (unsigned int)(bytesPerBin * numMCAChans);
    lenR = (unsigned int)(dataLen + 1 + RECV_BASE);

    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (receive == NULL) {
        pslLogError("pslGetMCAData", "Out-of-memory trying to create receive array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);

        sprintf(info_string, "Error getting MCA data from detChan %d", detChan);
        pslLogError("pslGetMCAData", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Transfer the spectra to the user's array. */
    for (i = 0; i < numMCAChans; i++) {
        data[i] = pslUlFromBytesOffset(receive, (int)bytesPerBin,
                            RECV_BASE + i * (int)bytesPerBin);
    }

    utils->funcs->dxp_md_free((void *)receive);

    return XIA_SUCCESS;
}


/*
 * This routine calculates and returns
 * the current livetime value.
 */
PSL_STATIC int pslGetLivetime(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetLivetime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double *)value) = stats[1];

    return XIA_SUCCESS;
}


/*
 * This routine calculates and returns the
 * current runtime value.
 */
PSL_STATIC int pslGetRuntime(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetRuntime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double *)value) = stats[0];

    return XIA_SUCCESS;
}


/*
 * This routine retrieves the Input Count Rate,
 * which can also be defined as the number of
 * fast peak events divided by the livetime.
 */
PSL_STATIC int pslGetICR(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetICR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double *)value) = stats[5];

    return XIA_SUCCESS;
}


/*
 * This routine retrieves the Output Count Rate,
 * which can also be defined as the number of
 * events divided by the realtime.
 */
PSL_STATIC int pslGetOCR(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetOCR", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double *)value) = stats[6];

    return XIA_SUCCESS;
}


/*
 * This routine retrieves the number
 * of events that were binned in the MCA
 * spectrum.
 */
PSL_STATIC int pslGetEvents(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetEvents", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((unsigned long *)value) = (unsigned long)stats[4];

    return XIA_SUCCESS;
}


/*
 * This routine retrieves the number of
 * triggers that occurred in the run.
 */
PSL_STATIC int pslGetTriggers(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double stats[9];

    ASSERT(value != NULL);

    statusX = pslGetModuleStatistics(detChan, (void *)stats, defs);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetTriggers", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((unsigned long *)value) = (unsigned long)stats[3];

    return XIA_SUCCESS;
}


/*
 * This routine returns the length of
 * the baseline histogram.
 */
PSL_STATIC int pslGetBaseHistogramLen(int detChan, void *value,
                                      XiaDefaults *defs)
{
    UNUSED(detChan);
    UNUSED(defs);

    *((unsigned long *)value) = BASELINE_LEN;

    return XIA_SUCCESS;
}


/*
 * This routine returns the baseline histogram.
 * The calling routine is responsible for allocating
 * the proper amount of memory.
 */
PSL_STATIC int pslGetBaseline(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;
    int status;

    unsigned int i;
    unsigned int lenS = 0;
    unsigned int lenR = 2049 + RECV_BASE;

    unsigned long *data = (unsigned long *)value;

    byte_t cmd       = CMD_READ_BASELINE;

    byte_t receive[2049 + RECV_BASE];

    UNUSED(defs);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string,
                "Error reading out baseline for detChan %d",
                detChan);
        pslLogError("pslGetBaseline", info_string, status);
        return status;
    }

    for (i = 0; i < BASELINE_LEN; i++) {
        data[i] = pslUlFromBytesOffset(receive, 2, RECV_BASE + i * 2);
    }

    return XIA_SUCCESS;
}



/*
 * Sets the gainbase linear in linear
 *
 * This routine actually converts the linear gain into the correct
 * GAINBASE value. An extra check is provided in this routine to
 * return a special error value when the hardware detects that the VGA
 * option is not installed.
 */
PSL_STATIC int pslSetGainbase(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double g        = *((double *)value);
    double gDB      = 0.0;

    double digitalGain;
    double gainSwitch;

    unsigned short gainMode  = 0;

    parameter_t GAINBASE = 0;
    parameter_t SWGAIN = 0;
    parameter_t DGAINBASE = 0;
    signed short DGAINBASEEXP = 0;

    unsigned int lenS;
    unsigned int lenR;

    byte_t cmd;

    /* Allocate the maximum size for the three commands */
    byte_t send[4];
    byte_t receive[4 + RECV_BASE];

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    gDB = 20.0 * log10(g);

    status = pslGetGainMode(detChan, NULL, defs, (void *)&gainMode);

    if (status != XIA_SUCCESS) {
        pslLogError("pslSetGainbase", "Error getting gain mode.", status);
        return status;
    }

    switch (gainMode)
    {
    case GAIN_MODE_FIXED:
    case GAIN_MODE_VGA:
    case GAIN_MODE_DIGITAL:

        GAINBASE = (parameter_t)ROUND(gDB / DB_PER_LSB);

        if (GAINBASE > MAX_GAINBASE || GAINBASE < MIN_GAINBASE) {
            sprintf(info_string, "Gain (%.3f) setting out of range (%d, %d) for detChan %d",
                    g, MIN_GAINBASE, MAX_GAINBASE, detChan);
            pslLogError("pslSetGainbase", info_string, XIA_GAIN_OOR);
            return XIA_GAIN_OOR;
        }

        cmd = CMD_SET_GAINBASE;
        lenS = 3;
        lenR = 3 + RECV_BASE;

        send[0] = (byte_t)0x00;
        send[1] = (byte_t)LO_BYTE(GAINBASE);
        send[2] = (byte_t)HI_BYTE(GAINBASE);

        statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

        /* If the VGA isn't installed then we want
         * to provide a slightly different error
         * message.
         */
        if (receive[4] == 1) {
            pslLogError("pslSetGainbase", "No VGA installed on specified board",
                        XIA_NO_VGA);
            return XIA_NO_VGA;
        }

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error setting base gain DAC for detChan %d", detChan);
            pslLogError("pslSetGainbase", info_string, XIA_XERXES);
            return XIA_XERXES;
        }

        gDB = (double)GAINBASE / DB_PER_LSB;

        /* Due to rounding, the calculated value can be different than the specified
         * value. Keep the rounded value for caching.
         */
        g = (double)pow(10.0, (gDB / 20.0));

        break;

    case GAIN_MODE_SWITCHED:
    case GAIN_MODE_HIGHLOW:

        /* Use the linear Variable Gain gDB to select SWGAIN */
        if (gainMode == GAIN_MODE_SWITCHED) {
            gDB -= 20.0 * log10(VARIABLE_LOWEST_BASEGAIN);
            gainSwitch = ROUND(gDB / VARIABLE_GAIN_SPACING);
            SWGAIN = (parameter_t)MIN(MAX(gainSwitch, 0.0), 15.0);
            digitalGain = g * GAIN_SCALE_FACTOR / VARIABLE_GAIN_LUT[SWGAIN];
        } else {
            gDB -= 20.0 * log10(HIGHLOW_LOWEST_BASEGAIN);
            gainSwitch = ROUND(gDB / HIGH_LOW_GAIN_SPACING);
            SWGAIN = (parameter_t)MIN(MAX(gainSwitch, 0.0), 1.0);
            digitalGain = g * GAIN_SCALE_FACTOR * GAIN_HIGHLOW_FACTOR / HIGHLOW_GAIN_LUT[SWGAIN];
        }

        DGAINBASEEXP = (signed short)floor(log(digitalGain) / log(2.0));
        DGAINBASE = (parameter_t)(32768.0 * digitalGain / pow(2.0, (double)DGAINBASEEXP));

        /* Recalculate the gain to get rounded off value */
        g = pslCalculateBaseGain(gainMode, SWGAIN, DGAINBASE, DGAINBASEEXP);

        cmd = CMD_SET_SWGAIN;
        lenS = 2;
        lenR = 2 + RECV_BASE;

        send[0] = (byte_t)0x00;
        send[1] = (byte_t)SWGAIN;

        statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error setting switched gain for detChan %d", detChan);
            pslLogError("pslSetGainbase", info_string, XIA_XERXES);
            return XIA_XERXES;
        }

        cmd = CMD_SET_DIGITALGAIN;
        lenS = 4;
        lenR = 4 + RECV_BASE;

        send[0] = (byte_t)0x00;
        send[1] = (byte_t)LO_BYTE(DGAINBASE);
        send[2] = (byte_t)HI_BYTE(DGAINBASE);
        send[3] = (byte_t)DGAINBASEEXP;

        statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error setting digital gain for detChan %d", detChan);
            pslLogError("pslSetGainbase", info_string, XIA_XERXES);
            return XIA_XERXES;
        }

        break;

    default:
        FAIL();
        break;
    }

    return XIA_SUCCESS;
}

/*
 * Internal function for calculating the base gain value
 * in GAIN_MODE_SWITCHED or GAIN_MODE_HIGHLOW only
 */
PSL_STATIC double pslCalculateBaseGain(unsigned int gainMode,
                                       parameter_t SWGAIN, parameter_t DGAINBASE, signed short DGAINBASEEXP)
{
    double baseGain = 0.0;
    double digitalGain = 0.0;
    double hybridGain = 0.0;

    digitalGain = pow(2.0, (double)DGAINBASEEXP) * ((double)DGAINBASE / 32768.0);

    if (gainMode == GAIN_MODE_SWITCHED) {
        hybridGain = VARIABLE_GAIN_LUT[SWGAIN] * digitalGain;
        baseGain = hybridGain / GAIN_SCALE_FACTOR;
    } else if (gainMode == GAIN_MODE_HIGHLOW) {
        hybridGain = HIGHLOW_GAIN_LUT[SWGAIN] * digitalGain;
        baseGain = hybridGain / (GAIN_SCALE_FACTOR * GAIN_HIGHLOW_FACTOR);
    } else {
        FAIL();
    }

    sprintf(info_string, "GAINMODE %hu, SWGAIN = %hu, DGAINBASEEXP = %hd, "
            "DGAINBASE = %hu, digitalGain = %0.3f, hybridGain = %0.3f, base gain = %0.3f",
            gainMode, SWGAIN, DGAINBASEEXP, DGAINBASE, digitalGain, hybridGain,
            baseGain);
    pslLogDebug("pslCalculateBaseGain", info_string);

    return baseGain;
}


/*
 * Returns the linear GAINBASE value
 */
PSL_STATIC int pslGetGainbase(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double *g  = (double *)value;
    double gDB = 0.0;

    unsigned short gainMode;

    parameter_t GAINBASE = 0;
    parameter_t SWGAIN = 0;
    parameter_t DGAINBASE = 0;
    signed short DGAINBASEEXP = 0;

    int maxSwgain = 0;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetGainMode(detChan, NULL, defs, (void *)&gainMode);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetGainbase", "Error getting gain mode.", status);
        return status;
    }

    switch (gainMode)
    {
    case GAIN_MODE_FIXED:
    case GAIN_MODE_VGA:
    case GAIN_MODE_DIGITAL:

        status = pslGetParameter(detChan, "GAINBASE", &GAINBASE);

        if (status != XIA_SUCCESS) {
            pslLogError("pslGetGainbase", "Error getting gain base for detChan %d", detChan);
            return status;
        }

        gDB = (double)(GAINBASE * DB_PER_LSB);
        *g = pow(10.0, (gDB / 20.0));

        break;

    case GAIN_MODE_SWITCHED:
    case GAIN_MODE_HIGHLOW:

        status = pslGetParameter(detChan, "SWGAIN", &SWGAIN);

        if (status != XIA_SUCCESS) {
            pslLogError("pslGetGainbase", "Error getting switched gain for detChan %d", detChan);
            return status;
        }

        /* Note that the exponent can be negative */
        status = pslGetParameter(detChan, "DGAINBASE", &DGAINBASE);
        status = pslGetParameter(detChan, "DGAINBASEEXP", (parameter_t *)&DGAINBASEEXP);

        if (status != XIA_SUCCESS) {
            pslLogError("pslGetGainbase", "Error getting digital gain for detChan %d", detChan);
            return status;
        }

        /* Manually correct SWGAIN if they are out of bounds */
        maxSwgain = gainMode == GAIN_MODE_SWITCHED ?
                    sizeof(VARIABLE_GAIN_LUT) / sizeof(VARIABLE_GAIN_LUT[0]) - 1 :
                    sizeof(HIGHLOW_GAIN_LUT) / sizeof(HIGHLOW_GAIN_LUT[0]) - 1;
        SWGAIN = (parameter_t)MIN(SWGAIN, maxSwgain);

        *g = pslCalculateBaseGain(gainMode, SWGAIN, DGAINBASE, DGAINBASEEXP);

        break;

    default:
        sprintf(info_string, "Unknown gain mode '%hu'", gainMode);
        pslLogError("pslGetGainbase", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }


    return XIA_SUCCESS;
}


/*
 * Changes the on-board PARSET to the specified value
 */
PSL_STATIC int pslSetParset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;
    int status;

    unsigned short maxParset;

    DEFINE_CMD(CMD_SET_PARSET, 2, 2);

    double parset = *((double *)value);

    UNUSED(name);

    ASSERT(value != NULL);
    ASSERT(defs != NULL);


    sprintf(info_string, "parset = %0.1f", parset);
    pslLogDebug("pslSetParset", info_string);

    statusX = pslGetNumPtPerFiPPI(detChan, NULL, defs, (void *)&maxParset);
    ASSERT(statusX == XIA_SUCCESS);

    /* Verify limits on the PARSETs
     */
    if ((parset >= maxParset) || (parset < 0)) {
        sprintf(info_string, "Specified PARSET '%0.1f' is out-of-range", parset);
        pslLogError("pslSetParset", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)parset;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string,
                "Error setting PARSET for detChan %d",
                detChan);
        pslLogError("pslSetParset", info_string, status);
        return status;
    }

    INVALIDATE("pslSetParset", "energy_gap_time");
    INVALIDATE("pslSetParset", "trigger_peak_time");
    INVALIDATE("pslSetParset", "trigger_gap_time");
    INVALIDATE("pslSetParset", "baseline_length");
    INVALIDATE("pslSetParset", "trigger_threshold");
    INVALIDATE("pslSetParset", "baseline_threshold");
    INVALIDATE("pslSetParset", "energy_threshold");
    INVALIDATE("pslSetParset", "gain_trim");
    INVALIDATE("pslSetParset", "peak_interval");
    INVALIDATE("pslSetParset", "peak_sample");
    INVALIDATE("pslSetParset", "peak_mode");
    INVALIDATE("pslSetParset", "peakint_offset");
    INVALIDATE("pslSetParset", "peaksam_offset");
    INVALIDATE("pslSetParset", "max_width");
    INVALIDATE("pslSetParset", "peaking_time");
    INVALIDATE("pslSetParset", "baseline_factor");

    return XIA_SUCCESS;
}


PSL_STATIC int pslSetGenset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double *genset = (double *)value;

    DEFINE_CMD(CMD_SET_GENSET, 2, 2);


    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    sprintf(info_string, "genset = %0.1f", *genset);
    pslLogDebug("pslSetGenset", info_string);

    send[0] = (byte_t)0;
    send[1] = (byte_t)(*genset);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting GENSET to '%0.1f' for detChan %d",
                *genset, detChan);
        pslLogError("pslSetGenset", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Invalidate GENSET acquisition values here */
    INVALIDATE("pslSetGenset", "number_mca_channels");
    INVALIDATE("pslSetGenset", "mca_bin_width");
    INVALIDATE("pslSetGenset", "number_of_scas");
    INVALIDATE("pslSetGenset", "gain");
    /* Some PARSET values depend on the selected GENSET as well */
    INVALIDATE("pslSetGenset", "trigger_threshold");
    INVALIDATE("pslSetGenset", "baseline_threshold");
    INVALIDATE("pslSetGenset", "energy_threshold");
    INVALIDATE("pslSetGenset", "gain_trim");

    return XIA_SUCCESS;
}

/*
 * Gets the length of the ADC trace from
 * the board. Handel expects an unsigned long,
 * so make the necessary cast.
 */
PSL_STATIC int pslGetADCTraceLen(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    parameter_t HSTLEN = 0x0000;


    UNUSED(defs);


    status = pslGetParameter(detChan, "HSTLEN", &HSTLEN);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetADCTraceLen",
                    "Error getting ADC trace length",
                    status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)HSTLEN;

    return XIA_SUCCESS;
}


/*
 * Calculates the minimum trace wait time in microseconds given the digitizing
 *  clock rate in MHz.
 */
PSL_STATIC double pslMinTraceWait(double clock)
{
    return 1.0 / clock;
}


/*
 * Returns the ADC trace from the board. The wait time is set as an optional
 * acquisition value, adc_trace_wait. If it isn't there then the time is set
 * to 0.
 */
PSL_STATIC int pslGetADCTrace(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;
    int status;

    unsigned int lenS = 2;
    unsigned int lenR = 0;
    unsigned int i;
    unsigned int tracetick = 0;

    byte_t cmd = CMD_READ_ADC_TRACE;

    byte_t send[2];

    byte_t *recv = NULL;

    unsigned long *data = (unsigned long *)value;

    parameter_t HSTLEN = 0x0000;

    double spd       = 0.0;
    double tracewait = 0.0;


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "HSTLEN", &HSTLEN);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetADCTrace",
                    "Error getting HSTLEN",
                    status);
        return status;
    }

    /* If direct readout is supported, we have already triggered collection in
     * the DoTrace phase and only need to read out the history buffer now.
     */
    if (IS_USB && dxp_has_direct_trace_readout(detChan)) {
        char mem[MAXITEM_LEN];
        unsigned long addr = 0;
        parameter_t HSTSTART = 0x0000;

        status = pslGetParameter(detChan, "HSTSTART", &HSTSTART);

        if (status != XIA_SUCCESS) {
            pslLogError("pslGetADCTrace", "Error getting HSTSTART", status);
            return status;
        }

        addr = DSP_DATA_MEMORY_OFFSET + HSTSTART;
        sprintf(mem, "direct:%#lx:%lu", addr, (unsigned long)HSTLEN);

        statusX = dxp_read_memory(&detChan, mem, data);

        if (statusX != DXP_SUCCESS) {
            sprintf(info_string, "Error reading ADC trace directly from the "
                    "USB (%s) for detChan %d.", mem, detChan);
            pslLogError("pslGetADCTrace", info_string, XIA_XERXES);
            return XIA_XERXES;
        }

        return XIA_SUCCESS;
    }

    /* For traditional RS-232 command trace readout, proceed with the command. */

    status = pslGetAcquisitionValues(detChan, "adc_trace_wait", (void *)&tracewait,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting ADC trace wait for detChan %d", detChan);
        pslLogError("pslGetADCTrace", info_string, status);
        return status;
    }

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&spd, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetADCTrace", info_string, status);
        return status;
    }

    tracetick = (unsigned int)ROUND(tracewait * spd) - 1;

    sprintf(info_string, "tracewait = %.3f, tracetick = %u", tracewait, tracetick);
    pslLogDebug("pslGetADCTrace", info_string);

    lenR = (unsigned int)((HSTLEN * 2) + 1 + RECV_BASE);
    recv = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (!recv) {
        sprintf(info_string, "Out-of-memory allocating %zu bytes for 'recv' array",
                lenR * sizeof(byte_t));
        pslLogError("pslGetADCTrace", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    send[0] = (byte_t)LO_BYTE((unsigned int)tracetick);
    send[1] = (byte_t)HI_BYTE((unsigned int)tracetick);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, recv);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)recv);
        recv = NULL;
        status = XIA_XERXES;
        sprintf(info_string,
                "Error reading out ADC trace for detChan %d",
                detChan);
        pslLogError("pslGetADCTrace", info_string, status);
        return status;
    }

    /* The user/Handel want the data in
     * unsigned longs, not bytes.
     */
    for (i = 0; i < HSTLEN; i++) {
        data[i] = (unsigned long)(recv[(i * 2) + 5] |
                                  (unsigned long)recv[(i * 2) + 6] << 8);
    }

    utils->funcs->dxp_md_free((void *)recv);
    recv = NULL;

    return XIA_SUCCESS;
}


/*
 * Sets the bytes per bin value used by the Read MCA command
 */
PSL_STATIC int pslSetBytePerBin(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double bytesPerBin = *((double *)value);

    UNUSED(defs);
    UNUSED(name);

    if ((bytesPerBin < MIN_BYTES_PER_BIN) ||
            (bytesPerBin > MAX_BYTES_PER_BIN)) {
        status = XIA_BPB_OOR;
        sprintf(info_string,
                "bytes_per_bin out-of-range for detChan %d",
                detChan);
        pslLogError("pslDoBytePerBin", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Set the MCA bin width multiplier
 */
PSL_STATIC int pslSetBinWidth(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    DEFINE_CMD(CMD_SET_BIN_WIDTH, 3, 3);

    double *width = (double *)value;


    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    if ((*width > MAX_BIN_WIDTH) || (*width < MIN_BIN_WIDTH)) {
        sprintf(info_string, "Bin width of %0.1f is out-of-range", *width);
        pslLogError("pslSetBinWidth", info_string, XIA_WIDTH_OOR);
        return XIA_WIDTH_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)4;
    send[2] = (byte_t)*width;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting MCA bin width for detChan %d", detChan);
        pslLogError("pslSetBinWidth", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Get the MCA bin width multiplier
 */
PSL_STATIC int pslGetBinWidth(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    DEFINE_CMD(CMD_GET_BIN_WIDTH, 3, 3);

    double *width = (double *)value;

    UNUSED(defs);
    UNUSED(name);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;
    send[2] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting MCA bin width for detChan %d", detChan);
        pslLogError("pslGetBinWidth", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *width = (double)receive[RECV_DATA_OFFSET_STATUS + 1];

    return XIA_SUCCESS;
}


/*
 * Get the number of MCA channels
 */
PSL_STATIC int pslGetNumMCA(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *nChans = (double *)value;

    DEFINE_CMD(CMD_GET_NUM_BINS, 5, 5);


    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;
    send[2] = (byte_t)0;
    send[3] = (byte_t)0;
    send[4] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                "Error getting the number of MCA channels for detChan %d", detChan);
        pslLogError("pslGetNumMCAChannels", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *nChans = (double)BYTE_TO_WORD(receive[RECV_DATA_OFFSET_STATUS],
                                   receive[RECV_DATA_OFFSET_STATUS + 1]);

    return XIA_SUCCESS;
}


/*
 * Sets the number of MCA channels
 */
PSL_STATIC int pslSetNumMCA(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    unsigned int nChans = (unsigned int)(*((double *)value));

    DEFINE_CMD(CMD_SET_NUM_BINS, 5, 5);


    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    if (nChans > MAX_NUM_BINS) {
        sprintf(info_string,
                "Specified number of bins '%u' is greater then the maximum allowed "
                "number '%d' for detChan %d", nChans, MAX_NUM_BINS, detChan);
        pslLogError("pslSetNumMCA", info_string, XIA_NUM_MCA_OOR);
        return XIA_NUM_MCA_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(nChans);
    send[2] = (byte_t)HI_BYTE(nChans);

    /* XXX REPLACE WITH LOW LIMIT ONCE/IF THAT ACQ VALUE IS ADDED */
    send[3] = (byte_t)0;
    send[4] = (byte_t)0;


    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                "Error setting the number of MCA channels for detChan %d", detChan);
        pslLogError("pslSetNumMCA", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((double*)value) = nChans;

    return XIA_SUCCESS;
}


/*
 * Gets the trigger threshold
 */
PSL_STATIC int pslGetTThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
    }


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;
    send[2] = (byte_t)0;
    if (isSuper) {
        send[3] = (byte_t)0;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting trigger threshold for detChan %d",
                detChan);
        pslLogError("pslGetTThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (isSuper) {
        *thresh = pslDoubleFromBytesOffset(receive, 2, RECV_DATA_OFFSET_STATUS);
    } else {
        *thresh = (double)receive[RECV_DATA_OFFSET_STATUS];
    }

    return XIA_SUCCESS;
}


/*
 * sets the trigger threshold
 */
PSL_STATIC int pslSetTThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;
    double MAX_THRESHOLD;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
        MAX_THRESHOLD = MAX_THRESHOLD_STD;
    } else {
        MAX_THRESHOLD = MAX_THRESHOLD_SUPER;
    }


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    if (*thresh > MAX_THRESHOLD) {
        sprintf(info_string,
                "The trigger threshold '%.3f' is larger then the maximum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MAX_THRESHOLD, detChan);
        pslLogError("pslSetTThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }

    if (*thresh < MIN_THRESHOLD) {
        sprintf(info_string,
                "The trigger threshold '%.3f' is smaller then the minimum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MIN_THRESHOLD, detChan);
        pslLogError("pslSetTThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }


    send[0] = (byte_t)0;
    send[1] = (byte_t)0;
    send[2] = (byte_t)(*thresh);
    if (isSuper) {
        send[3] = (byte_t)((int)*thresh >> 8);
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting trigger threshold for detChan %d",
                detChan);
        pslLogError("pslSetTThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the baseline threshold
 */
PSL_STATIC int pslGetBThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
    }


    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)1;
    send[2] = (byte_t)0;
    if (isSuper) {
        send[3] = (byte_t)0;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting baseline threshold for detChan %d",
                detChan);
        pslLogError("pslGetBThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (isSuper) {
        *thresh = pslDoubleFromBytesOffset(receive, 2, RECV_DATA_OFFSET_STATUS + 2);
    } else {
        *thresh = (double)receive[RECV_DATA_OFFSET_STATUS + 1];
    }

    return XIA_SUCCESS;
}


/*
 * Sets the baseline threshold
 */
PSL_STATIC int pslSetBThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;
    double MAX_THRESHOLD;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
        MAX_THRESHOLD = MAX_THRESHOLD_STD;
    } else {
        MAX_THRESHOLD = MAX_THRESHOLD_SUPER;
    }


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    if (*thresh > MAX_THRESHOLD) {
        sprintf(info_string,
                "The baseline threshold '%.3f' is larger then the maximum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MAX_THRESHOLD, detChan);
        pslLogError("pslSetBThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }

    if (*thresh < MIN_THRESHOLD) {
        sprintf(info_string,
                "The baseline threshold '%.3f' is smaller then the minimum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MIN_THRESHOLD, detChan);
        pslLogError("pslSetBThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)1;
    send[2] = (byte_t)(*thresh);
    if (isSuper) {
        send[3] = (byte_t)((int)*thresh >> 8);
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting baseline threshold for detChan %d",
                detChan);
        pslLogError("pslSetBThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the energy threshold
 */
PSL_STATIC int pslGetEThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
    }


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)2;
    send[2] = (byte_t)0;
    if (isSuper) {
        send[3] = (byte_t)0;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting energy threshold for detChan %d",
                detChan);
        pslLogError("pslGetEThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (isSuper) {
        *thresh = pslDoubleFromBytesOffset(receive, 2, RECV_DATA_OFFSET_STATUS + 4);
    } else {
        *thresh = (double)receive[RECV_DATA_OFFSET_STATUS + 2];
    }

    return XIA_SUCCESS;
}


/*
 * Sets the energy threshold
 */
PSL_STATIC int pslSetEThresh(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *thresh = (double *)value;
    double MAX_THRESHOLD;

    DEFINE_CMD(CMD_GET_THRESHOLD, 4, 7);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 4);
        MAX_THRESHOLD = MAX_THRESHOLD_STD;
    } else {
        MAX_THRESHOLD = MAX_THRESHOLD_SUPER;
    }


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    if (*thresh > MAX_THRESHOLD) {
        sprintf(info_string,
                "The energy threshold '%.3f' is larger then the maximum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MAX_THRESHOLD, detChan);
        pslLogError("pslSetEThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }

    if (*thresh < MIN_THRESHOLD) {
        sprintf(info_string,
                "The energy threshold '%.3f' is smaller then the minimum allowed "
                "threshold '%.3f' for detChan %d",
                *thresh, MIN_THRESHOLD, detChan);
        pslLogError("pslSetEThresh", info_string, XIA_THRESH_OOR);
        return XIA_THRESH_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)2;
    send[2] = (byte_t)(*thresh);
    if (isSuper) {
        send[3] = (byte_t)((int)*thresh >> 8);
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting energy threshold for detChan %d",
                detChan);
        pslLogError("pslSetEThresh", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Get the current PARSET
 */
PSL_STATIC int pslGetParset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    DEFINE_CMD(CMD_GET_PARSET, 2, 2);

    double *parset = (double *)value;


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string,
                "Error getting parset information for detChan %d",
                detChan);
        pslLogError("pslGetParset", info_string, status);
        return status;
    }

    *parset = (double)receive[RECV_DATA_OFFSET_STATUS];

    sprintf(info_string, "parset = %.3f", *parset);
    pslLogDebug("pslGetParset", info_string);

    return XIA_SUCCESS;
}


/*
 * Gets the bytes per bin
 */
PSL_STATIC int pslGetBytePerBin(int detChan, char *name, XiaDefaults *defs, void *value)
{
    double *bytes_per_bin = (double *)value;

    UNUSED(detChan);
    UNUSED(defs);
    UNUSED(name);

    *bytes_per_bin = 3.0;

    return XIA_SUCCESS;
}

/*
 * snapshot_sca_length special run data
 * value (unsigned long) same as acq value number_of_scas
 */
PSL_STATIC int pslGetSnapshotScaLen(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    double nSCAs = 0.0;

    ASSERT(defs != NULL);

    status = pslGetAcquisitionValues(detChan, "number_of_scas", (void *)&nSCAs, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error finding 'number_of_scas' for detChan %d", detChan);
        pslLogError("pslGetSnapshotScaLen", info_string, status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)nSCAs;

    return XIA_SUCCESS;
}

/*
 * snapshot_sca special run data
 * value (array of double)
 * in the same sequence as sca run data
 */
PSL_STATIC int pslGetSnapshotSca(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    unsigned long features;
    unsigned long addr;
    
    parameter_t SCASTART;
    
    double number_of_scas;
    double *sca64 = (double *)value;
    
    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SNAPSHOTSCA)) {
        pslLogError("pslGetSnapshotSca", "Connected device does not support "
                "'snapshot_sca' special run value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetDefault("number_of_scas", (void *)&number_of_scas, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "'number_of_scas' is not in the acquisition value "
                "list for detChan %d. Are there SCAs configured for this channel?",
                detChan);
        pslLogError("pslGetSCAData", info_string, status);
        return status;
    }
    
    /* USB raw data readout is supported */
    if (IS_USB) {
        status = pslGetParameter(detChan, "SCASTART", &SCASTART);
        if (status != XIA_SUCCESS) {
            pslLogError("pslGetSCAData", "Error getting SCASTART", status);
            return status;
        }
        addr = DSP_DATA_MEMORY_OFFSET + SCASTART + 32;        
        status = pslGetSCADataDirect(detChan, (int)number_of_scas, addr, sca64);
    } else {
        status = pslGetSCADataCmd(detChan, (int)number_of_scas, CMD_READ_SNAPSHOT_SCA, sca64);
    }

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading snapshot sca for detChan %d", detChan);
        pslLogError("pslGetSnapshotStats", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * snapshot_statistics_length special run data
 * value (unsigned long)
 */
PSL_STATIC int pslGetSnapshotStatsLen(int detChan, void *value, XiaDefaults *defs)
{
    UNUSED(detChan);
    UNUSED(defs);

    *((unsigned long *)value) = NUMBER_STATS;

    return XIA_SUCCESS;
}

/*
 * snapshot_statistics special run data
 * value (array of double)
 * in the same sequence as module_statistics_2 run data
 */
PSL_STATIC int pslGetSnapshotStats(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    unsigned long features;
    parameter_t SNAPSTATSTART = 0x0000;

    double *stats = (double *)value;

    DEFINE_CMD_ZERO_SEND(CMD_READ_SNAPSHOT_STATS, 29);

    UNUSED(defs);

    ASSERT(value != NULL);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SNAPSHOT)) {
        pslLogError("pslGetSnapshotStats", "Connected device does not support "
                "'snapshot_statistics' special run value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    /* USB raw data readout is supported */
    if (IS_USB) {
        status = pslGetParameter(detChan, "SNAPSTATSTART", &SNAPSTATSTART);
        if (status != XIA_SUCCESS) {
            pslLogError("pslGetSnapshotStatsDirect", "Error getting SNAPSTATSTART", status);
            return status;
        }
        status = pslReadDirectUsbMemory(detChan,
                    (unsigned long)(DSP_DATA_MEMORY_OFFSET + SNAPSTATSTART),
                    (unsigned long)lenR - RECV_BASE - 1, &receive[RECV_BASE]);
    } else {
        status = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);
    }

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error reading snapshot statistics for detChan %d", detChan);
        pslLogError("pslGetSnapshotStats", info_string, status);
        return status;
    }

    stats[TriggerLivetime] =  pslDoubleFromBytesOffset(receive, 6, RECV_BASE) * LIVETIME_CLOCK_TICK;
    stats[Realtime] =  pslDoubleFromBytesOffset(receive, 6, 11) * REALTIME_CLOCK_TICK;
    stats[EnergyLivetime] = 0.0;
    stats[Triggers] =  pslDoubleFromBytesOffset(receive, 4, 17);
    stats[Events] =  pslDoubleFromBytesOffset(receive, 4, 21);
    stats[Underflows] = pslDoubleFromBytesOffset(receive, 4, 25);
    stats[Overflows] = pslDoubleFromBytesOffset(receive, 4, 29);
    stats[Ocr] = (stats[Realtime] == 0.0) ? 0.0 : (stats[Events] + stats[Underflows] + stats[Overflows]) / stats[Realtime];
    stats[Icr] = (stats[TriggerLivetime] == 0.0) ? 0.0 : stats[Triggers] / stats[TriggerLivetime];

    return XIA_SUCCESS;
}

/*
 * snapshot_mca_length special run data
 * value (unsigned long)
 */
PSL_STATIC int pslGetSnapshotMcaLen(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    parameter_t MCALEN = 0x0000;

    UNUSED(defs);

    status = pslGetParameter(detChan, "MCALEN", &MCALEN);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetSnapshotMcaLen", "Error getting snapshot mca length",
                    status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)MCALEN;

    return XIA_SUCCESS;
}

/*
 * snapshot_mca special run data
 * value (array of unsigned long)
 */
PSL_STATIC int pslGetSnapshotMca(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    int statusX;

    unsigned long features;

    unsigned int i;
    unsigned int dataLen;
    unsigned int lenS = 5;
    unsigned int lenR = 0;

    unsigned long *data = (unsigned long *)value;

    double bytesPerBin = 0.0;
    double numMCAChans = 0.0;
    double mcaLowLim = 0.0;

    parameter_t SNAPSHOTSTART = 0x0000;

    byte_t cmd = CMD_READ_SNAPSHOT_MCA;

    byte_t send[5];

    byte_t *receive = NULL;

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SNAPSHOT)) {
        pslLogError("pslGetSnapshotMca", "Connected device does not support "
                    "'snapshot_mca' special run value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetAcquisitionValues(detChan, "bytes_per_bin", (void *)&bytesPerBin, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting bytes per bin for detChan %d", detChan);
        pslLogError("pslGetSnapshotMca", info_string, status);
        return status;
    }

    status = pslGetAcquisitionValues(detChan, "number_mca_channels", (void *)&numMCAChans, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting number of MCA channels for detChan %d", detChan);
        pslLogError("pslGetSnapshotMca", info_string, status);
        return status;
    }

    sprintf(info_string, "bytesPerBin = %.3f, numMCAChans = %.3f, mcaLowLim = %.3f",
            bytesPerBin, numMCAChans, mcaLowLim);
    pslLogDebug("pslGetSnapshotMca", info_string);

    if (IS_USB) {
        status = pslGetParameter(detChan, "SNAPSHOTSTART", &SNAPSHOTSTART);

        if (status != XIA_SUCCESS) {
            pslLogError("pslGetSnapshotMca", "Error getting SNAPSHOTSTART", status);
            return status;
        }

        status = pslGetMcaDirect(detChan, (int)bytesPerBin,
                          (int)numMCAChans, (unsigned long)SNAPSHOTSTART, data);
        return status;
    }

    dataLen = (unsigned int)(bytesPerBin * numMCAChans);
    lenR = (unsigned int)(dataLen + 1 + RECV_BASE);

    send[0] = LO_BYTE((unsigned short)mcaLowLim);
    send[1] = HI_BYTE((unsigned short)mcaLowLim);
    send[2] = LO_BYTE((unsigned short)numMCAChans);
    send[3] = HI_BYTE((unsigned short)numMCAChans);
    send[4] = (byte_t)bytesPerBin;

    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (receive == NULL) {
        pslLogError("pslGetSnapshotMca", "Out-of-memory trying to create "
                    "receive array", XIA_NOMEM);
        return XIA_NOMEM;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        sprintf(info_string, "Error getting MCA data from detChan %d", detChan);
        pslLogError("pslGetSnapshotMca", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Transfer the spectra to the user's array. */
    for (i = 0; i < numMCAChans; i++) {
        data[i] = pslUlFromBytesOffset(receive, (int)bytesPerBin,
                            RECV_BASE + i * (int)bytesPerBin);
    }

    utils->funcs->dxp_md_free((void *)receive);
    return XIA_SUCCESS;
}


PSL_STATIC int pslGetMcaDirect(int detChan, int bytesPerBin,
                                    int numMCAChans, unsigned long startAddr,
                                    unsigned long *data)
{
    int status;
    int i;

    unsigned int dataLen;
    byte_t *receive;

    ASSERT(IS_USB);


    /* Note that the data in spectrum memory will always contain 4 bytes
     * Despite of the bytePerBin setting
     */
    dataLen = numMCAChans * RAW_BYTES_PER_BIN;
    receive = (byte_t *)utils->funcs->dxp_md_alloc(dataLen * sizeof(byte_t));

    if (receive == NULL) {
        pslLogError("pslGetMcaDirect", "Out-of-memory trying to create "
                    "receive array", XIA_NOMEM);
        return XIA_NOMEM;
    }

    status = pslReadDirectUsbMemory(detChan, startAddr, dataLen, receive);

    if (status != XIA_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        pslLogError("pslGetMcaDirect", "Error getting data", status);
        return status;
    }

    /* Now it's time to discard the extra byte */
    for (i = 0; i < numMCAChans; i++) {
        data[i] = pslUlFromBytesOffset(receive, bytesPerBin, i * RAW_BYTES_PER_BIN);
    }

    utils->funcs->dxp_md_free((void *)receive);
    return XIA_SUCCESS;
}

PSL_STATIC int pslReadDirectUsbMemory(int detChan, unsigned long address,
                                    unsigned long num_bytes, byte_t *receive)
{
    int statusX;

    unsigned int i;
    char mem[MAXITEM_LEN];

    unsigned long size = 0;
    unsigned long *data;

    ASSERT(IS_USB);

    size = (unsigned long)ceil(num_bytes / 2.0);
    data = (unsigned long *)utils->funcs->dxp_md_alloc(size * sizeof(unsigned long));

    if (data == NULL) {
        pslLogError("pslReadDirectUsbMemory", "Out-of-memory trying to "
                    "create data array", XIA_NOMEM);
        return XIA_NOMEM;
    }

    sprintf(mem, "direct:%#lx:%lu", address, size);
    statusX = dxp_read_memory(&detChan, mem, data);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)data);
        sprintf(info_string, "Error reading data directly from the "
                "USB (%s) for detChan %d.", mem, detChan);
        pslLogError("pslReadDirectUsbMemory", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    sprintf(info_string, "readout mem (%s) for detChan %d.",  mem, detChan);
    pslLogDebug("pslReadDirectUsbMemory", info_string);

    for (i = 0; i < (num_bytes / 2.0); i++) {
        receive[i * 2] = LO_BYTE(data[i]);
        receive[i * 2 + 1] = HI_BYTE(data[i]);
    }

    utils->funcs->dxp_md_free((void *)data);

    return XIA_SUCCESS;
}


PSL_STATIC int pslGetBaseHistLen(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    parameter_t HSTLEN = 0x0000;

    UNUSED(defs);

    status = pslGetParameter(detChan, "HSTLEN", &HSTLEN);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetBaseHistLen",
                    "Error getting baseline history length",
                    status);
        return status;
    }

    *((unsigned long *)value) = (unsigned long)HSTLEN;

    return XIA_SUCCESS;
}



PSL_STATIC int pslGetBaseHist(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;
    int status;

    unsigned int lenS = 0;
    unsigned int lenR = 0;
    unsigned int i;

    byte_t cmd       = CMD_READ_BASELINE_HIST;

    byte_t *receive = NULL;

    unsigned long *data = (unsigned long *)value;

    parameter_t HSTLEN = 0x0000;

    UNUSED(defs);

    status = pslGetParameter(detChan, "HSTLEN", &HSTLEN);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetBaseHist",
                    "Error getting HSTLEN",
                    status);
        return status;
    }

    lenR = (unsigned int)((HSTLEN * 2) + 1 + RECV_BASE);
    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (receive == NULL) {
        status = XIA_NOMEM;
        pslLogError("pslGetBaseHist",
                    "Out-of-memory creating receive array",
                    status);
        return status;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        receive = NULL;
        status = XIA_XERXES;
        sprintf(info_string,
                "Error reading out baseline history for detChan %d",
                detChan);
        pslLogError("pslGetBaseHist", info_string, status);
        return status;
    }

    /* The user/Handel want the data in unsigned longs. */
    for (i = 0; i < HSTLEN; i++) {
        data[i] = pslUlFromBytesOffset(receive, 2, RECV_BASE + i * 2);
    }

    utils->funcs->dxp_md_free((void *)receive);
    receive = NULL;

    return XIA_SUCCESS;
}


/*
 * Asks the board for the current run status.
 */
PSL_STATIC int pslGetRunActive(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    byte_t cmd       = CMD_STATUS;

    unsigned int lenS = 0;
    unsigned int lenR = 6 + RECV_BASE;

    byte_t recv[6 + RECV_BASE];


    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, recv);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading board status for detChan %d",
                detChan);
        pslLogError("pslGetRunActive", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((unsigned long *)value) = (unsigned long)recv[7];

    return XIA_SUCCESS;
}


/*
 * Get the current list of peaking times for the board
 */
PSL_STATIC int pslGetCurrentPeakingTimes(int detChan, char *name,
                                         XiaDefaults *defs, void *value)

{
    int status;

    double *pts = (double *)value;

    UNUSED(name);

    ASSERT(value != NULL);

    status = pslReadoutPeakingTimes(detChan, defs, FALSE_, pts);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error getting peaking times from detChan %d", detChan);
        pslLogError("pslGetCurrentPeakingTimes", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Get a list of all peaking times for the board
 */
PSL_STATIC int pslGetPeakingTimes(int detChan, char *name,
                                         XiaDefaults *defs, void *value)

{
    int status;
    double *pts = (double *)value;

    UNUSED(name);
    ASSERT(value != NULL);

    status = pslReadoutPeakingTimes(detChan, defs, TRUE_, pts);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error getting peaking times from detChan %d", detChan);
        pslLogError("pslGetPeakingTimes", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Read out the peaking time array by CMD_READ_SLOWLEN_VALS
 */
PSL_STATIC int pslReadoutPeakingTimes(int detChan, XiaDefaults *defs,
                                      boolean_t allFippis, double *pts)
{
    int statusX;
    int status;
    int i;
    int bytePerPt = pslNumBytesPerPt(detChan);

    double baseclock;
    double currentFiPPI;

    unsigned short nFiPPIs;
    unsigned short ptPerFippi;

    DEFINE_CMD_ZERO_SEND(CMD_READ_SLOWLEN_VALS, 52);

    status = pslGetNumFiPPIs(detChan, NULL, defs, (void *)&nFiPPIs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error getting number of FiPPIS from detChan %d", detChan);
        pslLogError("pslReadoutPeakingTimes", info_string, status);
        return status;
    }

    status = pslGetNumPtPerFiPPI(detChan, NULL, defs, (void *)&ptPerFippi);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetBaseClock(detChan, &baseclock);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting base clock for detChan %d", detChan);
        pslLogError("pslReadoutPeakingTimes", info_string, status);
        return status;
    }

    lenR = (ptPerFippi * bytePerPt + 1 ) * nFiPPIs + 3 + RECV_BASE;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                    "Error reading SLOWLEN values for detChan %d", detChan);
        pslLogError("pslReadoutPeakingTimes", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (allFippis) {
      for (i = 0; i < nFiPPIs; i++) {
          pslCalculatePeakingTimes(detChan, i, ptPerFippi, baseclock, receive,
                                    (double *)&pts[i * ptPerFippi]);
      }
    } else {

      status = pslGetAcquisitionValues(detChan, "fippi", (void *)&currentFiPPI,
                                       defs);

      if (status != XIA_SUCCESS) {
          sprintf(info_string,
                  "Error getting current FiPPI for detChan %d", detChan);
          pslLogError("pslReadoutPeakingTimes", info_string, status);
          return status;
      }

      pslCalculatePeakingTimes(detChan, (int)currentFiPPI, ptPerFippi, baseclock,
                            receive, pts);
    }

    return XIA_SUCCESS;
}


/*
 * Calculates the peaking time array from the data received by CMD_READ_SLOWLEN_VALS
 */
PSL_STATIC void pslCalculatePeakingTimes(int detChan, int fippi,
      unsigned short ptPerFippi, double baseclock, byte_t *receive, double *pts)
{
    int i;

    int slowlenOffset;
    int decIdx;

    int bytePerPt = pslNumBytesPerPt(detChan);
    double ptTick;

    byte_t DECIMATION;
    byte_t CLKSET;

    /* Return data structure
     * Status (RECV_BASE)
     * CLKSET
     * NFIPPI
     * DECIMATION[FIPPI]
     * SLOWLEN[1..ptPerFippi][FIPPI]
     */
    decIdx = RECV_DATA_OFFSET_STATUS + 2 +
             ((unsigned short)fippi * (ptPerFippi * bytePerPt + 1 ));
    DECIMATION = receive[decIdx];

    CLKSET = receive[RECV_DATA_OFFSET_STATUS];

    slowlenOffset = RECV_DATA_OFFSET_STATUS + 2 +
                    ((unsigned short)fippi * (ptPerFippi * bytePerPt + 1 )) + 1;

    ptTick = (1.0 / baseclock) * pow(2.0, CLKSET + DECIMATION);

    sprintf(info_string, "DEC = %u, CLK = %#x, tick = %0.3f",
            DECIMATION, CLKSET, ptTick);
    pslLogDebug("pslCalculatePeakingTimes", info_string);

    for (i = 0; i < ptPerFippi; i++) {
        pts[i] = ptTick * pslDoubleFromBytes(
                     &receive[slowlenOffset + i * bytePerPt], bytePerPt);
    }
}



/*
 * Reads the serial number from the
 * board.
 */
PSL_STATIC int pslGetSerialNumber(int detChan, char *name, XiaDefaults *defs,
                                  void *value)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int lenS = 0;
    unsigned int lenR = 18 + RECV_BASE;

    byte_t cmd       = CMD_GET_SERIAL_NUMBER;

    byte_t receive[18 + RECV_BASE];

    char *serialNum = (char *)value;


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        pslLogError("pslGetSerialNumber",
                    "Error reading serial number from board",
                    status);
        return status;
    }

    for (i = 0; i < SERIAL_NUM_LEN; i++) {
        serialNum[i] = (char)receive[i + 5];
    }

    /* Must allocate SERIAL_NUM_LEN + 1 for termination */
    serialNum[SERIAL_NUM_LEN] = '\0';

    return XIA_SUCCESS;
}


PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value,
                                 XiaDefaults *defs)
{
    int status;
    int i;
    int nOps = sizeof(boardOps) / sizeof(boardOps[0]);


    ASSERT(name != NULL);
    ASSERT(value != NULL);

    for (i = 0; i < nOps; i++) {
        if (STREQ(boardOps[i].name, name)) {
            status = boardOps[i].fn(detChan, name, defs, value);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error doing '%s' operation for detChan %d",
                        name, detChan);
                pslLogError("pslBoardOperation", info_string, status);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    sprintf(info_string, "Unknown board operation: '%s'", name);
    pslLogError("pslBoardOperation", info_string, XIA_BAD_NAME);
    return XIA_BAD_NAME;
}


/*
 * Save the current values to the specified GENSET
 */
PSL_STATIC int pslSaveGenset(int detChan, char *name, XiaDefaults *defs,
                             void *value)
{
    int statusX;

    DEFINE_CMD(CMD_SAVE_GENSET, 3, 2);

    unsigned short genset = *((unsigned short *)value);

    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    sprintf(info_string, "Saving genset = %u", genset);
    pslLogDebug("pslSaveGenset", info_string);

    send[0] = (byte_t)genset;
    send[1] = (byte_t)0x55;
    send[2] = (byte_t)0xAA;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error saving GENSET to detChan %d", detChan);
        pslLogError("pslSaveGenset", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /*   if (receive[5] != genset) { */
    /*    sprintf(info_string, "GENSET mismatch returned from hardware: '%u' != '%u'", */
    /*            receive[5], genset); */
    /*    pslLogError("pslSaveGenset", info_string, XIA_GENSET_MISMATCH); */
    /*    return XIA_GENSET_MISMATCH; */
    /*   } */

    return XIA_SUCCESS;
}


/*
 * Save the current values to the specified PARSET
 */
PSL_STATIC int pslSaveParset(int detChan, char *name, XiaDefaults *defs,
                             void *value)
{
    int statusX;
    unsigned short maxParset;

    DEFINE_CMD(CMD_SAVE_PARSET, 3, 2);

    unsigned short parset = *((unsigned short *)value);


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);

    sprintf(info_string, "parset = %u", parset);
    pslLogDebug("pslSaveParset", info_string);

    statusX = pslGetNumPtPerFiPPI(detChan, NULL, defs, (void *)&maxParset);
    ASSERT(statusX == XIA_SUCCESS);

    /* Verify limits on the PARSETs
     */
    if ((parset >= maxParset) || (parset < 0)) {
        sprintf(info_string, "Specified PARSET '%u' is out-of-range", parset);
        pslLogError("pslSaveParset", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    send[0] = (byte_t)parset;
    send[1] = (byte_t)0x55;
    send[2] = (byte_t)0xAA;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error saving PARSET to detChan %d", detChan);
        pslLogError("pslSaveParset", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * This routine returns the current selected
 * FiPPI.
 */
PSL_STATIC int pslGetFiPPI(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double *fippi = (double *)value;

    DEFINE_CMD(CMD_GET_FIPPI_CONFIG, 2, 3);


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string, "Error getting fippi value from detChan %d",
                detChan);
        pslLogError("pslGetFiPPI", info_string, status);
        return status;
    }

    *fippi = (double)receive[RECV_DATA_OFFSET_STATUS];

    return XIA_SUCCESS;
}


/*
 * Switch to the specified FiPPI
 */
PSL_STATIC int pslSetFiPPI(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double fippi = *((double *)value);
    unsigned short numberFippis = 0;

    DEFINE_CMD(CMD_SET_FIPPI_CONFIG, 2, 3);

    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetNumFiPPIs(detChan, NULL, NULL, (void *)&numberFippis);
    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting number of FiPPIs");
        pslLogError("pslSetFiPPI", info_string, status);
        return status;
    }

    if ((fippi < 0.0) || (fippi >= numberFippis)) {
        status = XIA_FIP_OOR;
        sprintf(info_string, "Specified FiPPI %0.1f is not a valid value",
                fippi);
        pslLogError("pslSetFiPPI", info_string, status);
        return status;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)fippi;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR,
                      receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string, "Error setting FiPPI on detChan %d", detChan);
        pslLogError("pslSetFiPPI", info_string, status);
        return status;
    }

    status = pslInvalidateAll(AV_MEM_PARSET, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error invalidating PARSET data for detChan %d",
                detChan);
        pslLogError("pslSetFiPPI", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Get the gain mode
 */
PSL_STATIC int pslGetGainMode(int detChan, char *name, XiaDefaults *defs,
                              void *value)
{
    int status;

    unsigned short *gainMode = (unsigned short *)value;

    parameter_t GAINMODE;

    UNUSED(defs);
    UNUSED(name);

    /* Instead of excuting CMD_GET_BOARD_INFO retrieve the DSP parameter here
     * to avoid stopping an active run */
    status = pslGetParameter(detChan, "GAINMODE", &GAINMODE);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting GAINMODE for detChan %d", detChan);
        pslLogError("pslGetGainMode", info_string, status);
        return status;
    }

    *gainMode = (unsigned short)GAINMODE;

    /* VGA + Digital and Switched gain modes only supported by supermicro */
    ASSERT((*gainMode < GAIN_MODE_DIGITAL) || dxp_is_supermicro(detChan));

    return XIA_SUCCESS;
}

/*
 * Reads the number of FiPPIs from the board and returns it
 * to the user.
 */
PSL_STATIC int pslGetNumFiPPIs(int detChan, char *name, XiaDefaults *defs,
                               void *value)
{
    int statusX;
    int status;

    unsigned short *nFiPPIs = (unsigned short *)value;

    unsigned int lenS = 0;
    unsigned int lenR = RECV_BASE + 27;

    byte_t cmd = CMD_GET_BOARD_INFO;

    byte_t receive[RECV_BASE + 27];


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string,
                "Error getting board information from detChan %d",
                detChan);
        pslLogError("pslGetNumFiPPIs", info_string, status);
        return status;
    }

    *nFiPPIs = (unsigned short)receive[13];

    return XIA_SUCCESS;
}


/*
 * Get the peaking times and return the max and min for each FiPPi
 * in an array of doubles
 */
PSL_STATIC int pslGetPTRanges(int detChan, char *name, XiaDefaults *defs,
                              void *value)
{
    int statusX;
    int status;
    int bytePerPt = pslNumBytesPerPt(detChan);

    unsigned short ptPerFippi = 0;
    unsigned short nFiPPIs = 0;

    double baseclock;
    double *ranges = (double *)value;

    byte_t cmd       = CMD_READ_SLOWLEN_VALS;

    unsigned int lenS = 0;
    unsigned int lenR = 0;

    /* This is set to the maximum size so that we can avoid
     * dynamically allocating memory. If the board has fewer
     * then 3 FiPPIs, the other values in the array will be
     * ignored.
     */
    byte_t receive[52 + RECV_BASE];

    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetNumPtPerFiPPI(detChan, NULL, defs, (void *)&ptPerFippi);
    ASSERT(status == XIA_SUCCESS);

    status = pslGetNumFiPPIs(detChan, NULL, defs, (void *)&nFiPPIs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error getting number of FiPPIs from detChan %d", detChan);
        pslLogError("pslGetPTRanges", info_string, status);
        return status;
    }

    /* Return data structure
     * Status (RECV_BASE)
     * CLKSET
     * NFIPPI
     * DECIMATION[FIPPI]
     * SLOWLEN[1..ptPerFippi][FIPPI]
     */
    lenR = (ptPerFippi * bytePerPt + 1) * nFiPPIs + 3 + RECV_BASE;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                "Error reading SLOWLEN values from detChan %d", detChan);
        pslLogError("pslGetPTRanges", info_string, status);
        return status;
    }

    ASSERT(nFiPPIs == receive[1 + RECV_BASE]);

    status = pslGetBaseClock(detChan, &baseclock);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting base clock for detChan %d", detChan);
        pslLogError("pslGetPTRanges", info_string, status);
        return status;
    }

    status = pslCalculateRanges((byte_t)nFiPPIs, ptPerFippi, bytePerPt,
                                baseclock, receive[RECV_BASE],
                                receive + RECV_BASE + 2, ranges);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error calculating peaking time ranges for "
                "detChan %d", detChan);
        pslLogError("pslGetPTRanges", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Calculate the peaking time ranges from data returned in a format
 *  similar to that of command 0x90
 */
PSL_STATIC int pslCalculateRanges(byte_t nFiPPIs, int ptPerFippi, int bytePerPt,
                                  double BASE_CLOCK, byte_t CLKSET, byte_t *data,
                                  double *ranges)
{
    byte_t i;
    byte_t j;
    byte_t dec;

    double min;
    double max;
    double val;

    int offset = 0;

    double ptBase = 0.0;

    ASSERT(nFiPPIs > 0);
    ASSERT(data    != NULL);
    ASSERT(ranges  != NULL);

    for (i = 0; i < nFiPPIs; i++) {

        dec = data[offset];
        min = pslDoubleFromBytes(&data[offset + 1], bytePerPt);
        max = min;

        for (j = 0; j < ptPerFippi; j++) {
            val = pslDoubleFromBytes(&data[offset + j * bytePerPt + 1], bytePerPt);
            if (val < min) {
                min = val;
            } else if (val > max) {
                max = val;
            }
        }

        sprintf(info_string, "dec = %u, min = %0f, max = %0f", dec, min, max);
        pslLogDebug("pslCalculateRanges", info_string);

        ptBase = (1.0 / BASE_CLOCK) * pow(2.0, (double)CLKSET + (double)dec);

        sprintf(info_string, "ptBase = %.3f", ptBase);
        pslLogDebug("pslCalculateRanges", info_string);

        ranges[i * 2]       = min * ptBase;
        ranges[(i * 2) + 1] = max * ptBase;

        offset += (ptPerFippi * bytePerPt + 1);
    }

    return XIA_SUCCESS;
}


/*
 * Calls the associated Xerxes exit routine as part of
 * the board-specific shutdown procedures.
 */
PSL_STATIC int pslUnHook(int detChan)
{
    int statusX;
    int status;

    sprintf(info_string, "Unhooking detChan %d", detChan);
    pslLogDebug("pslUnHook", info_string);

    statusX = dxp_exit(&detChan);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string, "Error shutting down detChan %d", detChan);
        pslLogError("pslUnHook", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Reads out the history sector of the flash memory and
 * returns all 256 bytes in the value array. This board
 * operation is undocumented. "value" is assumed to be
 * of the proper size.
 */
PSL_STATIC int pslGetHistorySector(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int status;
    int statusX;

    unsigned int i;
    unsigned int addr = 0x0000;
    unsigned int lenS = 3;
    unsigned int lenR = 65 + RECV_BASE;

    byte_t cmd       = CMD_READ_FLASH;

    byte_t *history = (byte_t *)value;

    byte_t send[3];
    byte_t receive[65 + RECV_BASE];


    UNUSED(name);
    UNUSED(defs);


    for (i = 0, addr = XUP_HISTORY_ADDR; i < 4; i++, addr += MAX_FLASH_READ) {
        send[0] = (byte_t)LO_BYTE(addr);
        send[1] = (byte_t)HI_BYTE(addr);
        send[2] = (byte_t)MAX_FLASH_READ;

        statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                          &lenR, receive);

        if (statusX != DXP_SUCCESS) {
            status = XIA_XERXES;
            pslLogError("pslGetHistorySector",
                        "Error reading XUP history from board",
                        status);
            return status;
        }

        memcpy(history + (i * 64), receive + 5, MAX_FLASH_READ * 2);
    }

    return XIA_SUCCESS;
}


/*
 * Retrieves the board info (cmd 0x49)
 */
PSL_STATIC int pslGetBoardInfo(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    byte_t *data = (byte_t *)value;

    DEFINE_CMD_ZERO_SEND(CMD_GET_BOARD_INFO, 27);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                "Error getting board information for detChan %d",
                detChan);
        pslLogError("pslGetBoardInfo", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* XXX Fix these magic numbers! */
    memcpy(data, receive + RECV_BASE, 26);

    return XIA_SUCCESS;
}

PSL_STATIC double pslDoubleFromBytes(byte_t *bytes, int size)
{
    return pslDoubleFromBytesOffset(bytes, size, 0);
}

/*
 * Computes an unsigned integer from a sequence of bytes. bytes[0] should be
 * the least significant byte. size is the number of bytes to use from the
 * buffer. The mantissa of a double has more bits than a long, so we'll use
 * that until we drop support of compilers without uint64_t. size must fit
 * in the mantissa (typically 53 bits, so 6 bytes max).
 */
PSL_STATIC double pslDoubleFromBytesOffset(byte_t *bytes, int size, int offset)
{
    int i;
    double value;

    /* Fit the unsigned integer into the mantissa of a double. */
    ASSERT(size <= (DBL_MANT_DIG / 8));

    for (i = 0, value = 0; i < size; i++) {
        value += ((unsigned long long)bytes[offset + i]) << (i * 8);
    }

    return value;
}

PSL_STATIC unsigned long pslUlFromBytesOffset(byte_t *bytes, int size, int offset)
{
    int i;
    unsigned long value;

    ASSERT(size <= (int)sizeof(value));

    for (i = 0, value = 0; i < size; i++) {
        value += ((unsigned long)bytes[offset + i]) << (i * 8);
    }

    return value;
}

/*
 * Computes a fraction from the bits word. Bit 7 represents 2^-1 (0.5),
 * bit 6 is 2^-2 (0.25), and so on.
 *
 * Here are some example inputs and resulting fractions:
 *   255: 0.9375
 *   128: 0.5
 *    16: 0.0625
 *     0: 0.0
 *
 * word: An unsigned value whose bits are used in the computation.
 *
 * nBits: Number of bits to use in the computation. Computation starts
 * at the highest bit number and works down.
 *
 * Note: There's nothing particularly PSL-specific about this routine. It's
 * here because that's the only place it's used currently. If other udxp
 * libraries need it, move this to udxp_common (probably with a different name
 * to match the convention there) and add that file to the udxp_psl makefile.
 */
PSL_STATIC double pslComputeFraction(byte_t word, int nBits)
{
    int i;

    double fraction = 0.0;


    for (i = 8 - nBits; i >= 0 && i < 8; i++) {
        if (word & (1 << i)) {
            fraction += pow(2, -1 * (8 - i));
        }
    }

    return fraction;
}


/*
 * Retrieves the temperature from the onboard digital temperature sensor.
 *
 * value returns the temperature to 1/16 of a degree Celsius as a
 * double.
 */
PSL_STATIC int pslGetTemperature(int detChan, char *name, XiaDefaults *defs,
                                 void *value)
{
    int statusX;
    int nBits;

    double *temperature = (double *)value;

    DEFINE_CMD_ZERO_SEND(CMD_GET_TEMPERATURE, 3);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting temperature for detChan %d",
                detChan);
        pslLogError("pslGetTemperature", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Start with the signed integer part. */
    *temperature = (signed char)receive[RECV_DATA_OFFSET + 1];

    /* Then compute a decimal value from the bits of the fractional part. Per
     * the command spec, only bits 4-7 are meaningful, providing accuracy to
     * 1/16 of a degree. Bits 0-3 are not used.
     *
     * Per the command spec, the fraction is always included by adding to the
     * integer part, regardless of whether the integer is negative or positive.
     */
    nBits = 4;
    *temperature += pslComputeFraction(receive[RECV_DATA_OFFSET + 2], nBits);

    return XIA_SUCCESS;
}


/*
 * Converts the stored RC Tau value into microseconds
 */
PSL_STATIC int pslGetRCTau(int detChan, XiaDefaults *defs, double *val)
{
    int status;
    int statusX;

    unsigned short taurc = 0x0000;

    double cs = DECAYTIME_CLOCK_SPEED;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    DEFINE_CMD(CMD_GET_RCFEED, 3, 3);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;
    send[2] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting RC tau value for detChan %d", detChan);
        pslLogError("pslGetRCTau", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (isSuper) {
        status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&cs, defs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
            pslLogError("pslGetRCTau", info_string, status);
            return status;
        }
    }

    taurc = (unsigned short)(((unsigned short)receive[6] << 8) | receive[5]);

    *val = (double)(taurc / cs);

    return XIA_SUCCESS;
}


/*
 * Get the current reset interval from the hardware
 */
PSL_STATIC int pslGetResetInterval(int detChan, double *value)
{
    int statusX;

    DEFINE_CMD(CMD_GET_RESET, 2, 2);


    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting reset interval for detChan %d", detChan);
        pslLogError("pslGetResetInterval", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *value = (double)receive[RECV_DATA_OFFSET_STATUS];

    return XIA_SUCCESS;
}


/*
 * Sets the preamplifier type-specific value
 *
 * This routine dispatches to the appropriate routine based on the
 * firmware determined preamplifier type.
 */
PSL_STATIC int pslSetPreampVal(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    unsigned short type = 0;

    double *val = (double *)value;

    ASSERT(value != NULL);
    ASSERT(defs  != NULL);

    UNUSED(name);

    status = pslQueryPreampType(detChan, &type);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting preamp type for detChan %d", detChan);
        pslLogError("pslSetPreampVal", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    switch(type) {
    case PREAMP_TYPE_RESET:
        status = pslSetResetInterval(detChan, val);
        break;

    case PREAMP_TYPE_RC:
        status = pslSetRCTau(detChan, defs, val);
        break;

    default:
        FAIL();
        break;
    }

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting preamp value for detChan %d", detChan);
        pslLogError("pslSetPreampVal", info_string, status);
        return status;
    }

    /* pslSetResetInterval() and pslSetRCTau() can round the specified value, but
     * set *val to the rounded value. pslApply() will cache this rounded value.
     */

    return XIA_SUCCESS;
}


/*
 * Gets the current preamplifier type-specific value
 */
PSL_STATIC int pslGetPreampVal(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    unsigned short type = 0;

    double *val = (double *)value;


    ASSERT(value != NULL);

    UNUSED(name);

    status = pslQueryPreampType(detChan, &type);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting preamp type for detChan %d", detChan);
        pslLogError("pslGetPreampVal", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    switch(type) {
    case PREAMP_TYPE_RESET:
        status = pslGetResetInterval(detChan, val);
        break;

    case PREAMP_TYPE_RC:
        status = pslGetRCTau(detChan, defs, val);
        break;

    default:
        FAIL();
        break;
    }

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting preamp value for detChan %d", detChan);
        pslLogError("pslGetPreampVal", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the reset interval via the appropriate command.
 * This routine may have to round the value requested
 * by the user.
 */
PSL_STATIC int pslSetResetInterval(int detChan, double *value)
{
    int statusX;

    byte_t rt = 0;

    double *resetinterval = (double *)value;

    DEFINE_CMD(CMD_SET_RESET, 2, 2);

    ASSERT(value != NULL);

    if (*resetinterval > MAX_RESET_INTERVAL || *resetinterval < 0) {
        sprintf(info_string, "Requested reset interval (%0.2f) is out of range "
             "(%d, %d), resetting to max (%d).", *resetinterval, 0,
             MAX_RESET_INTERVAL, MAX_RESET_INTERVAL);
        pslLogWarning("pslSetResetInterval", info_string);
        *resetinterval = MAX_RESET_INTERVAL;
    }

    rt = (byte_t)ROUND(*resetinterval);

    send[0] = (byte_t)0;
    send[1] = rt;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting reset interval for detChan %d", detChan);
        pslLogError("pslSetResetInterval", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *resetinterval = (double)receive[RECV_DATA_OFFSET_STATUS];
    return XIA_SUCCESS;
}


/*
 * Set the RC Tau decay time
 */
PSL_STATIC int pslSetRCTau(int detChan, XiaDefaults *defs, double *value)
{
    int status;
    int statusX;

    DEFINE_CMD(CMD_SET_RCFEED, 3, 3);

    double *decaytime = (double *)value;
    double max = 0.0;

    double cs = DECAYTIME_CLOCK_SPEED;

    unsigned short taurc = 0;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    ASSERT(value != NULL);
    ASSERT(defs  != NULL);

    if (isSuper) {
        status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&cs, defs);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
            pslLogError("pslSetRCTau", info_string, status);
            return status;
        }
    }

    /* Maximum of decay time scaled by clockspeed */
    max = (double)MAX_DECAY_TIME / cs;

    if (*decaytime > max || *decaytime < 0) {
        sprintf(info_string, "Requested decay time (%0.2f) is out of range "
             "(%d, %0.2f), resetting to max (%0.2f).", *decaytime, 0, max, max);
        pslLogWarning("pslSetRCTau", info_string);
        *decaytime = max;
    }

    taurc  = (unsigned short)ROUND(*decaytime * cs);

    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(taurc);
    send[2] = (byte_t)HI_BYTE(taurc);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting RC time constant for detChan %d",
                detChan);
        pslLogError("pslSetRCTau", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Pass the value returned from CMD_SET_RCFEED */
    *decaytime = ((double)BYTE_TO_WORD(receive[5], receive[6])) / cs;

    return XIA_SUCCESS;
}


/*
 * Retrieves the polarity setting from the hardware
 */
PSL_STATIC int pslGetPreampPol(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *pol = (double *)value;

    DEFINE_CMD(CMD_GET_POLARITY, 2, 2);

    UNUSED(defs);

    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting polarity for detChan %d", detChan);
        pslLogError("pslGetPreampPol", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *pol = (double)receive[RECV_DATA_OFFSET_STATUS];

    return XIA_SUCCESS;
}


/*
 * Lets the hardware know what the preamplifier polarity is
 */
PSL_STATIC int pslSetPreampPol(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double pol = *((double *)value);

    DEFINE_CMD(CMD_SET_POLARITY, 2, 2);

    UNUSED(defs);

    UNUSED(name);

    ASSERT(value != NULL);


    if ((pol != 0.0) && (pol != 1.0)) {
        sprintf(info_string, "Polarity = %.3f is out-of-range. (Should be 0 or 1)",
                pol);
        pslLogError("pslSetPreampPol", info_string, XIA_POL_OOR);
        return XIA_POL_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)pol;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting polarity for detChan %d", detChan);
        pslLogError("pslSetPreampPol", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Apply changed acquisition values to the board
 *  This function now does nothing for udxp
 */
PSL_STATIC int pslApply(int detChan, char *name, XiaDefaults *defs, void *value)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);

    return XIA_SUCCESS;
}


/*
 * Retrieve the current GENSET value
 */
PSL_STATIC int pslGetGenset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *genset = (double *)value;

    DEFINE_CMD(CMD_GET_GENSET, 2, 2);


    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting genset for detChan %d",
                detChan);
        pslLogError("pslGetGenset", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *genset = (double)receive[5];

    sprintf(info_string, "genset = %.3f", *genset);
    pslLogDebug("pslGetGenset", info_string);

    return XIA_SUCCESS;
}


/*
 * Find the acquisition value with the specified name
 *  the given name could have additional parameters appended to it
 *  e.g. scalo_0 can match "sca"
 */
PSL_STATIC Udxp_AcquisitionValue *pslFindAV(char *name)
{
    int i;

    ASSERT(name != NULL);

    for (i = 0; i < NUM_ACQ_VALS; i++) {

        if (STRNEQ(name, acqVals[i].name)) {
            return &acqVals[i];
        }
    }

    return NULL;
}


/*
 * Gets the gap time based on the current SLOWGAP setting
 */
PSL_STATIC int pslGetEGapTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SLOWGAP    = 0;

    double clkTick = 0.0;

    double *gap = (double *)value;

    UNUSED(name);

    status = pslGetParameter(detChan, "SLOWGAP", &SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting SLOWGAP for detChan %d", detChan);
        pslLogError("pslGetEGapTime", info_string, status);
        return status;
    }

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetEGapTime", info_string, status);
        return status;
    }

    *gap = (double)SLOWGAP * clkTick;

    return XIA_SUCCESS;
}


/*
 * Sets filter parameter n to value in arbitrary DSP units. This only
 * performs the IO, not validation. n should come from the FILTER_*
 * definitions, e.g. FILTER_SLOWGAP. value should already be
 * validated/coerced within its valid range.
 */
PSL_STATIC int pslSetFilterParam(int detChan, byte_t n, parameter_t value)
{
    int status;

    DEFINE_CMD(CMD_SET_FILTER_PARAMS, 4, 4);
    boolean_t isSuper = dxp_is_supermicro(detChan);
    if (!isSuper) {
        OLD_MICRO_CMD(3, 3);
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)n;
    send[2] = (byte_t)value;
    if (isSuper) {
        send[3] = (byte_t)((int)value >> 8);
    }

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error setting a filter parameter for detChan %d", detChan);
        pslLogError("pslSetFilterParam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets filter parameter number n from the hardware. value is returned in
 * arbitrary DSP units. n should come from the FILTER_* definitions, e.g.
 * FILTER_SLOWGAP.
 */
PSL_STATIC int pslGetFilterParam(int detChan, byte_t n, parameter_t *value)
{
    int status;

    DEFINE_CMD(CMD_GET_FILTER_PARAMS, 3, 4);

    boolean_t isSuper = dxp_is_supermicro(detChan);

    ASSERT(value != NULL);

    if (!isSuper) {
        OLD_MICRO_CMD(3, 3);
    }

    send[0] = (byte_t)1;
    send[1] = (byte_t)n;

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting filter parameter %uh for detChan %d", n, detChan);
        pslLogError("pslGetFilterParam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (isSuper) {
        *value = (parameter_t)pslDoubleFromBytesOffset(receive, 2,
                                                       RECV_DATA_OFFSET_STATUS + 1);
    } else {
        *value = (parameter_t)receive[RECV_DATA_OFFSET_STATUS + 1];
    }

    return XIA_SUCCESS;
}


/*
 * Sets SLOWGAP based on the specified energy gap time
 */
PSL_STATIC int pslSetEGapTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SLOWGAP    = 0;
    parameter_t SLOWLEN    = 0;
    parameter_t BFACTOR    = 0;

    parameter_t MIN_SLOWGAP = 0;
    parameter_t MAX_SLOWGAP = 0;

    double psoffset = 0.0;
    double pioffset = 0.0;

    double clkTick = 0.0;

    double *gap = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(name);

    ASSERT(defs  != NULL);
    ASSERT(value != NULL);

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting SLOWLEN for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    SLOWGAP = (parameter_t)ROUND(*gap / clkTick);

    /* Check SLOWGAP limits */
    MIN_SLOWGAP = isSuper ? 0 : 3;
    MAX_SLOWGAP = isSuper ? MAX_FILTER_PARAM(isSuper) - SLOWLEN : 29;

    if (SLOWGAP < MIN_SLOWGAP) {
        sprintf(info_string, "Resetting SLOWGAP from %hu to the minimum allowed"
                " value %hu", SLOWGAP, MIN_SLOWGAP);
        pslLogInfo("pslSetEGapTime", info_string);
        SLOWGAP = MIN_SLOWGAP;
    }

    if (SLOWGAP > MAX_SLOWGAP) {
        sprintf(info_string, "Resetting SLOWGAP from %hu to the maximum allowed"
                " value %hu", SLOWGAP, MAX_SLOWGAP);
        pslLogInfo("pslSetEGapTime", info_string);
        SLOWGAP = MAX_SLOWGAP;
    }

    /* Supermicro requires SLOWGAP to be multiples of 2^(BFACTOR+1) */
    if (isSuper) {
        status = pslGetParameter(detChan, "BFACTOR", &BFACTOR);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error getting BFACTOR for detChan %d", detChan);
            pslLogError("pslSetEGapTime", info_string, status);
            return status;
        }

        sprintf(info_string, "Resetting SLOWGAP from %hu to be multiple of"
                " %d", SLOWGAP, (int)pow(2, BFACTOR + 1));
        pslLogInfo("pslSetEGapTime", info_string);

        SLOWGAP = SLOWGAP - (SLOWGAP % (int)pow(2, BFACTOR + 1));
    }

    status = pslSetFilterParam(detChan, FILTER_SLOWGAP, SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting SLOWGAP for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    *gap = (double)SLOWGAP * clkTick;

    /* Update PEAKINT and PEAKSAMP */
    status = pslGetAcquisitionValues(detChan, "peaksam_offset", (void *)&psoffset,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting peak sample offset for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    status = pslGetAcquisitionValues(detChan, "peakint_offset", (void *)&pioffset,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting peak interval offset for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    status = pslUpdateFilterParams(detChan, &pioffset, &psoffset, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error Updating filter parameters for detChan %d", detChan);
        pslLogError("pslSetEGapTime", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the digitizing clock speed on the board in MHz
 */
PSL_STATIC int pslSetClockSpd(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    byte_t clkIdx = 0xFF;

    double baseclock;

    double *clkSpd = (double *)value;

    DEFINE_CMD(CMD_SET_DIG_CLOCK, 2, 2);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    /* At the very least, we can verify that the clock index is
     * within the range 0-3. We let the board worry if the value
     * is an allowed setting for that specific board.
     */
    status = pslGetBaseClock(detChan, &baseclock);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting base clock for detChan %d", detChan);
        pslLogError("pslSetClockSpd", info_string, status);
        return status;
    }

    clkIdx = (byte_t)ROUND((log(baseclock / *clkSpd) / log(2.0)));

    /* Since clkIdx is unsigned, a value < 0 will simply be a real
     * big positive number.
     */
    if (clkIdx > 3) {
        sprintf(info_string,
                "The specified clock value of '%.3f' is not valid (idx = %u)",
                *clkSpd, clkIdx);
        pslLogError("pslSetClockSpd", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    send[0] = (byte_t)0;
    send[1] = clkIdx;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting the clock speed for detChan %d",
                detChan);
        pslLogError("pslSetClockSpd", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    INVALIDATE("pslSetClockSpd", "energy_gap_time");
    INVALIDATE("pslSetClockSpd", "trigger_peak_time");
    INVALIDATE("pslSetClockSpd", "trigger_gap_time");
    INVALIDATE("pslSetClockSpd", "peak_interval");

    return XIA_SUCCESS;
}


/*
 * Gets the number of peaking time for each FiPPI
 */
PSL_STATIC int pslGetNumPtPerFiPPI(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    unsigned short *nbrptPerFippi = (unsigned short *)value;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);
    *nbrptPerFippi = dxp_is_supermicro(detChan) ? 24 : 5;

    return XIA_SUCCESS;
}

/*
 * Number of bytes per peaking time is different for supermicro and regular
 */
PSL_STATIC int pslNumBytesPerPt(int detChan)
{
    return dxp_is_supermicro(detChan) ? 2 : 1;
}

/*
 * Gets the base DSP clock rate in MHz */
PSL_STATIC int pslGetBaseClock(int detChan, double *value)
{
    int status;

    parameter_t DSPSPEED = 0;
    boolean_t isSuper = dxp_is_supermicro(detChan);

    if (isSuper) {
        status = pslGetParameter(detChan, "DSPSPEED", &DSPSPEED);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error reading DSPSPEED for detChan %d", detChan);
            pslLogError("pslGetBaseClock", info_string, status);
            return status;
        }

        *value = (double)DSPSPEED;

    } else {
        *value = (double)BASE_CLOCK_STD;
    }

    return XIA_SUCCESS;
}

/*
 * Gets the clock tick of current channel
 */
PSL_STATIC int pslGetClockTick(int detChan, XiaDefaults *defs, double *value)
{
    int status;

    parameter_t DECIMATION = 0;
    double clkSpd = 0.0;

    double *clkTick = (double *)value;

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetClockTick", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "DECIMATION", &DECIMATION);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading DECIMATION for detChan %d", detChan);
        pslLogError("pslGetClockTick", info_string, status);
        return status;
    }

    *clkTick = pow(2.0, (double)DECIMATION) / clkSpd;

    return XIA_SUCCESS;
}

/*
 * Gets the digitizing clock speed from the board in MHz
 */
PSL_STATIC int pslGetClockSpd(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;
    int status;

    double clkIdx;
    double baseclock = 0;

    double *clkSpd = (double *)value;

    DEFINE_CMD(CMD_GET_DIG_CLOCK, 2, 2);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;
    send[1] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetClockSpd", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    clkIdx = (double)receive[RECV_DATA_OFFSET_STATUS];

    sprintf(info_string, "Clock setting = %.3lf", clkIdx);
    pslLogDebug("pslGetClockSpd", info_string);

    status = pslGetBaseClock(detChan, &baseclock);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting base clock for detChan %d", detChan);
        pslLogError("pslGetClockSpd", info_string, status);
        return status;
    }

    *clkSpd = baseclock / (pow(2.0, (double)receive[RECV_DATA_OFFSET_STATUS]));

    return XIA_SUCCESS;
}


/*
 * Gets the trigger peaking time from the hardware
 */
PSL_STATIC int pslGetTPeakTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t FASTLEN = 0;

    double clkSpd = 0.0;

    double *pt = (double *)value;

    UNUSED(name);

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetTPeakTime", info_string, status);
        return status;
    }

    status = pslGetFilterParam(detChan, FILTER_FASTLEN, &FASTLEN);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting FASTLEN for detChan %d", detChan);
        pslLogError("pslGetTPEakTime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *pt = (double)((double)FASTLEN / clkSpd);

    return XIA_SUCCESS;
}

/*Sets the trigger peaking time
 */
PSL_STATIC int pslSetTPeakTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t FASTLEN     = 0;
    parameter_t MAX_FASTLEN = 0;
    parameter_t FASTGAP     = 0;

    double clkSpd = 0.0;

    double *pt = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(name);

    ASSERT(defs  != NULL);
    ASSERT(value != NULL);


    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslSetTPeakTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "FASTGAP", &FASTGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting FASTGAP for detChan %d", detChan);
        pslLogError("pslSetTPeakTime", info_string, status);
        return status;
    }

    FASTLEN = (parameter_t)ROUND(*pt * clkSpd);

    sprintf(info_string, "FASTLEN = %u", FASTLEN);
    pslLogDebug("pslSetTPeakTime", info_string);

    if (FASTLEN < 2) {
        sprintf(info_string, "Calculated FASTLEN is too small. Setting to min value 2.");
        pslLogInfo("pslSetTPeakTime", info_string);

        FASTLEN = 2;
    }

    /* SuperMicro: FASTLEN + FASTGAP <= 255.
     * So we set a hard limit of 255 for FASTLEN and bump FASTGAP if needed.
     *
     * Previously Handel set a hard limit of 28.
     */
    MAX_FASTLEN = isSuper ? 255 : 28;
    if (FASTLEN > MAX_FASTLEN) {
        sprintf(info_string, "Calculated FASTLEN is too large. Setting to max value %hu.",
                MAX_FASTLEN);
        pslLogInfo("pslSetTPeakTime", info_string);

        FASTLEN = MAX_FASTLEN;
    }

    status = pslSetFilterParam(detChan, FILTER_FASTLEN, FASTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting FASTLEN for detChan %d", detChan);
        pslLogError("pslSetTPeakTime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (FASTLEN + FASTGAP > MAX_FASTLEN) {
        sprintf(info_string, "Updating FASTLEN made FASTGAP too large. "
                "Coercing FASTGAP = %hu.", FASTGAP);
        pslLogInfo("pslSetTPeakTime", info_string);

        FASTGAP = MAX_FASTLEN - FASTLEN;

        status = pslSetFilterParam(detChan, FILTER_FASTGAP, FASTGAP);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting FASTGAP for detChan %d", detChan);
            pslLogError("pslSetTPeakTime", info_string, XIA_XERXES);
            return XIA_XERXES;
        }

        INVALIDATE("pslSetTPeakTime", "trigger_gap_time");
    }

    /* The specified peaking time may be different then the actual, calculated
     * peaking time.
     */
    *pt = (double)((double)FASTLEN / clkSpd);

    return XIA_SUCCESS;
}


/*
 * Sets FASTGAP based on the specified trigger gap time
 */
PSL_STATIC int pslSetTGapTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    parameter_t FASTGAP = 0;
    parameter_t MAX_FASTGAP = 0;
    parameter_t FASTLEN = 0;

    double clkSpd = 0.0;

    double *gap = (double *)value;

    UNUSED(name);

    ASSERT(defs  != NULL);
    ASSERT(value != NULL);


    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslSetTGapTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "FASTLEN", &FASTLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting FASTLEN for detChan %d", detChan);
        pslLogError("pslSetTGapTime", info_string, status);
        return status;
    }

    FASTGAP = (parameter_t)ROUND(*gap * clkSpd);

    sprintf(info_string, "FASTGAP = %u", FASTGAP);
    pslLogDebug("pslSetTGapTime", info_string);

    /* 2 <= FASTLEN
     * 0 <= FASTGAP
     * FASTLEN + FASTGAP <= 255
     */
    MAX_FASTGAP = 255 - FASTLEN;

    if (FASTGAP > MAX_FASTGAP) {
        sprintf(info_string, "Calculated FASTGAP is too large with FASTLEN = %hu. "
                "Setting to max value %hu.",
                FASTLEN, MAX_FASTGAP);
        pslLogInfo("pslSetTGapTime", info_string);

        FASTGAP = MAX_FASTGAP;
    }

    statusX = pslSetFilterParam(detChan, FILTER_FASTGAP, FASTGAP);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting FASTGAP for detChan %d", detChan);
        pslLogError("pslSetTGapTime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *gap = (double)((double)FASTGAP / clkSpd);

    return XIA_SUCCESS;
}


/*
 * Gets the gap time based on the current FASTGAP setting
 */
PSL_STATIC int pslGetTGapTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t FASTGAP = 0;

    double clkSpd = 0.0;

    double *gap = (double *)value;

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    UNUSED(name);

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetTGapTime", info_string, status);
        return status;
    }

    status = pslGetFilterParam(detChan, FILTER_FASTGAP, &FASTGAP);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting FASTGAP for detChan %d", detChan);
        pslLogError("pslGetTGapTime", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *gap = (double)((double)FASTGAP / clkSpd);

    return XIA_SUCCESS;
}


/*
 * Sets the baseline length via BLFILTER
 */
PSL_STATIC int pslSetBaseLen(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    parameter_t BLFILTER = 0;

    double *baseLen = (double *)value;

    DEFINE_CMD(CMD_SET_BLFILTER, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    BLFILTER = (parameter_t)ROUND(32768.0 / *baseLen);

    sprintf(info_string, "New BLFILTER = %u (bl = %.3f)", BLFILTER, *baseLen);
    pslLogDebug("pslSetBaseLen", info_string);

    if (BLFILTER == 0) {
        sprintf(info_string, "Baseline length is 0 for detChan %d", detChan);
        pslLogError("pslSetBaseLen", info_string, XIA_BASELINE_OOR);
        return XIA_BASELINE_OOR;
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(BLFILTER);
    send[2] = (byte_t)HI_BYTE(BLFILTER);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting BLFILTER to %#x on detChan %d",
                BLFILTER, detChan);
        pslLogError("pslSetBaseLen", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Due to rounding, the calculated value can be different than the specified
     * value. Keep the rounded value for caching.
     */
    *baseLen = (double)(32768.0 / (double)BLFILTER);

    return XIA_SUCCESS;
}


/*
 * Gets the baseline filter length from BLFILTER
 */
PSL_STATIC int pslGetBaseLen(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    parameter_t BLFILTER = 0;

    double *baseLen = (double *)value;

    DEFINE_CMD(CMD_GET_BLFILTER, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting BLFILTER for detChan %d", detChan);
        pslLogError("pslGetBaseLen", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    BLFILTER = (unsigned short)(((unsigned short)receive[RECV_DATA_BASE + 1] << 8)
                                | receive[RECV_DATA_BASE]);

    sprintf(info_string, "BLFILTER = %u", BLFILTER);
    pslLogDebug("pslGetBaseLen", info_string);

    *baseLen = (double)(32768.0 / BLFILTER);

    return XIA_SUCCESS;
}


/*
 * Invalidate all acquisition values of a specific type
 */
PSL_STATIC int pslInvalidateAll(flag_t member, XiaDefaults *defs)
{
    int i;
    int status;


    for (i = 0; i < NUM_ACQ_VALS; i++) {

        if (member == AV_MEM_ALL || acqVals[i].member & member) {

            status = pslInvalidate(acqVals[i].name, defs);

            if (status != XIA_SUCCESS) {
                sprintf(info_string, "Error invalidating all members with flag %#x",
                        member);
                pslLogError("pslInvalidateAll", info_string, status);
                return status;
            }
        }
    }

    return XIA_SUCCESS;
}


/*
 * value should be a 2 element array of doubles. The value at index 0
 * is the PRESET type (see the RS-232 specification for a description of
 * valid values). Index 1 holds the PRESET length/time. This value is
 * ignored if the PRESET type is set to "indefinite". Times should be
 * specified in seconds.
 */
PSL_STATIC int pslSetPreset(int detChan, char *name, XiaDefaults *defs,
                            void *value)
{
    int status;

    double *data = (double *)value;

    UNUSED(name);

    ASSERT(value != NULL);

    pslLogWarning("pslBoardOperation", "The board operation set_preset is "
                  "deprecated and will be removed in future releaes, please use "
                  "acquisition values preset_type and preset_value instead.");

    status = pslSetPresetType(detChan, NULL, defs, (void *)&data[0]);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting preset_type for detChan %d", detChan);
        pslLogError("pslSetPreset", info_string, status);
        return status;
    }

    status = pslSetPresetValue(detChan, NULL, defs, (void *)&data[1]);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting preset_value for detChan %d", detChan);
        pslLogError("pslSetPreset", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}



/*
 * Retrieves the current ADC trace wait time
 *
 * This function will only be called if adc_trace_wait haven't been set
 * Whereas it will return the minimum allowable trace wait
 */
PSL_STATIC int pslGetADCWait(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double spd = 0.0;

    /* In microseconds */
    double *minTracewait = (double *)value;

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);


    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&spd, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetADCWait", info_string, status);
        return status;
    }

    *minTracewait = pslMinTraceWait(spd);

    sprintf(info_string, "tracewait = %.3f", *minTracewait);
    pslLogDebug("pslGetADCWait", info_string);

    return XIA_SUCCESS;
}


/*
 * Sets the ADC tracewait time adc_trace_wait
 *
 * Time should be specified in microseconds.
 */
PSL_STATIC int pslSetADCWait(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    /* In microseconds */
    double *tracewait = (double *)value;

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    status = pslCheckTraceWaitRange(detChan, tracewait, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error checking tracewait range for detChan %d", detChan);
        pslLogError("pslSetADCWait", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Reads out all of the statistics
 *
 * Unlike other DXP products, statisitics for the microDXP are returned as part
 * of a single routine. Therefore, reading out the individual statistics, in lieu
 * of a clever caching scheme, requires a seperate call to the same routine. This
 * is both resource consuming, in terms of I/O, and error-prone since a large
 * amount of skew is introduced in the statistics values. This routine fixes all
 * of these problems by returning all of the statisitics that the single microDXP
 * command returns. The returned values are:
 * value[0] = livetime
 * value[1] = realtime
 * value[2] = fastpeaks (input events)
 * value[3] = output events
 * value[4] = input count rate
 * value[5] = output count rate
 */
PSL_STATIC int pslGetAllStatistics(int detChan, void *value, XiaDefaults *defs)
{
    int statusX;

    double lt  = 0.0;
    double rt  = 0.0;
    double in  = 0.0;
    double out = 0.0;
    double icr = 0.0;
    double ocr = 0.0;

    double *stats = (double *)value;

    DEFINE_CMD_ZERO_SEND(CMD_READ_STATISTICS, 21);

    UNUSED(defs);

    ASSERT(value != NULL);

    pslLogWarning("pslGetAllStatistics", "The run data all_statistics is "
                  "deprecated, please use module_statistics_2 instead.");

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetAllStatistics", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    lt =  pslDoubleFromBytesOffset(receive, 6, 5) * LIVETIME_CLOCK_TICK;
    rt =  pslDoubleFromBytesOffset(receive, 6, 11) * REALTIME_CLOCK_TICK;
    in =  pslDoubleFromBytesOffset(receive, 4, 17);
    out =  pslDoubleFromBytesOffset(receive, 4, 21);

    icr = in / lt;
    ocr = out / rt;

    stats[0] = lt;
    stats[1] = rt;
    stats[2] = in;
    stats[3] = out;
    stats[4] = icr;
    stats[5] = ocr;

    return XIA_SUCCESS;
}

/*
 * Returns all of the statistics in a single array for run
 * data module_statistics_2 value is expected to be a double array capable
 * of holding 9 values returned in the following format:
 *
 * [runtime, trigger_livetime, energy_livetime, triggers, events, icr,
 * ocr, underflows, overflows]
 */
PSL_STATIC int pslGetModuleStatistics(int detChan, void *value, XiaDefaults *defs)
{

    int statusX;

    double lt  = 0.0;
    double rt  = 0.0;
    double in  = 0.0;
    double out = 0.0;
    double icr = 0.0;
    double ocr = 0.0;
    double unders = 0.0;
    double overs = 0.0;

    double *stats = (double *)value;

    DEFINE_CMD(CMD_READ_STATISTICS, 1, 29);
    boolean_t isSuper = dxp_is_supermicro(detChan);

    if (!isSuper) {
        OLD_MICRO_CMD(0, 21);
    }

    UNUSED(defs);

    ASSERT(value != NULL);

    /* long read mode to retrieve under and over flows */
    if (isSuper) {
        send[0] = (byte_t)1;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading statistics for detChan %d", detChan);
        pslLogError("pslGetModuleStatistics", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    lt =  pslDoubleFromBytesOffset(receive, 6, 5) * LIVETIME_CLOCK_TICK;
    rt =  pslDoubleFromBytesOffset(receive, 6, 11) * REALTIME_CLOCK_TICK;
    in =  pslDoubleFromBytesOffset(receive, 4, 17);
    out =  pslDoubleFromBytesOffset(receive, 4, 21);

    if (isSuper) {
        unders =  pslDoubleFromBytesOffset(receive, 4, 25);
        overs =  pslDoubleFromBytesOffset(receive, 4, 29);
    }

    if (rt > 0.0) {
        ocr = (out + unders + overs) / rt;
    } else {
        ocr = 0.0;
    }

    if (lt > 0.0) {
        icr = in / lt;
    } else {
        icr = 0.0;
    }

    stats[0] = rt;
    stats[1] = lt;
    /* microDXP doesn't support energy_livetime */
    stats[2] = 0.0;
    stats[3] = in;
    stats[4] = out;
    stats[5] = icr;
    stats[6] = ocr;
    stats[7] = unders;
    stats[8] = overs;

    return XIA_SUCCESS;
}

/*
 * Determines the preamplifier type based on the firmware loaded on
 * the hardware
 */
PSL_STATIC int pslGetPreampType(int detChan, char *name, XiaDefaults *defs,
                                void *value)
{
    int status;

    unsigned short *type = (unsigned short *)value;

    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    status = pslQueryPreampType(detChan, type);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting preamplifier type for detChan %d",
                detChan);
        pslLogError("pslGetPreampType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Querys the firmware to determine the preamplifier type
 *
 * 0 -> Reset
 * 1 -> RC Feedback
 */
PSL_STATIC int pslQueryPreampType(int detChan, unsigned short *type)
{
    int status;

    parameter_t CODEVAR = 0;


    ASSERT(type != NULL);


    status = pslGetParameter(detChan, "CODEVAR", &CODEVAR);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting CODEVAR for detChan %d", detChan);
        pslLogError("pslQueryPreampType", info_string, status);
        return status;
    }

    *type = (unsigned short)(CODEVAR & 0x1);

    return XIA_SUCCESS;
}


/*
 * Get the current setting for FIPCONTROL
 */
PSL_STATIC int pslGetFipControl(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *fipcontrol = (double *)value;

    DEFINE_CMD(CMD_GET_FIPCONTROL, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading FIPCONTROL for detChan %d", detChan);
        pslLogError("pslGetFipControl", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *fipcontrol = (double)(((unsigned short)receive[RECV_DATA_BASE + 1] << 8) |
                           receive[RECV_DATA_BASE]);

    sprintf(info_string, "lo = %#x, hi = %#x, fipcontrol = %.3f",
            receive[RECV_DATA_BASE], receive[RECV_DATA_BASE + 1], *fipcontrol);
    pslLogDebug("pslGetFipControl", info_string);

    return XIA_SUCCESS;
}


/*
 * Set FIPCONTROL
 */
PSL_STATIC int pslSetFipControl(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    unsigned short fipcontrol = (unsigned short)(*((double *)value));

    DEFINE_CMD(CMD_SET_FIPCONTROL, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(fipcontrol);
    send[2] = (byte_t)HI_BYTE(fipcontrol);

    sprintf(info_string, "Setting FIPCONTROL to %#x", fipcontrol);
    pslLogDebug("pslSetFipControl", info_string);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting FIPCONTROL to %#x on detChan %d",
                fipcontrol, detChan);
        pslLogError("pslSetFipControl", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the current value for RUNTASKS from the hardware
 */
PSL_STATIC int pslGetRuntasks(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    double *runtasks = (double *)value;

    DEFINE_CMD(CMD_GET_RUNTASKS, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading RUNTASKS for detChan %d", detChan);
        pslLogError("pslGetRuntasks", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *runtasks = (double)(((unsigned short)receive[RECV_DATA_BASE + 1] << 8) |
                         receive[RECV_DATA_BASE]);

    return XIA_SUCCESS;
}


/*
 * Set the current value for RUNTASKS on the hardware
 */
PSL_STATIC int pslSetRuntasks(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    unsigned short runtasks = (unsigned short)(*((double *)value));

    DEFINE_CMD(CMD_SET_RUNTASKS, 3, 3);

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(runtasks);
    send[2] = (byte_t)HI_BYTE(runtasks);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting RUNTASKS to %#x on detChan %d",
                runtasks, detChan);
        pslLogError("pslSetRuntasks", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the fine gain trim setting
 *
 * The gain trim is only allowed to modify GAINBASE by a factor of 2 in either
 * direction. There are several levels to this check: 1) Check the linear
 * gain trim value passed in via 'value' and 2) Check that the result of
 * (gain trim) * (gain base) > 1.0 and < 100.0.
 */
PSL_STATIC int pslSetGainTrim(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double gDB      = 0.0;
    double gainbase = 0.0;
    double gain     = 0.0;

    unsigned short gainMode;

    int gaintweakval = 0;
    parameter_t GAINTWEAK = 0;

    double *gaintrim = (double *)value;

    DEFINE_CMD(CMD_SET_GAINTWEAK, 3, 3);

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    status = pslGetGainMode(detChan, NULL, defs, (void *)&gainMode);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting gain mode for detchan %d.",
                detChan);
        pslLogError("pslSetGainTrim", info_string, status);
        return status;
    }

    if (*gaintrim > GAIN_TRIM_LINEAR_MAX) {
        sprintf(info_string,
                "Gain trim of %.3f is larger then the max trim (%.3f) "
                "for detChan %d",
                *gaintrim, GAIN_TRIM_LINEAR_MAX, detChan);
        pslLogError("pslSetGainTrim", info_string, XIA_GAIN_TRIM_OOR);
        return XIA_GAIN_TRIM_OOR;
    }

    if (*gaintrim < GAIN_TRIM_LINEAR_MIN) {
        sprintf(info_string,
                "Gain trim of %.3f is smaller then the min trim (%.3f) "
                "for detChan %d",
                *gaintrim, GAIN_TRIM_LINEAR_MIN, detChan);
        pslLogError("pslSetGainTrim", info_string, XIA_GAIN_TRIM_OOR);
        return XIA_GAIN_TRIM_OOR;
    }

    status = pslGetAcquisitionValues(detChan, "gain", (void *)&gainbase, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting base gain value for detChan %d",
                detChan);
        pslLogError("pslSetGainTrim", info_string, status);
        return status;
    }

    gain = (*gaintrim) * gainbase;

    sprintf(info_string, "gain = %.3f, gainbase = %.3f", gain, gainbase);
    pslLogDebug("pslSetGainTrim", info_string);

    if (gain > GAIN_LINEAR_MAX) {
        sprintf(info_string,
                "Total calculated gain (%.3f) is too large. Reduce the current "
                "value of the gain trim (%.3f) or adjust the base gain value "
                "(%.3f)",
                gain, *gaintrim, gainbase);
        pslLogError("pslSetGainTrim", info_string, XIA_GAIN_OOR);
        return XIA_GAIN_OOR;
    }

    if (gain < GAIN_LINEAR_MIN) {
        sprintf(info_string,
                "Total calculated gain (%.3f) is too small. Increase the current "
                "value of the gain trim (%.3f) or adjust the base gain value "
                "(%.3f)",
                gain, *gaintrim, gainbase);
        pslLogError("pslSetGainTrim", info_string, XIA_GAIN_OOR);
        return XIA_GAIN_OOR;
    }

    /* At this point, it is safe to continue the calculation since the gain
     * time is within acceptable limits.
     */
    if (gainMode < GAIN_MODE_DIGITAL) {
        gDB       = 20.0 * log10(*gaintrim);
        GAINTWEAK = (parameter_t)(short)ROUND(gDB / DB_PER_LSB);
    } else {
        gaintweakval = (int)(32768.0 * (*gaintrim));
        /* max allowed fine gain trim is slightly smaller than 2.0 */
        if (gaintweakval > GAINTWEAK_MAX) {
            gaintweakval = GAINTWEAK_MAX;
            sprintf(info_string, "Calculated GAINTWEAK (%d) is greater than maximum"
                    " value allowed, resetting to max (%d).",
                    gaintweakval, GAINTWEAK_MAX);
            pslLogWarning("pslSetGainTrim", info_string);
        }
        GAINTWEAK = (parameter_t)gaintweakval;
    }

    sprintf(info_string, "gaintrim = %.3f, gDB = %.3f, GAINTWEAK = %#x", *gaintrim,
            gDB, GAINTWEAK);
    pslLogDebug("pslSetGainTrim", info_string);

    send[0] = (byte_t)0;
    send[1] = (byte_t)LO_BYTE(GAINTWEAK);
    send[2] = (byte_t)HI_BYTE(GAINTWEAK);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting GAINTWEAK to '%#x' on detChan %d",
                GAINTWEAK, detChan);
        pslLogError("pslSetGainTrim", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Due to rounding, the calculated value can be different than the specified
     * value. Keep the rounded value for caching.
     */
    if (gainMode < GAIN_MODE_DIGITAL) {
        *gaintrim = (double)pow(10, (double)GAINTWEAK * DB_PER_LSB / 20.0);
    } else {
        *gaintrim = (double)((double)GAINTWEAK / 32768.0);
    }

    return XIA_SUCCESS;
}


/*
 * Get the fine gain trim setting
 */
PSL_STATIC int pslGetGainTrim(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    unsigned short gainMode;
    double genset;

    double gDB = 0.0;

    char gaintweakname[MAX_DSP_PARAM_NAME_LEN] = "";
    parameter_t GAINTWEAK = 0;

    double *gaintrim = (double *)value;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetGainMode(detChan, NULL, defs, (void *)&gainMode);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetGainTrim", "Error getting gain mode.", status);
        return status;
    }

    status = pslGetGenset(detChan, NULL, defs, (void *)&genset);

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetGainTrim", "Error getting genset.", status);
        return status;
    }

    sprintf(gaintweakname, "GAINTWEAK%d", (int)genset);
    status = pslGetParameter(detChan, gaintweakname, &GAINTWEAK);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading GAINTWEAK for detChan %d", detChan);
        pslLogError("pslGetGainTrim", info_string, status);
        return status;
    }

    if (gainMode < GAIN_MODE_DIGITAL) {

        gDB = (double)(DB_PER_LSB * (double)GAINTWEAK);

        /* If the sign bit is set on GAINTWEAK, we need to be sure to interpret it
         * as a negative number.
         */
        if (GAINTWEAK & 0x1000) {
            gDB = gDB - 40.0;
        }

        *gaintrim = pow(10.0, (gDB / 20.0));

    } else {

        *gaintrim = (double)GAINTWEAK / 32768.0;
    }

    sprintf(info_string, "%s = %#x, gDB = %.3f, gaintrim = %.3f",
            gaintweakname, GAINTWEAK, gDB, *gaintrim);
    pslLogDebug("pslGetGainTrim", info_string);

    return XIA_SUCCESS;
}


/*
 * Currently a stub for some TBD hardware recovery procedures
 */
PSL_STATIC int pslRecover(int detChan, char *name, XiaDefaults *defs,
                          void *value)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    FAIL();


    return XIA_UNKNOWN;
}


/*
 * Sets the value of PEAKINT and PEAKSAM based on acquisition values
 *  peakint_offset and peaksam_offset.
 *  PEAKINT = SLOWLEN + SLOWGAP + peak_interval_offset/ClockTick
 *  PEAKSAM = SLOWLEN + SLOWGAP - peak_sample_offset/ClockTick
 */
PSL_STATIC int pslUpdateFilterParams(int detChan, double *pioffset,
                                     double *psoffset, XiaDefaults *defs)
{
    int status;

    parameter_t PEAKINT = 0;
    parameter_t SLOWLEN = 0;
    parameter_t SLOWGAP = 0;
    parameter_t PEAKSAM = 0;
    double peaksam = 0.0;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    double clkTick = 0.0;

    ASSERT(defs != NULL);


    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslUpdateFilterParams", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWLEN for detChan %d", detChan);
        pslLogError("pslUpdateFilterParams", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWGAP", &SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWGAP for detChan %d", detChan);
        pslLogError("pslUpdateFilterParams", info_string, status);
        return status;
    }

    PEAKINT = SLOWLEN + SLOWGAP + (parameter_t)ROUND(*pioffset / clkTick);

    sprintf(info_string, "PEAKINT = %u", PEAKINT);
    pslLogDebug("pslUpdateFilterParams", info_string);

    /* The only limit we enforce is a software limit where the value would exceed
     * the max parameter size.
     */
    if (PEAKINT > MAX_FILTER_TIMER(isSuper)) {
        sprintf(info_string, "Calculated PEAKINT %hu is out of range (%hu, %hu)"
                " for detChan %d, reset to maximum value", PEAKINT, 0,
                MAX_FILTER_TIMER(isSuper), detChan);
        pslLogWarning("pslUpdateFilterParams", info_string);

        PEAKINT = MAX_FILTER_TIMER(isSuper);
        INVALIDATE("pslUpdateFilterParams", "peakint_offset");
    }

    status = pslSetFilterParam(detChan, FILTER_PEAKINT, PEAKINT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PEAKINT for detChan %d", detChan);
        pslLogError("pslUpdateFilterParams", info_string, status);
        return status;
    }

    *pioffset = (double)(PEAKINT - SLOWLEN - SLOWGAP) * clkTick;

    /* peak sampling offset constraints:
     * - PEAKSAM = SLOWLEN + SLOWGAP - peaksam_offset/ClockTick
     * - 0 <= PEAKSAM <= PEAKINT <= 4095
     * - peaksam_offset can be negative, but only as far as peakint_offset
     * - peak_sample_offset <= peaking_time + energy_gap_time
     */
    peaksam = (SLOWLEN + SLOWGAP) * clkTick - *psoffset;

    if (peaksam < 0.0) {
        sprintf(info_string, "peaksam_offset %0.3f is out of range negative for "
                "SLOWLEN + SLOWGAP = %hu. Setting PEAKSAM=0.",
                *psoffset, SLOWLEN + SLOWGAP);
        pslLogWarning("pslUpdateFilterParams", info_string);

        PEAKSAM = 0;
        INVALIDATE("pslUpdateFilterParams", "peaksam_offset");
    } else {
        PEAKSAM = (parameter_t)ROUND(peaksam / clkTick);

        sprintf(info_string, "Calculated PEAKSAM = %u", PEAKSAM);
        pslLogDebug("pslUpdateFilterParams", info_string);

        if (PEAKSAM > PEAKINT) {
            sprintf(info_string, "PEAKSAM %hu is out of range for PEAKINT = %hu. "
                    "Setting PEAKSAM = PEAKINT.", PEAKSAM, PEAKINT);
            pslLogWarning("pslUpdateFilterParams", info_string);

            PEAKSAM = PEAKINT;
            INVALIDATE("pslUpdateFilterParams", "peaksam_offset");
        }
    }

    status = pslSetFilterParam(detChan, FILTER_PEAKSAM, PEAKSAM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PEAKSAM for detChan %d", detChan);
        pslLogError("pslUpdateFilterParams", info_string, status);
        return status;
    }

    *psoffset = (double)(SLOWLEN + SLOWGAP - PEAKSAM) * clkTick;

    /* Force the acquisition values updates to get the updated value */
    INVALIDATE("pslUpdateFilterParams", "peak_sample");
    INVALIDATE("pslUpdateFilterParams", "peak_interval");

    return XIA_SUCCESS;
}


/*
 * Sets the value of peak interval offset, specified in microseconds
 *  This will in effect set PEAKINT for the current decimation
 */
PSL_STATIC int pslSetPeakIntOffset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double psoffset = 0.0;
    double *pioffset = (double *)value;

    UNUSED(name);

    status = pslGetAcquisitionValues(detChan, "peaksam_offset", (void *)&psoffset,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting peak interval offset for detChan %d", detChan);
        pslLogError("pslSetPeakIntOffset", info_string, status);
        return status;
    }

    status = pslUpdateFilterParams(detChan, pioffset, &psoffset, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error Updating filter parameters for detChan %d", detChan);
        pslLogError("pslSetPeakIntOffset", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the current value of peak interval offset in microseconds
 *  Calculated as peak_interval_offset = (PEAKINT - SLOWGAP - SLOWLEN) * ClockTick
 */
PSL_STATIC int pslGetPeakIntOffset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKINT = 0;
    parameter_t SLOWLEN = 0;
    parameter_t SLOWGAP = 0;

    double *pioffset = (double *)value;
    double clkTick = 0.0;

    UNUSED(name);

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetPeakIntOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWLEN for detChan %d", detChan);
        pslLogError("pslGetPeakIntOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWGAP", &SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWGAP for detChan %d", detChan);
        pslLogError("pslGetPeakIntOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "PEAKINT", &PEAKINT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading PEAKINT for detChan %d", detChan);
        pslLogError("pslGetPeakIntOffset", info_string, status);
        return status;
    }

    *pioffset = (double)(PEAKINT - SLOWLEN - SLOWGAP) * clkTick;

    return XIA_SUCCESS;
}


/*
 * Sets the value of peak sample offset, specified in microseconds
 *  This will in effect set PEAKSAM
 */
PSL_STATIC int pslSetPeakSamOffset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double *psoffset = (double *)value;
    double pioffset = 0.0;

    UNUSED(name);

    status = pslGetAcquisitionValues(detChan, "peakint_offset", (void *)&pioffset,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting peak interval offset for detChan %d", detChan);
        pslLogError("pslSetPeakSamOffset", info_string, status);
        return status;
    }

    status = pslUpdateFilterParams(detChan, &pioffset, psoffset, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error Updating filter parameters for detChan %d", detChan);
        pslLogError("pslSetPeakSamOffset", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the current value of peak sample offset in microseconds
 *  Calculated as peak_sample_offset = (SLOWLEN + SLOWGAP - PEAKSAM) * ClockTick
 */
PSL_STATIC int pslGetPeakSamOffset(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKSAM = 0;
    parameter_t SLOWLEN = 0;
    parameter_t SLOWGAP = 0;

    double *psoffset = (double *)value;
    double clkTick = 0.0;

    UNUSED(name);

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetPeakSamOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWLEN for detChan %d", detChan);
        pslLogError("pslGetPeakSamOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWGAP", &SLOWGAP);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWGAP for detChan %d", detChan);
        pslLogError("pslGetPeakSamOffset", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "PEAKSAM", &PEAKSAM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading PEAKSAM for detChan %d", detChan);
        pslLogError("pslGetPeakSamOffset", info_string, status);
        return status;
    }

    *psoffset = (double)(SLOWLEN + SLOWGAP - PEAKSAM) * clkTick;

    return XIA_SUCCESS;
}


/*
 * Sets the value of PEAKINT, specified in microseconds
 */
PSL_STATIC int pslSetPeakInt(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKINT = 0;

    double clkTick = 0.0;
    double *pi = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    pslLogWarning("pslSetPeakInt", "The acquisition value peak_interval is "
                  "DEPRECATED, please use peakint_offset instead.");

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslSetPeakInt", info_string, status);
        return status;
    }

    PEAKINT = (parameter_t)ROUND(*pi / clkTick);

    /* The only limit we enforce is a software limit where the value would exceed
     * the max parameter size.
     * Technically we could enforce a minimum of 4 since SLOWLEN is >= 4 and
     * PEAKINT = SLOWLEN + SLOWGAP + offset.
     */
    if (PEAKINT > MAX_FILTER_TIMER(isSuper)) {
        sprintf(info_string, "Requested peak interval (%.3lf microseconds) is too "
                "large for detChan %d", *pi, detChan);
        pslLogError("pslSetPeakInt", info_string, XIA_PEAKINT_OOR);
        return XIA_PEAKINT_OOR;
    }

    status = pslSetFilterParam(detChan, FILTER_PEAKINT, PEAKINT);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PEAKINT for detChan %d", detChan);
        pslLogError("pslSetPeakInt", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Due to rounding, the calculated value can be different than the specified
     * value. Keep the rounded value for caching.
     */
    *pi = (double)PEAKINT * clkTick;

    INVALIDATE("pslSetPeakInt", "peakint_offset");
    return XIA_SUCCESS;
}


/*
 * Gets the current value of PEAKINT in microseconds
 */
PSL_STATIC int pslGetPeakInt(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKINT    = 0;

    double clkTick = 0.0;

    double *pi = (double *)value;

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    pslLogWarning("pslGetPeakInt", "The acquisition value peak_interval is "
                  "DEPRECATED, please use peakint_offset instead.");

    status = pslGetFilterParam(detChan, FILTER_PEAKINT, &PEAKINT);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting PEAKINT for detChan %d", detChan);
        pslLogError("pslGetPeakInt", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetPeakInt", info_string, status);
        return status;
    }

    *pi = (double)PEAKINT * clkTick;

    return XIA_SUCCESS;
}


/*
 * Sets the value of PEAKSAM, specified in microseconds
 */
PSL_STATIC int pslSetPeakSam(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKSAM = 0;

    double clkTick = 0.0;

    double *ps = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    pslLogWarning("pslSetPeakSam", "The acquisition value peak_sample is "
                  "deprecated, please use peaksam_offset instead.");

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslSetPeakSam", info_string, status);
        return status;
    }

    PEAKSAM = (parameter_t)ROUND(*ps / clkTick);

    /* 4 <= PEAKINT <= MAX_FILTER_TIMER
     * 0 <= PEAKSAM <= PEAKINT
     */
    if (PEAKSAM > MAX_FILTER_TIMER(isSuper)) {
        sprintf(info_string, "Requested peak sample time (%.3lf microseconds) is "
                "too large for detChan %d", *ps, detChan);
        pslLogError("pslSetPeakSam", info_string, XIA_PEAKSAM_OOR);
        return XIA_PEAKSAM_OOR;
    }

    status = pslSetFilterParam(detChan, FILTER_PEAKSAM, PEAKSAM);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PEAKSAM for detChan %d", detChan);
        pslLogError("pslSetPeakSam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Due to rounding, the calculated value can be different than the specified
     * value. Keep the rounded value for caching.
     */
    *ps = (double)PEAKSAM * clkTick;

    INVALIDATE("pslSetPeakSam", "peaksam_offset");
    return XIA_SUCCESS;
}


/*
 * Gets the current value of PEAKSAM in microseconds
 */
PSL_STATIC int pslGetPeakSam(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t PEAKSAM    = 0;

    double clkTick = 0.0;

    double *ps = (double *)value;

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    pslLogWarning("pslGetPeakSam", "The acquisition value peak_sample is "
                  "deprecated, please use peaksam_offset instead.");

    status = pslGetFilterParam(detChan, FILTER_PEAKSAM, &PEAKSAM);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting PEAKSAM for detChan %d", detChan);
        pslLogError("pslGetPeakSam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetPeakSam", info_string, status);
        return status;
    }

    *ps = (double)PEAKSAM * clkTick;

    return XIA_SUCCESS;
}


/*
 * Set the value of MAXWIDTH, specified in microseconds
 */
PSL_STATIC int pslSetMaxWidth(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t MAXWIDTH = 0;
    parameter_t MAX_MAXWIDTH;

    double clkSpd = 0.0;

    double *mw = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);


    MAX_MAXWIDTH = MAX_FILTER_PARAM(isSuper);

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslSetMaxWidth", info_string, status);
        return status;
    }

    MAXWIDTH = (parameter_t)ROUND((*mw) * clkSpd);

    sprintf(info_string, "MAXWIDTH = %u", MAXWIDTH);
    pslLogDebug("pslSetMaxWidth", info_string);

    if (MAXWIDTH > MAX_MAXWIDTH) {
        sprintf(info_string, "Requested max width time (%.3lf microseconds) is "
                "too large. Coercing to %.3lf microseconds.",
                *mw, (double)MAX_MAXWIDTH / clkSpd);
        pslLogWarning("pslSetMaxWidth", info_string);

        MAXWIDTH = MAX_MAXWIDTH;
    }

    status = pslSetFilterParam(detChan, FILTER_MAXWIDTH, MAXWIDTH);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting MAXWIDTH for detChan %d", detChan);
        pslLogError("pslSetMaxWidth", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Due to rounding, the calculated value can be different than the specified
     * value. Keep the rounded value for caching.
    */
    *mw = (double)((double)MAXWIDTH / clkSpd);

    return XIA_SUCCESS;
}


/*
 * Get the value of MAXWIDTH in microseconds
 */
PSL_STATIC int pslGetMaxWidth(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t MAXWIDTH    = 0;

    double clkSpd = 0.0;

    double *mw = (double *)value;

    UNUSED(name);

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    status = pslGetFilterParam(detChan, FILTER_MAXWIDTH, &MAXWIDTH);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting MAXWIDTH for detChan %d", detChan);
        pslLogError("pslGetMaxWidth", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    status = pslGetAcquisitionValues(detChan, "clock_speed", (void *)&clkSpd,
                                     defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock speed for detChan %d", detChan);
        pslLogError("pslGetMaxWidth", info_string, status);
        return status;
    }

    *mw = (double)((double)MAXWIDTH / clkSpd);

    return XIA_SUCCESS;
}


/*
 * Sets the value of PEAKMODE,
 * "Peak-Sensing" (PEAKMODE=0) or "Peak-Sampling" (PEAKMODE=1)
 */
PSL_STATIC int pslSetPeakMode(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    parameter_t PEAKMODE = (parameter_t)*((double *)value);

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value peak_mode is not supported by "
                "non-supermicro variant");
        pslLogError("pslSetPeakMode", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    if ((PEAKMODE != XIA_PEAK_SENSING_MODE) &&
            (PEAKMODE != XIA_PEAK_SAMPLING_MODE)) {
        sprintf(info_string, "User specified peak mode %hu is not within the "
                "valid range (0,1) for detChan %d", PEAKMODE, detChan);
        pslLogError("pslSetPeakMode", info_string, XIA_PEAKMODE_OOR);
        return XIA_PEAKMODE_OOR;
    }

    ASSERT(value != NULL);

    statusX = pslSetFilterParam(detChan, FILTER_PEAKMODE, PEAKMODE);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting PEAKMODE for detChan %d", detChan);
        pslLogError("pslSetPeakMode", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the current value of PEAKMODE
 */
PSL_STATIC int pslGetPeakMode(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    parameter_t PEAKMODE = 0;

    double *pm = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value peak_mode is not supported by "
                "non-supermicro variant");
        pslLogError("pslGetPeakMode", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    ASSERT(value != NULL);

    statusX = pslGetFilterParam(detChan, FILTER_PEAKMODE, &PEAKMODE);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting PEAKMODE for detChan %d", detChan);
        pslLogError("pslGetPeakMode", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *pm = (double)PEAKMODE;

    return XIA_SUCCESS;
}


/*
 * Setting of BFACTOR is not supported yet
 */
PSL_STATIC int pslSetBFactor(int detChan, char *name, XiaDefaults *defs, void *value)
{

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(value);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value baseline_factor is not supported "
                "by non-supermicro variant");
        pslLogError("pslSetBFactor", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    pslLogError("pslSetBFactor", "Setting of acquisition value baseline_factor "
                "is not supported.", XIA_NOSUPPORT_VALUE);
    return XIA_NOSUPPORT_VALUE;
}


/*
 * Gets the current value of BFACTOR
 */
PSL_STATIC int pslGetBFactor(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int statusX;

    parameter_t BFACTOR = 0;

    double *bf = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value baseline_factor is not supported "
                "by non-supermicro variant");
        pslLogError("pslGetBFactor", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    ASSERT(value != NULL);

    statusX = pslGetFilterParam(detChan, FILTER_BFACTOR, &BFACTOR);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting BFACTOR for detChan %d", detChan);
        pslLogError("pslGetBFactor", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *bf = (double)BFACTOR;

    return XIA_SUCCESS;
}


/*
 * Get the peaking_time by converting PARSET value to corresponding
 *  entry in
 */
PSL_STATIC int pslGetPeakingTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    double *pt = (double *)value;

    double clkTick;

    parameter_t SLOWLEN;

    UNUSED(defs);
    UNUSED(name);

    status = pslGetClockTick(detChan, defs, &clkTick);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting clock tick for detChan %d", detChan);
        pslLogError("pslGetPeakingTime", info_string, status);
        return status;
    }

    status = pslGetParameter(detChan, "SLOWLEN", &SLOWLEN);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SLOWLEN for detChan %d", detChan);
        pslLogError("pslGetPeakingTime", info_string, status);
        return status;
    }

    *pt = (double)SLOWLEN * clkTick;

    return XIA_SUCCESS;
}


/*
 * Setting of peaking_time is not supported and must be done by
 *  setting parset.
 */
PSL_STATIC int pslSetPeakingTime(int detChan, char *name, XiaDefaults *defs, void *value)
{
    UNUSED(detChan);
    UNUSED(defs);
    UNUSED(value);
    UNUSED(name);

    pslLogError("pslSetPeakingTime", "Setting of acquisition value peaking_time "
                "is not supported and must be done by setting parset.", XIA_NOSUPPORT_VALUE);

    return XIA_NOSUPPORT_VALUE;
}


PSL_STATIC int pslSetTriggerType(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t TRACETRIG;
    double trigType = *((double *)value);

    UNUSED(defs);
    UNUSED(name);

    if (trigType < 0 || trigType > 255.0) {
        sprintf(info_string, "Trace trigger type %0f is out-of-range",
                trigType);
        pslLogError("pslSetTriggerType", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRACETRIG = (parameter_t)trigType;

    status = pslSetParameter(detChan, "TRACETRIG", TRACETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting TRACETRIG for detChan %d", detChan);
        pslLogError("pslSetTriggerType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int pslSetTriggerPosition(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t TRACEPRETRIG;
    double trigPosition = *((double *)value);

    UNUSED(defs);
    UNUSED(name);

    if (trigPosition < 0 || trigPosition > 255.0) {
        sprintf(info_string, "Trace trigger position %0f is out-of-range",
                trigPosition);
        pslLogError("pslSetTriggerPosition", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    TRACEPRETRIG = (parameter_t)trigPosition;

    status = pslSetParameter(detChan, "TRACEPRETRIG", TRACEPRETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting TRACEPRETRIG for detChan %d", detChan);
        pslLogError("pslSetTriggerPosition", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int pslGetTriggerType(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t TRACETRIG;
    double *tracetrig = (double *)value;

    UNUSED(defs);
    UNUSED(name);

    status = pslGetParameter(detChan, "TRACETRIG", &TRACETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading TRACETRIG for detChan %d", detChan);
        pslLogError("pslGetTriggerType", info_string, status);
        return status;
    }

    *tracetrig = (double)TRACETRIG;
    return XIA_SUCCESS;
}

PSL_STATIC int pslGetTriggerPosition(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t TRACEPRETRIG;
    double *tracepos = (double *)value;

    UNUSED(defs);
    UNUSED(name);

    status = pslGetParameter(detChan, "TRACEPRETRIG", &TRACEPRETRIG);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading TRACEPRETRIG for detChan %d", detChan);
        pslLogError("pslGetTriggerPosition", info_string, status);
        return status;
    }

    *tracepos = (double)TRACEPRETRIG;

    return XIA_SUCCESS;
}


/*
 * board operation get_board_features, check the DSP coderev to
 * determine board features, returns unsigned long representing bit flags
 * defined in BoardFeatures
 */
PSL_STATIC int pslGetBoardFeatures(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    boolean_t isSuper = dxp_is_supermicro(detChan);

    unsigned long *features = (unsigned long *)value;
    unsigned long coderev = dxp_dsp_coderev(detChan);

    UNUSED(defs);
    UNUSED(name);

    *features = BOARD_SUPPORTS_NO_EXTRA_FEATURES;
    *features |= ((unsigned long) (coderev >= MIN_SCA_SUPPORT_CODEREV)) << BOARD_SUPPORTS_SCA;
    *features |= ((unsigned long) (coderev >= MIN_UPDATED_SCA_CODEREV)) << BOARD_SUPPORTS_UPDATED_SCA;
    *features |= ((unsigned long) isSuper) << BOARD_SUPPORTS_TRACETRIGGERS;
    *features |= ((unsigned long) isSuper) << BOARD_SUPPORTS_MULTITRACETYPES;
    *features |= ((unsigned long) isSuper) << BOARD_USE_UPDATED_BOARDINFO;
    *features |= ((unsigned long) (coderev >= MIN_UPDATED_PRESET_CODEREV)) << BOARD_SUPPORTS_UPDATED_PRESET;
    *features |= ((unsigned long) (coderev >= MIN_SNAPSHOT_SUPPORT_CODEREV)) << BOARD_SUPPORTS_SNAPSHOT;
    *features |= ((unsigned long) (coderev >= MIN_PASSTHROUGH_SUPPORT_CODEREV)) << BOARD_SUPPORTS_PASSTHROUGH;
    *features |= ((unsigned long) (coderev >= MIN_SNAPSHOTSCA_SUPPORT_CODEREV)) << BOARD_SUPPORTS_SNAPSHOTSCA;

    return XIA_SUCCESS;
}


/*
 * Pass a command through to a UART attached to the processor.
 *
 * The value type is void**, an array pointing to the following elements:
 *  byte* send: an array of bytes to send with the command.
 *  int* send length: number of bytes in the send array.
 *  byte* receive: an array of bytes to return the command response.
 *  int* receive length: number of bytes to read in the command response.
 *
 */
PSL_STATIC int pslPassthrough(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int status;
    unsigned long features = 0;

    int **value_int_array = (int **)value;
    byte_t **value_byte_array = (byte_t **)value;

    byte_t *send_byte = value_byte_array[0];
    int send_len = *(value_int_array[1]);
    byte_t *receive_byte = value_byte_array[2];
    int receive_len = *(value_int_array[3]);

    /* The return size for the command is fixed at 32 + 1 (status) */
    DEFINE_CMD(CMD_PASSTHROUGH, MAX_PASSTHROUGH_SIZE, RECV_BASE + MAX_PASSTHROUGH_SIZE + 1);

    UNUSED(name);


    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_PASSTHROUGH)) {
        pslLogError("pslPassthrough", "Connected device does not support "
                    "'passthrough' board operation", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    sprintf(info_string, "Sending %d bytes to UART passthrough, receive buffer "
            "%d bytes, for detChan %d", send_len, receive_len, detChan);
    pslLogInfo("pslPassthrough", info_string);

    if (send_len > MAX_PASSTHROUGH_SIZE || receive_len > MAX_PASSTHROUGH_SIZE) {
        sprintf(info_string, "Requested passthrough size send: %d "
                    "receive: %d exceeds supported size: %d", send_len,
                    receive_len, MAX_PASSTHROUGH_SIZE);
        pslLogError("pslPassthrough", info_string, XIA_PARAMETER_OOR);
        return XIA_PARAMETER_OOR;
    }

    lenS = send_len;
    memcpy(send, send_byte, send_len);

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error executing UART passthrough command for "
                "detChan %d", detChan);
        pslLogError("pslPassthrough", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (receive[RECV_BASE] != 0) {
        sprintf(info_string, "Hardware reported error status code 0x%X sending "
                "UART passthrough command for detChan %d", receive[RECV_BASE],
                detChan);
        pslLogError("pslPassthrough", info_string, XIA_PASSTHROUGH);
        return XIA_PASSTHROUGH;
    }

    memcpy(receive_byte, receive + RECV_BASE + 1, receive_len);

    return XIA_SUCCESS;
}

/*
 * Get sca_time_on acquisition value, specified in microseconds
 */
PSL_STATIC int pslGetScaTimeOn(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SCATIMEON;
    double *scaon = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value sca_time_on is not supported by "
                "non-supermicro variant");
        pslLogError("pslGetScaTimeOn", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetParameter(detChan, "SCATIMEON", &SCATIMEON);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SCATIMEON for detChan %d", detChan);
        pslLogError("pslGetScaTimeOn", info_string, status);
        return status;
    }

    *scaon = (double)SCATIMEON / PULSER_PERIOD_SCALE;

    return XIA_SUCCESS;
}


/*
 * Set sca_time_on acquisition value, specified in microseconds
 */
PSL_STATIC int pslSetScaTimeOn(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SCATIMEON;
    double scaon = *((double *)value);

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value sca_time_on is not supported by "
                "non-supermicro variant");
        pslLogError("pslSetScaTimeOn", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    SCATIMEON = (parameter_t)(scaon * PULSER_PERIOD_SCALE);

    if (SCATIMEON > MAX_PULSER_PERIOD || SCATIMEON < MIN_PULSER_PERIOD) {
        sprintf(info_string, "Acquisition value sca_time_on %0f is out of range "
                "for detChan %d", scaon, detChan);
        pslLogError("pslSetScaTimeOn", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    status = pslSetParameter(detChan, "SCATIMEON", SCATIMEON);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting SCATIMEON for detChan %d", detChan);
        pslLogError("pslSetScaTimeOn", info_string, status);
        return status;
    }

    scaon = (double)SCATIMEON / PULSER_PERIOD_SCALE;
    return XIA_SUCCESS;
}


/*
 * Get sca_time_off acquisition value, specified in microseconds
 */
PSL_STATIC int pslGetScaTimeOff(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SCATIMEOFF;
    double *scaoff = (double *)value;

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value sca_time_off is not supported by "
                "non-supermicro variant");
        pslLogError("pslGetScaTimeOff", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetParameter(detChan, "SCATIMEOFF", &SCATIMEOFF);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading SCATIMEOFF for detChan %d", detChan);
        pslLogError("pslGetScaTimeOff", info_string, status);
        return status;
    }

    *scaoff = (double)SCATIMEOFF / PULSER_PERIOD_SCALE;

    return XIA_SUCCESS;
}


/*
 * Set sca_time_off acquisition value, specified in microseconds
 */
PSL_STATIC int pslSetScaTimeOff(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t SCATIMEOFF;
    double scaoff = *((double *)value);

    boolean_t isSuper = dxp_is_supermicro(detChan);

    UNUSED(defs);
    UNUSED(name);

    if (!isSuper) {
        sprintf(info_string, "Acquisition value sca_time_off is not supported by "
                "non-supermicro variant");
        pslLogError("pslSetScaTimeOff", info_string, XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    SCATIMEOFF = (parameter_t)(scaoff * PULSER_PERIOD_SCALE);

    if (SCATIMEOFF > MAX_PULSER_PERIOD || SCATIMEOFF < MIN_PULSER_PERIOD) {
        sprintf(info_string, "Acquisition value sca_time_off %0f is out of range "
                "for detChan %d", scaoff, detChan);
        pslLogError("pslSetScaTimeOff", info_string, XIA_BAD_VALUE);
        return XIA_BAD_VALUE;
    }

    status = pslSetParameter(detChan, "SCATIMEOFF", SCATIMEOFF);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting SCATIMEOFF for detChan %d", detChan);
        pslLogError("pslSetScaTimeOff", info_string, status);
        return status;
    }

    scaoff = (double)SCATIMEOFF / PULSER_PERIOD_SCALE;
    return XIA_SUCCESS;
}


/*
 * Get number_of_scas acquisition value
 */
PSL_STATIC int pslGetNumScas(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;

    parameter_t NUMSCA;
    double *numsca = (double *)value;
    unsigned long max_sca_length;
    unsigned long features = 0;

    UNUSED(name);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SCA)) {
        pslLogError("pslGetNumScas", "Connected device does not support "
                    "'number_of_scas' acquisition value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetParameter(detChan, "NUMSCA", &NUMSCA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading NUMSCA for detChan %d", detChan);
        pslLogError("pslGetNumScas", info_string, status);
        return status;
    }

    status = pslGetMaxSCALength(detChan, (void *)&max_sca_length, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting maximum sca length for detChan %d", detChan);
        pslLogError("pslGetNumScas", info_string, status);
        return status;
    }

    /* This is a fatal error for microDXP, yet it is possible
     * to set the NUMSCA DSP parameter to an arbitrary large number
     * instead of letting the device stuck in an unusable mode
     * just reset the DSP parameter instead
     */
    if (NUMSCA > max_sca_length) {
        sprintf(info_string, "Number of SCAs is greater then the maximum allowed "
                "%lu for detChan %d, resetting to default", max_sca_length, detChan);
        pslLogWarning("pslGetNumScas", info_string);

        NUMSCA = (parameter_t)max_sca_length;

        status = pslSetParameter(detChan, "NUMSCA", NUMSCA);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error setting NUMSCA for detChan %d", detChan);
            pslLogError("pslGetNumScas", info_string, status);
            return status;
        }
    }

    *numsca = (double)NUMSCA;

    return XIA_SUCCESS;
}


/*
 * Set number_of_scas acquisition value
 */
PSL_STATIC int pslSetNumScas(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    unsigned long features = 0;

    parameter_t NUMSCA;
    double nSCA = *((double *)value);
    unsigned long max_sca_length;

    UNUSED(name);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SCA)) {
        pslLogError("pslSetNumScas", "Connected device does not support "
                    "'number_of_scas' acquisition value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    status = pslGetMaxSCALength(detChan, (void *)&max_sca_length, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting maximum sca length for detChan %d", detChan);
        pslLogError("pslGetNumScas", info_string, status);
        return status;
    }

    if ((unsigned int)nSCA > max_sca_length) {
        sprintf(info_string, "Number of SCAs is greater then the maximum allowed "
                "%lu for detChan %d", max_sca_length, detChan);
        pslLogError("pslSetNumScas", info_string, XIA_MAX_SCAS);
        return XIA_MAX_SCAS;
    }

    NUMSCA = (parameter_t)nSCA;

    status = pslSetParameter(detChan, "NUMSCA", NUMSCA);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting NUMSCA for detChan %d", detChan);
        pslLogError("pslSetNumScas", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Get the SCA limit specified in the acquisition name.
 *
 * The name should have the format sca{n}_[lo|hi], where n refers to the SCA #.
 */
PSL_STATIC int pslGetSca(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int idx;
    unsigned short scaNum = 0;
    unsigned long features = 0;

    char limit[9];

    double *scalimit = (double *)value;
    double nSCAs = 0.0;

    unsigned int lenS = 1;
    unsigned int lenR = 0;

    byte_t cmd = CMD_GET_SCALIMIT;
    byte_t *receive = NULL;
    byte_t send[1];

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SCA)) {
        pslLogError("pslGetSca", "Connected device does not support "
                    "'sca' acquisition value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    ASSERT(STRNEQ(name, "sca"));
    sscanf(name, "sca%hu_%s", &scaNum, limit);

    if (!(STREQ(limit, "lo") || STREQ(limit, "hi"))) {
        sprintf(info_string, "Malformed SCA string '%s': missing 'lo' or 'hi' "
                "specifier for detChan %d", name, detChan);
        pslLogError("pslGetSca", info_string, XIA_BAD_NAME);
        return XIA_BAD_NAME;
    }

    status = pslGetAcquisitionValues(detChan, "number_of_scas", (void *)&nSCAs, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting 'number_of_scas' for detChan %d", detChan);
        pslLogError("pslGetSca", info_string, status);
        return status;
    }

    if (scaNum >= nSCAs) {
        sprintf(info_string, "Requested SCA number '%hu' is larger then the number "
                "of SCAs (%0.0f) for detChan %d", scaNum, nSCAs, detChan);
        pslLogError("pslGetSca", info_string, XIA_SCA_OOR);
        return XIA_SCA_OOR;
    }

    lenR = 1 + 4 * (int)nSCAs + RECV_BASE + 1;
    send[0] = 0x01;
    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (receive == NULL) {
        pslLogError("pslGetSca", "Out-of-memory trying to create receive array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        sprintf(info_string, "Error getting SCA limit %s for detChan %d", name,
                detChan);
        pslLogError("pslGetSca", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    idx = 1 + scaNum * 4 + (STREQ(limit, "lo") ? 0 : 1) * 2;

    *scalimit = (double)BYTE_TO_WORD(receive[RECV_DATA_BASE + idx],
                                     receive[RECV_DATA_BASE + idx + 1]);

    utils->funcs->dxp_md_free((void *)receive);
    return XIA_SUCCESS;
}


/*
 * Set the SCA specified in the name.
 *
 * The name should have the format sca{n}_[lo|hi], where
 * n refers to the SCA #.
 */
PSL_STATIC int pslSetSca(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int idx;
    int i;

    unsigned long features = 0;
    unsigned short scaNum = 0;

    char limit[9];

    double *scalimit = (double *)value;
    double nSCAs = 0.0;

    unsigned int lenS = 0;
    unsigned int lenR = 0;

    byte_t cmd = CMD_SET_SCALIMIT;

    byte_t send_get_cmd[1];

    byte_t *send = NULL;
    byte_t *receive = NULL;

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_SCA)) {
        pslLogError("pslSetSca", "Connected device does not support 'sca' "
                    "acquisition value", XIA_NOSUPPORT_VALUE);
        return XIA_NOSUPPORT_VALUE;
    }

    ASSERT(STRNEQ(name, "sca"));

    sscanf(name, "sca%hu_%s", &scaNum, limit);

    if (!(STREQ(limit, "lo") || STREQ(limit, "hi"))) {
        sprintf(info_string, "Malformed SCA string '%s': missing 'lo' or 'hi' "
                "specifier for detChan %d", name, detChan);
        pslLogError("pslSetSca", info_string, XIA_BAD_NAME);
        return XIA_BAD_NAME;
    }

    status = pslGetAcquisitionValues(detChan, "number_of_scas", (void *)&nSCAs, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting 'number_of_scas' for detChan %d", detChan);
        pslLogError("pslSetSca", info_string, status);
        return status;
    }

    if (scaNum >= nSCAs) {
        sprintf(info_string, "Requested SCA number '%hu' is larger then the number "
                "of SCAs (%0.0f) for detChan %d", scaNum, nSCAs, detChan);
        pslLogError("pslSetSca", info_string, XIA_SCA_OOR);
        return XIA_SCA_OOR;
    }

    /* Read out the SCA limits first
     * so that we don't reset the rest of the limits
     */
    lenS = 1;
    lenR = 1 + 4 * (int)nSCAs + RECV_BASE + 1;
    send_get_cmd[0] = 0x01;

    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));
    if (receive == NULL) {
        pslLogError("pslSetSca", "Out-of-memory trying to create receive array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    status = dxp_cmd(&detChan, &cmd, &lenS, send_get_cmd, &lenR, receive);

    if (status != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        sprintf(info_string, "Error getting SCA limit %s for detChan %d", name,
                detChan);
        pslLogError("pslSetSca", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    lenS = 2 + 4 * (int)nSCAs;
    send = (byte_t *)utils->funcs->dxp_md_alloc(lenS * sizeof(byte_t));

    if (send == NULL) {
        utils->funcs->dxp_md_free((void *)receive);
        pslLogError("pslSetSca", "Out-of-memory trying to create send array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    send[0] = 0x00;
    for (i = 0; i < 4 * (int)nSCAs + 1; i++) {
        send[i + 1] = receive[RECV_DATA_BASE + i];
    }

    idx = 2 + scaNum * 4 + (STREQ(limit, "lo") ? 0 : 1) * 2;

    send[idx] = LO_BYTE((int)*scalimit);
    send[idx+1] = HI_BYTE((int)*scalimit);

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);
        utils->funcs->dxp_md_free((void *)send);
        sprintf(info_string, "Error getting SCA limit %s for detChan %d", name,
                detChan);
        pslLogError("pslSetSca", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    utils->funcs->dxp_md_free((void *)send);
    utils->funcs->dxp_md_free((void *)receive);
    return XIA_SUCCESS;
}


/*
 * Get the length of the return SCA data array sca_length, this is
 *   equivalent as number_of_scas acq value
 */
PSL_STATIC int pslGetSCALength(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    double nSCAs = 0.0;

    ASSERT(defs != NULL);

    status = pslGetAcquisitionValues(detChan, "number_of_scas", (void *)&nSCAs, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error finding 'number_of_scas' for detChan %d", detChan);
        pslLogError("pslGetSCALength", info_string, status);
        return status;
    }

    *((unsigned short *)value) = (unsigned short)nSCAs;

    return XIA_SUCCESS;
}


/*
 * Get the SCA data array for the specified channel
 */
PSL_STATIC int pslGetSCAData(int detChan, void *value, XiaDefaults *defs)
{
    int status;
    
    unsigned long features = 0;
    unsigned long addr;
    parameter_t SCASTART = 0x0000;
    
    double number_of_scas = 0.0;
    double *sca64 = (double *)value;

    ASSERT(defs != NULL);
    ASSERT(value != NULL);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if ((status != XIA_SUCCESS) || !(features & 1 << BOARD_SUPPORTS_UPDATED_SCA)) {
        pslLogError("pslGetSCAData", "Connected device does not support 'sca' run "
                    "data", XIA_NOSUPPORT_RUNDATA);
        return XIA_NOSUPPORT_RUNDATA;
    }

    status = pslGetDefault("number_of_scas", (void *)&number_of_scas, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "'number_of_scas' is not in the acquisition value "
                "list for detChan %d. Are there SCAs configured for this channel?",
                detChan);
        pslLogError("pslGetSCAData", info_string, status);
        return status;
    }

    /* Only USB microDxp support direct memory read out */
    if (IS_USB) {
        status = pslGetParameter(detChan, "SCASTART", &SCASTART);
        if (status != XIA_SUCCESS) {
            pslLogError("pslGetSCAData", "Error getting SCASTART", status);
            return status;
        }

        addr = DSP_DATA_MEMORY_OFFSET + SCASTART;        
        status = pslGetSCADataDirect(detChan, (int)number_of_scas, addr, sca64);
    } else {
        status = pslGetSCADataCmd(detChan, (int)number_of_scas, CMD_READ_SCA, sca64);
    }

    if (status != XIA_SUCCESS) {
        pslLogError("pslGetSCAData", "Error reading out SCA data.", status);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int pslGetSCADataCmd(int detChan, int numSca, byte_t cmd, double *sca64)
{
    int statusX;

    int i;

    unsigned int lenR = 0;
    unsigned int lenS = 0;

    byte_t *receive = NULL;

    lenR = 4 * numSca + 2 + RECV_BASE;
    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR * sizeof(byte_t));

    if (receive == NULL) {
        pslLogError("pslGetSCAData", "Out-of-memory trying to create receive array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)receive);

        sprintf(info_string, "Error getting SCA data from detChan %d", detChan);
        pslLogError("pslGetSCAData", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    sprintf(info_string, "Read out %hu SCA data from detChan %d",
            receive[RECV_BASE], detChan);
    pslLogInfo("pslGetSCAData", info_string);

    for (i = 0; i < numSca; i++) {
        sca64[i] = pslDoubleFromBytesOffset(receive, 4, RECV_BASE + 1 + i * 4);
    }

    utils->funcs->dxp_md_free((void *)receive);

    return XIA_SUCCESS;
}

PSL_STATIC int pslGetSCADataDirect(int detChan, int numSca, unsigned long addr,
                                    double *sca64)
{
    int statusX;

    int i;

    unsigned long memLen = 0;

    char mem[MAXITEM_LEN];

    unsigned long *data;

    ASSERT(IS_USB);

    memLen = 2 * numSca;
    sprintf(mem, "direct:%#lx:%lu", addr, memLen);

    data = (unsigned long *)utils->funcs->dxp_md_alloc(memLen * sizeof(unsigned long));

    if (data == NULL) {
        pslLogError("pslGetSCADataDirect", "Out-of-memory trying to create data array",
                    XIA_NOMEM);
        return XIA_NOMEM;
    }

    statusX = dxp_read_memory(&detChan, mem, data);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free((void *)data);
        sprintf(info_string, "Error reading SCA data directly from the "
                "USB (%s) for detChan %d.", mem, detChan);
        pslLogError("pslGetSCADataDirect", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    for (i = 0; i < numSca; i++) {
        sca64[i] = (double)(data[i * 2] + ((data[i * 2 + 1] & 0xFFFF) << 16));
    }

    utils->funcs->dxp_md_free((void *)data);
    return XIA_SUCCESS;

}

/*
 * Get the maximum allowed number of SCA, run data max_sca_length
 */
PSL_STATIC int pslGetMaxSCALength(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    unsigned long features = 0;

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting supported features detChan %d", detChan);
        pslLogError("pslGetMaxSCALength", info_string, status);
        return status;
    }

    *((unsigned short *)value) = (features & 1 << BOARD_SUPPORTS_UPDATED_SCA) ?
                                 (unsigned short)MAX_NUM_INTERNAL_SCA_HI
                                 : (unsigned short)MAX_NUM_INTERNAL_SCA;

    return XIA_SUCCESS;
}


/*
 * Gets auto_adjust_offset from RUNTASKS parameter
 */
PSL_STATIC int pslGetAutoAdjust(int detChan, char *name, XiaDefaults *defs,
                                void *value)
{
    int status;

    parameter_t RUNTASKS;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetParameter(detChan, "RUNTASKS", &RUNTASKS);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting RUNTASKS parameter for detChan %d", detChan);
        pslLogError("pslGetAutoAdjust", info_string, status);
        return status;
    }

    *((double *)value) = (double)((RUNTASKS >> AutoAdjustOffsets) & 0x1);

    return XIA_SUCCESS;
}


/*
 * Sets auto_adjust_offset from RUNTASKS parameter
 */
PSL_STATIC int pslSetAutoAdjust(int detChan, char *name, XiaDefaults *defs,
                                void *value)
{
    int status;

    unsigned int setauto = (unsigned int)*((double *)value);

    parameter_t RUNTASKS;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetParameter(detChan, "RUNTASKS", &RUNTASKS);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting RUNTASKS parameter for detChan %d", detChan);
        pslLogError("pslSetAutoAdjust", info_string, status);
        return status;
    }

    sprintf(info_string, "Set auto %hd RUNTASKS %hu bit position %d mask %hu", setauto, RUNTASKS, AutoAdjustOffsets, ~(0x1 << AutoAdjustOffsets));
    pslLogInfo("pslSetAutoAdjust", info_string);

    if (setauto == 0) {
        RUNTASKS &= ~(0x1 << AutoAdjustOffsets);
    } else {
        RUNTASKS |= (0x1 << AutoAdjustOffsets);
    }

    status = pslSetParameter(detChan, "RUNTASKS", RUNTASKS);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting RUNTASKS parameter for detChan %d", detChan);
        pslLogError("pslSetAutoAdjust", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Get preset_type acquisition value
 */
PSL_STATIC int pslGetPresetType(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    parameter_t PRESET;

    UNUSED(name);
    UNUSED(defs);

    status = pslGetParameter(detChan, "PRESET", &PRESET);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting PRESET parameter for detChan %d", detChan);
        pslLogError("pslGetPresetType", info_string, status);
        return status;
    }

    *((double *)value) = (double)PRESET;

    return XIA_SUCCESS;
}

/*
 * Get preset_value acquisition value
 */
PSL_STATIC int pslGetPresetValue(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    int statusX;

    double *data = (double *)value;

    unsigned long features;
    boolean_t support_long_readout = FALSE_;

    int num_bytes;
    byte_t type = 0x00;

    DEFINE_CMD(CMD_GET_PRESET, 1, 8);

    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);
    ASSERT(status == XIA_SUCCESS);

    support_long_readout = (features & 1 << BOARD_SUPPORTS_UPDATED_PRESET) ;
    num_bytes = support_long_readout ? 6 : 4;

    if (!support_long_readout) {
        OLD_MICRO_CMD(6, 6);
    }

    send[0] = 0x01;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting preset setting for detChan %d", detChan);
        pslLogError("pslGetPresetValue", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    type = receive[RECV_BASE];

    /* Convert the data, "preset length" into scaled value */
    switch(type) {
    case PRESET_STANDARD:
        /* The value is meaningless if we don't know the units in which it was first set. */
        *data = 0.0;
        break;
    case PRESET_REALTIME:
    case PRESET_LIVETIME:
        *data = pslDoubleFromBytesOffset(receive, num_bytes, RECV_BASE + 1) * PRESET_CLOCK_TICK;
        break;
    case PRESET_OUTPUT_COUNTS:
    case PRESET_INPUT_COUNTS:
        *data = pslDoubleFromBytesOffset(receive, num_bytes, RECV_BASE + 1);
        break;
    default:
        sprintf(info_string, "Unknown PRESET run type '%#x'", type);
        pslLogError("pslGetPresetValue", info_string, XIA_UNKNOWN_PRESET);
        return XIA_UNKNOWN_PRESET;
        break;
    }

     return XIA_SUCCESS;
}

/*
 * Set preset_type acquisition value
 */
PSL_STATIC int pslSetPresetType(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status;
    parameter_t PRESET = (parameter_t)*(double *)value;

    UNUSED(name);
    UNUSED(defs);

    ASSERT(value != NULL);

    switch(PRESET) {
    case PRESET_STANDARD:
    case PRESET_REALTIME:
    case PRESET_LIVETIME:
    case PRESET_OUTPUT_COUNTS:
    case PRESET_INPUT_COUNTS:
        break;
    default:
        sprintf(info_string, "Unknown PRESET run type '%#x'", PRESET);
        pslLogError("pslSetPresetType", info_string, XIA_UNKNOWN_PRESET);
        return XIA_UNKNOWN_PRESET;
        break;
    }

    status = pslSetParameter(detChan, "PRESET", PRESET);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting PRESET parameter for detChan %d", detChan);
        pslLogError("pslSetPresetType", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Set preset_value acquisition value
 */
PSL_STATIC int pslSetPresetValue(int detChan, char *name, XiaDefaults *defs, void *value)
{
    int status, statusX;

    unsigned long features;
    boolean_t support_long_readout = FALSE_;

    int num_bytes;
    unsigned long long max_value;

    double preset_type;
    double *preset_value = (double *)value;

    unsigned long long length = 0x00;

    DEFINE_CMD(CMD_SET_PRESET, 8, 8);

    UNUSED(name);

    ASSERT(value != NULL);

    status = pslGetAcquisitionValues(detChan, "preset_type", (void *)&preset_type, defs);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error finding 'preset_type' for detChan %d", detChan);
        pslLogError("pslSetPresetValue", info_string, status);
        return status;
    }

    status = pslGetBoardFeatures(detChan, NULL, defs, (void *)&features);
    ASSERT(status == XIA_SUCCESS);

    support_long_readout = (features & 1 << BOARD_SUPPORTS_UPDATED_PRESET) ;
    num_bytes = support_long_readout ? 6 : 4;
    max_value = (1ULL << (num_bytes * 8)) - 1;

    if (!support_long_readout) {
        OLD_MICRO_CMD(6, 6);
    }

    send[0] = (byte_t)0;
    send[1] = (byte_t)preset_type;

    /* Convert the data[1], "preset length" into the relevant value */
    switch((byte_t)preset_type) {
    case PRESET_STANDARD:
        /* Do nothing since the length will be ignored for this type */
        break;

    case PRESET_REALTIME:
    case PRESET_LIVETIME:
        length = (unsigned long long)(*preset_value / PRESET_CLOCK_TICK);
        break;

    case PRESET_OUTPUT_COUNTS:
    case PRESET_INPUT_COUNTS:
        length = (unsigned long long)*preset_value;
        break;

    default:
        sprintf(info_string, "Unknown PRESET run type '%0f'", preset_type);
        pslLogError("pslSetPresetValue", info_string, XIA_UNKNOWN_PRESET);
        return XIA_UNKNOWN_PRESET;
        break;
    }

    if (length > max_value) {
        sprintf(info_string, "Calculated PRESET length 0x%llx is greater than "
            "maximum allowed 0x%llx, resetting to maximum", length, max_value);
        pslLogDebug("pslSetPresetValue", info_string);
        length = max_value;
    }

    send[2] = (byte_t)(length & 0xFF);
    send[3] = (byte_t)((length >> 8) & 0xFF);
    send[4] = (byte_t)((length >> 16) & 0xFF);
    send[5] = (byte_t)((length >> 24) & 0xFF);

    if (support_long_readout) {
        send[6] = (byte_t)((length >> 32) & 0xFF);
        send[7] = (byte_t)((length >> 40) & 0xFF);
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    sprintf(info_string, "Setting PRESET run: type = %0f, length = %llu, detChan %d",
            preset_type, length, detChan);
    pslLogInfo("pslSetPresetValue", info_string);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting PRESET run: type = %0f, length = "
                "%llu, detChan %d", preset_type, length, detChan);
        pslLogError("pslSetPresetValue", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(receive[RECV_BASE] == (byte_t)preset_type);

    /* Pass the actual value used back to the user */
    switch((byte_t)preset_type) {
    case PRESET_REALTIME:
    case PRESET_LIVETIME:
        *preset_value = pslDoubleFromBytesOffset(receive, num_bytes, RECV_BASE + 1) * PRESET_CLOCK_TICK;
        break;
    case PRESET_OUTPUT_COUNTS:
    case PRESET_INPUT_COUNTS:
        *preset_value = pslDoubleFromBytesOffset(receive, num_bytes, RECV_BASE + 1);
        break;
    default:
        break;
    }

    return XIA_SUCCESS;
}

#ifdef XIA_ALPHA
/*
 * Perform the requested I/O operation with the tilt sensor.
 *
 * rw indicates the direction of the I/O: ALPHA_I2C_READ or
 * ALPHA_I2C_WRITE. reg refers to the registers on the tilt
 * sensor. data is the byte to be read or written to reg.
 *
 * Requires UltraLo firmware and only works with pure USB motherboards.
 */
PSL_STATIC int pslUltraDoTiltIO(int detChan, int rw, byte_t reg, byte_t *data)
{
    int statusX;

    byte_t cmd = CMD_TILT_IO;

    unsigned int lenS = 5;
    unsigned int lenR = RECV_BASE + 1;

    /* To prevent using memory heap (and malloc/free) set these arrays
     * to the worst-case size remembering that we can always use less
     * than the full size.
     */
    byte_t send[6];
    byte_t recv[2 + RECV_BASE];


    ASSERT(IS_USB);
    ASSERT(reg >= ULTRA_TILT_WHO_AM_I && reg <= ULTRA_TILT_DD_THSE_H);
    ASSERT(rw == ALPHA_I2C_READ || rw == ALPHA_I2C_WRITE);


    send[0] = (byte_t)rw;
    send[1] = ULTRA_TILT_I2C_ADDR;
    send[2] = 0x01;
    send[3] = 0x01;
    send[4] = reg;

    switch (rw) {
    case ALPHA_I2C_READ:
        lenR++;
        break;

    case ALPHA_I2C_WRITE:
        lenS++;
        send[5] = *data;
        break;

    default:
        FAIL();
        break;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, recv);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error performing tilt sensor I/O for detChan %d.",
                detChan);
        pslLogError("pslUltraDoTiltIO", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if (rw == ALPHA_I2C_READ) {
        *data = recv[RECV_DATA_BASE];
    }

    return XIA_SUCCESS;
}


/*
 * Initialize the tilt sensor using the configuration required
 * for direction detection mode.
 *
 * Requires UltraLo firmware and pure USB motherboard.
 */
PSL_STATIC int pslUltraTiltInit(int detChan, char *name, XiaDefaults *defs,
                                void *value)
{
    int status;

    byte_t reg;

    double accels[3];

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    /* Clear CTRL_REG3 */
    reg = 0x00;
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_CTRL_REG3,
                              &reg);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error clearing CTRL_REG3 on the tilt sensor for "
                "detChan %d.", detChan);
        pslLogError("pslUltraTiltInit", info_string, status);
        return status;
    }

    /* Configure CTRL_REG1 */
    reg = ULTRA_TILT_CTRL_REG1_NORMAL_MODE;
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_CTRL_REG1,
                              &reg);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting tilt sensor to normal mode "
                "(CTRL_REG1) for detChan %d.", detChan);
        pslLogError("pslUltraTiltInit", info_string, status);
        return status;
    }

    /* Configure CTRL_REG2 */
    reg = ULTRA_TILT_CTRL_REG2_NORMAL_MODE;
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_CTRL_REG2,
                              &reg);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting tilt sensor to normal mode "
                "(CTRL_REG2) for detChan %d.", detChan);
        pslLogError("pslUltraTiltInit", info_string, status);
        return status;
    }

    status = pslUltraTiltGetOutput(detChan, "_debug_tilt_output", defs, accels);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading tilt sensor after initialization "
                "for detChan %d.", detChan);
        pslLogError("pslUltraTiltInit", info_string, status);
        return status;
    }

    sprintf(info_string, "Initial tilt output: a_x = %0.6f g, a_y = %0.6f g, a_z = %0.6f g",
            accels[0], accels[1], accels[2]);
    pslLogDebug("pslUltraTiltInit", info_string);

    return XIA_SUCCESS;
}


/*
 * Returns a 3d array of doubles via value corresponding to
 * the x, y and z acceleration values in g's. If the tilt sensor is
 * not properly initialized the returned values may be garbage.
 */
PSL_STATIC int pslUltraTiltGetOutput(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int status;

    byte_t low;
    byte_t high;

    double *accels;

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);


    accels = (double *)value;

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTX_L,
                              &low);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTX_L register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTX_H,
                              &high);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTX_H register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    accels[0] = pslUltraTiltRawToGs(low, high);

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTY_L,
                              &low);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTY_L register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTY_H,
                              &high);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTY_H register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    accels[1] = pslUltraTiltRawToGs(low, high);

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTZ_L,
                              &low);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTZ_L register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_READ, ULTRA_TILT_OUTZ_H,
                              &high);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading OUTZ_H register for the tilt "
                "sensor using detChan %d.", detChan);
        pslLogError("pslUltraTiltGetOutput", info_string, status);
        return status;
    }

    accels[2] = pslUltraTiltRawToGs(low, high);

    return XIA_SUCCESS;
}


/*
 * Sets the Internal and External thresholds as defined in the
 * datasheet for the accelerometer.
 *
 * Requires UltraLo firmware and USB motherboard.
 */
PSL_STATIC int pslUltraTiltSetThresholds(int detChan, char *name,
                                         XiaDefaults *defs, void *value)
{
    int status;

    double *thresholdGs;

    unsigned short threshInt;
    unsigned short threshExt;

    byte_t threshIntLow;
    byte_t threshIntHigh;
    byte_t threshExtLow;
    byte_t threshExtHigh;


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);
    ASSERT(IS_USB);


    thresholdGs = (double *)value;

    if (thresholdGs[0] < ULTRA_TILT_G_MIN ||
            thresholdGs[1] < ULTRA_TILT_G_MIN ||
            thresholdGs[0] > ULTRA_TILT_G_MAX ||
            thresholdGs[1] > ULTRA_TILT_G_MAX) {

        sprintf(info_string, "Specified internal/external thresholds "
                "(%0.2f g/%0.2f g) are out of allowed range [%0.2f, %0.2f] for "
                "detChan %d.", thresholdGs[0], thresholdGs[1],
                ULTRA_TILT_G_MIN, ULTRA_TILT_G_MAX, detChan);
        pslLogError("pslUltraTiltSetThresholds", info_string,
                    XIA_TILT_THRESHOLD_OOR);
        return XIA_TILT_THRESHOLD_OOR;
    }


    threshInt = (unsigned short)ROUND(thresholdGs[0] * 16384.0);
    threshExt = (unsigned short)ROUND(thresholdGs[1] * 16384.0);

    sprintf(info_string, "Setting internal/external tilt sensor thresholds to: "
            "%0.2f g (%#hx)/%0.2f g (%#hx) for detChan %d.", thresholdGs[0],
            threshInt, thresholdGs[1], threshExt, detChan);
    pslLogDebug("pslUltraTiltSetThresholds", info_string);

    threshIntLow = (byte_t)(threshInt & 0xFF);
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_DD_THSI_L,
                              &threshIntLow);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing tilt sensor internal threshold "
                "(%#hx), low byte for detChan %d.", threshInt, detChan);
        pslLogError("pslUltraTiltSetThresholds", info_string, status);
        return status;
    }

    threshIntHigh = (byte_t)((threshInt >> 8) & 0xFF);
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_DD_THSI_H,
                              &threshIntHigh);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing tilt sensor internal threshold "
                "(%#hx), high byte for detChan %d.", threshInt, detChan);
        pslLogError("pslUltraTiltSetThresholds", info_string, status);
        return status;
    }

    threshExtLow = (byte_t)(threshExt & 0xFF);
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_DD_THSE_L,
                              &threshExtLow);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing tilt sensor external threshold "
                "(%#hx), low byte for detChan %d.", threshExt, detChan);
        pslLogError("pslUltraTiltSetThresholds", info_string, status);
        return status;
    }

    threshExtHigh = (byte_t)((threshExt >> 8) & 0xFF);
    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_DD_THSE_H,
                              &threshExtHigh);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error writing tilt sensor external threshold "
                "(%#hx), high byte for detChan %d.", threshExt, detChan);
        pslLogError("pslUltraTiltSetThresholds", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Enables the DD interrupt on the tilt sensor.
 *
 * Requires UltraLo firmware and USB motherboard.
 */
PSL_STATIC int pslUltraTiltEnableInterlock(int detChan, char *name,
                                           XiaDefaults *defs, void *value)
{
    int status;

    byte_t reg = ULTRA_TILT_DD_CFG_X_LOW;


    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    status = pslUltraDoTiltIO(detChan, ALPHA_I2C_WRITE, ULTRA_TILT_DD_CFG,
                              &reg);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error enabling tilt sensor interlock for "
                "detChan %d.", detChan);
        pslLogError("pslUltraTiltEnableInterlock", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Sets value as a boolean indicating if the tilt sensor has
 * triggered or not.
 *
 * Requires UltraLo firmware and USB motherboard.
 */
PSL_STATIC int pslUltraTiltIsTriggered(int detChan, char *name,
                                       XiaDefaults *defs, void *value)
{
    int statusX;

    char mem[MAXITEM_LEN];

    unsigned long ret;


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);
    ASSERT(IS_USB);


    sprintf(mem, "direct:%#x:%lu", ULTRA_USB_TILT_STATUS, 1);

    statusX = dxp_read_memory(&detChan, mem, &ret);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading tilt status directly from the "
                "USB (%#x) for detChan %d.", ULTRA_USB_TILT_STATUS, detChan);
        pslLogError("pslUltraTiltIsTriggered", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    if ((byte_t)(ret & 0xFF) != 0) {
        sprintf(info_string, "Error reading tilt status: USB reports %#hx for "
                "detChan %d.", ret & 0xFF, detChan);
        pslLogError("pslUltraTiltIsTriggered", info_string, XIA_USB_BUSY);
        return XIA_USB_BUSY;
    }

    *((boolean_t *)value) =
        ((byte_t)((ret >> 8) & 0xFF) == ULTRA_TILT_STATUS_TRIGGERED);

    return XIA_SUCCESS;
}


/*
 * Converts the raw byte output from the tilt sensor into a g
 * value.
 *
 * Assumptions: The tilt sensor is configured for 16-bit,
 * little-endian mode with the full range set to +/- 2g.
 *
 * Even though we have a 16-bit value, the documentation states that
 * the bottom nibble "might assume random values based on the SNR
 * performances of the device."[sic] To be safe we clear the lower
 * 4-bits and make sure that we interpret the result as a signed
 * 16-bit value (with the high bit reserved for the sign aka two's
 * complement). Since the full range is +/- 2g we can convert by
 * normalizing the raw value to (2^15 / 2).
 */
PSL_STATIC double pslUltraTiltRawToGs(byte_t l, byte_t h)
{
    unsigned short raw = BYTE_TO_WORD(l, h);
    raw &= ~(0xF);
    return (double)((signed short)raw) / 16384.0;
}


/*
 * Set the Alpha event length parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaEventLen(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int status;

    unsigned short eventLen;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    eventLen = (unsigned short)*((double *)value);

    if ((eventLen < ALPHA_EVENT_LEN_MIN) || (eventLen > ALPHA_EVENT_LEN_MAX)) {
        sprintf(info_string, "Specified Alpha event length '%u' is outside the "
                "valid range of %d-%d for detChan %d.", eventLen,
                ALPHA_EVENT_LEN_MIN, ALPHA_EVENT_LEN_MAX, detChan);
        pslLogError("pslSetAlphaEventLen", info_string, XIA_EVENT_LEN_OOR);
        return XIA_EVENT_LEN_OOR;
    }

    status = pslSetAlphaParam(detChan, ALPHA_EVENT_LEN, eventLen);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting Alpha event length to %u for detChan %d",
                eventLen, detChan);
        pslLogError("pslSetAlphaEventLen", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the Alpha event length parameter.
 *
 *  Requires Alpha-project firmware.
 */
PSL_STATIC int pslGetAlphaEventLen(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int status;

    unsigned short eventLen = 0;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    status = pslGetAlphaParam(detChan, ALPHA_EVENT_LEN, &eventLen);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting Alpha event length for detChan %d",
                detChan);
        pslLogError("pslGetAlphaEventLen", info_string, status);
        return status;
    }

    *((double *)value) = (double)eventLen;

    return XIA_SUCCESS;
}


/*
 * Set the Alpha pre-buffer length parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaPreBufferLen(int detChan, char *name, XiaDefaults *defs,
                                       void *value)
{
    int status;

    unsigned short preBufLen;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    preBufLen = (unsigned short)*((double *)value);

    if ((preBufLen < ALPHA_PRE_BUF_LEN_MIN) ||
            (preBufLen > ALPHA_PRE_BUF_LEN_MAX)) {
        sprintf(info_string, "Specified Alpha pre-buffer length '%u' is outside the "
                "valid range of %d-%d for detChan %d.", preBufLen,
                ALPHA_PRE_BUF_LEN_MIN, ALPHA_PRE_BUF_LEN_MAX, detChan);
        pslLogError("pslSetAlphaPreBufferLen", info_string, XIA_PRE_BUF_LEN_OOR);
        return XIA_PRE_BUF_LEN_OOR;
    }

    status = pslSetAlphaParam(detChan, ALPHA_PRE_BUF_LEN, preBufLen);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Error setting Alpha pre-buffer length to %u for detChan %d",
                preBufLen, detChan);
        pslLogError("pslSetAlphaPreBufferLen", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the Alpha pre-buffer length parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslGetAlphaPreBufferLen(int detChan, char *name, XiaDefaults *defs,
                                       void *value)
{
    int status;

    unsigned short preBufLen = 0;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    status = pslGetAlphaParam(detChan, ALPHA_PRE_BUF_LEN, &preBufLen);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting Alpha pre-buffer length for detChan %d",
                detChan);
        pslLogError("pslGetAlphaPreBufferLen", info_string, status);
        return status;
    }

    *((double *)value) = (double)preBufLen;

    return XIA_SUCCESS;
}

/*
 * Set the Alpha DAC target parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaDACTarget(int detChan, char *name, XiaDefaults *defs,
                                    void *value)
{
    int status;

    unsigned short dacTarget;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    dacTarget = (unsigned short)*((double *)value);

    if ((dacTarget < ALPHA_DAC_TARGET_MIN) ||
            (dacTarget > ALPHA_DAC_TARGET_MAX)) {
        sprintf(info_string, "Specified Alpha DAC target '%u' is outside the valid "
                "range of %d-%d for detChan %d.", dacTarget, ALPHA_DAC_TARGET_MIN,
                ALPHA_DAC_TARGET_MAX, detChan);
        pslLogError("pslSetAlphaDACTarget", info_string, XIA_DAC_TARGET_OOR);
        return XIA_DAC_TARGET_OOR;
    }

    status = pslSetAlphaParam(detChan, ALPHA_DAC_TARGET, dacTarget);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting Alpha DAC target to %u for detChan %d",
                dacTarget, detChan);
        pslLogError("pslSetAlphaDACTarget", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the Alpha DAC target parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslGetAlphaDACTarget(int detChan, char *name, XiaDefaults *defs,
                                    void *value)
{
    int status;

    unsigned short dacTarget = 0;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    status = pslGetAlphaParam(detChan, ALPHA_DAC_TARGET, &dacTarget);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting Alpha DAC target for detChan %d",
                detChan);
        pslLogError("pslGetAlphaDACTarget", info_string, status);
        return status;
    }

    *((double *)value) = (double)dacTarget;

    return XIA_SUCCESS;
}

/*
 * Set the Alpha DAC tolerance parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaDACTolerance(int detChan, char *name, XiaDefaults *defs,
                                       void *value)
{
    int status;

    unsigned short dacTol;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    dacTol = (unsigned short)*((double *)value);

    if ((dacTol < ALPHA_DAC_TOL_MIN) || (dacTol > ALPHA_DAC_TOL_MAX)) {
        sprintf(info_string, "Specified Alpha DAC tolerance '%u' is outside the "
                "valid range of %d-%d for detChan %d.", dacTol,
                ALPHA_DAC_TOL_MIN, ALPHA_DAC_TOL_MAX, detChan);
        pslLogError("pslSetAlphaDACTolerance", info_string, XIA_DAC_TOL_OOR);
        return XIA_DAC_TOL_OOR;
    }

    status = pslSetAlphaParam(detChan, ALPHA_DAC_TOL, dacTol);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error setting Alpha DAC tolerance to %u for detChan %d",
                dacTol, detChan);
        pslLogError("pslSetAlphaDACTolerance", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get the Alpha DAC tolerance parameter.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslGetAlphaDACTolerance(int detChan, char *name, XiaDefaults *defs,
                                       void *value)
{
    int status;

    unsigned short dacTol = 0;

    UNUSED(defs);
    UNUSED(name);

    ASSERT(value != NULL);


    status = pslGetAlphaParam(detChan, ALPHA_DAC_TOL, &dacTol);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting Alpha DAC tolerance for detChan %d",
                detChan);
        pslLogError("pslGetAlphaDACTolerance", info_string, status);
        return status;
    }

    *((double *)value) = (double)dacTol;

    return XIA_SUCCESS;
}


/*
 * Set an Alpha parameter by index.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaParam(int detChan, unsigned int idx,
                                unsigned short value)
{
    int status;

    unsigned short NUMCUSTSET = 0;

    unsigned int lenS = 4;
    unsigned int lenR;

    byte_t cmd       = CMD_ALPHA_PARAMS;

    byte_t send[4];
    byte_t *receive;


    status = pslGetParameter(detChan, "NUMCUSTSET", &NUMCUSTSET);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Unable to get number of customer parameters for detChan %d "
                " for setting Alpha parameter %u.", detChan, idx);
        pslLogError("pslSetAlphaParam", info_string, status);
        return status;
    }

    ASSERT(idx >=0 && idx < NUMCUSTSET);


    lenR    = (NUMCUSTSET * 2) + 1 + RECV_BASE;
    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR);

    if (receive == NULL) {
        sprintf(info_string, "Out-of-memory allocating %u bytes for 'receive'",
                lenR);
        pslLogError("pslSetAlphaParam", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    send[0] = 0;
    send[1] = (byte_t)idx;
    send[2] = (byte_t)LO_BYTE(value);
    send[3] = (byte_t)HI_BYTE(value);

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    utils->funcs->dxp_md_free(receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Unable to set alpha param %u to %u for detChan %d",
                idx, value, detChan);
        pslLogError("pslSetAlphaParam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Get an Alpha parameter by index.
 *
 * The acquisition values for Alpha parameters are not required,
 * so this routine will return an error unless you first set the
 * acquisition value.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslGetAlphaParam(int detChan, unsigned int idx,
                                unsigned short *value)
{
    int status;

    unsigned short NUMCUSTSET;

    unsigned int lenS = 1;
    unsigned int lenR;

    byte_t cmd       = CMD_ALPHA_PARAMS;

    byte_t send[2];
    byte_t *receive;


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "NUMCUSTSET", &NUMCUSTSET);

    if (status != XIA_SUCCESS) {
        sprintf(info_string,
                "Unable to get number of customer parameters for detChan %d "
                "for getting Alpha parameter %u.", detChan, idx);
        pslLogError("pslGetAlphaParam", info_string, status);
        return status;
    }

    ASSERT(idx >=0 && idx < NUMCUSTSET);


    lenR    = (NUMCUSTSET * 2) + 1 + RECV_BASE;
    receive = (byte_t *)utils->funcs->dxp_md_alloc(lenR);

    if (receive == NULL) {
        sprintf(info_string, "Out-of-memory allocating %u bytes for 'receive'", lenR);
        pslLogError("pslGetAlphaParam", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    send[0] = 1;
    send[1] = (byte_t)idx;

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        utils->funcs->dxp_md_free(receive);
        sprintf(info_string, "Unable to get alpha param %u for detChan %d",
                idx, detChan);
        pslLogError("pslGetAlphaParam", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *value = BYTE_TO_WORD(receive[RECV_DATA_BASE + (idx * 2)],
                          receive[RECV_DATA_BASE + (idx * 2) + 1]);

    utils->funcs->dxp_md_free(receive);

    return XIA_SUCCESS;
}


/*
 * Free the requested number of events in the circular buffer.
 */
PSL_STATIC int pslAlphaFreeEvents(int detChan, unsigned short nEvents)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_FREE_EVENTS, 2, 1);


    ASSERT(nEvents <= ALPHA_MAX_EVENTS_IN_BUFFER);
    ASSERT(nEvents > 0);

    send[0] = LO_BYTE(nEvents);
    send[1] = HI_BYTE(nEvents);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Unable to free %hu events from the buffer of "
                "detChan %d", nEvents, detChan);
        pslLogError("pslAlphaFreeEvents", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Tell the system how many events are to be read in the next call to
 * get them.
 */
PSL_STATIC int pslAlphaRequestEvents(int detChan, char *name,
                                     XiaDefaults *defs, void *value)
{
    UNUSED(name);
    UNUSED(defs);


    ASSERT(detChan == 0 || detChan == 1);
    ASSERT(value != NULL);


    ALPHA_NEXT_N_EVENTS[detChan] = *((unsigned short *)value);

    return XIA_SUCCESS;
}


/*
 * Read a fixed number of events from the buffer on the hardware.
 *
 * It is an exception to request a read across the buffer
 * boundary. The calling routine is responsible for allocating the memory for
 * buf. (It should be large enough to hold nEvt number of event blocks.)
 */
PSL_STATIC int pslAlphaReadFromEventBuffer(int detChan, unsigned short startIdx,
                                           unsigned short nEvt,
                                           unsigned short *buf)
{
    int statusX;

    unsigned short i;

    unsigned long startAddr;
    unsigned long memLen;

    char memStr[MAXITEM_LEN];

    unsigned long *rawMem = NULL;


    ASSERT(detChan == 0 || detChan == 1);
    ASSERT(buf != NULL);
    ASSERT((startIdx + nEvt) <= ALPHA_MAX_EVENTS_IN_BUFFER);

    if (nEvt == 0) {
        sprintf(info_string, "Zero events requested");
        pslLogError("pslAlphaReadFromEventBuffer", info_string, XIA_NO_EVENTS);
        return XIA_NO_EVENTS;
    }

    sprintf(info_string, "Reading %hu events starting at index %hu for "
            "detChan %d", nEvt, startIdx, detChan);
    pslLogDebug("pslAlphaReadFromEventBuffer", info_string);

    startAddr = (startIdx * EVENTLEN) + OUTBUFSTART;
    memLen    = nEvt * 2 * EVENTLEN;

    rawMem = utils->funcs->dxp_md_alloc(memLen * sizeof(unsigned long));

    if (rawMem == NULL) {
        sprintf(info_string, "Error allocating %d bytes for the raw memory "
                "array for detChan %d", (size_t)memLen, detChan);
        pslLogError("pslAlphaReadFromEventBuffer", info_string, XIA_NOMEM);
        return XIA_NOMEM;
    }

    sprintf(memStr, "direct:%#lx:%lu", startAddr, memLen);

    statusX = dxp_read_memory(&detChan, memStr, rawMem);

    if (statusX != DXP_SUCCESS) {
        utils->funcs->dxp_md_free(rawMem);
        sprintf(info_string, "Error reading the memory '%s' for detChan %d",
                memStr, detChan);
        pslLogError("pslAlphaReadFromEventBuffer", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    for (i = 0; i < (nEvt * EVENTLEN); i++) {
        /* The memory should not be packed. */
        ASSERT(((rawMem[i * 2] >> 16) & 0xFFFF) == 0);
        /* The event buffer is located in the DSP Program Memory
         * which is 24-bits wide. The end result is that each read
         * is twice as long as it needs to be since we skip every
         * other 16-bit word.
         */
        buf[i] = (unsigned short)(rawMem[i * 2] & 0xFFFF);
    }

    utils->funcs->dxp_md_free(rawMem);

    return XIA_SUCCESS;
}


/*
 * Read the number of events previously specified via a call to
 * pslAlphaSetNetReadCount() and free them from the system.
 */
PSL_STATIC int pslGetAlphaEvents(int detChan, void *value, XiaDefaults *defs)
{
    int status;

    unsigned short modEvtStartIdx;
    unsigned short nEvtToRead;
    unsigned short evtBase = 0;

    unsigned short *events = NULL;

    UNUSED(defs);


    ASSERT(detChan == 0 || detChan == 1);
    ASSERT(value != NULL);
    ASSERT(EVENTLEN != 0);


    events = (unsigned short *)value;

    modEvtStartIdx = (unsigned short)(ALPHA_EVENT_COUNT[detChan] %
                                      ALPHA_MAX_EVENTS_IN_BUFFER);
    nEvtToRead     = ALPHA_NEXT_N_EVENTS[detChan];

    /* If the # of events to read spans the boundaries of the circular
     * buffer, do the initial chunk first as a special case.
     */
    if ((modEvtStartIdx + nEvtToRead) > ALPHA_MAX_EVENTS_IN_BUFFER) {
        unsigned short nEvtLeftInBuf =
            (unsigned short)(ALPHA_MAX_EVENTS_IN_BUFFER - modEvtStartIdx);

        sprintf(info_string, "Reading %hu events from the end of the buffer "
                "for detChan %d", nEvtLeftInBuf, detChan);
        pslLogDebug("pslGetAlphaEvents", info_string);

        status = pslAlphaReadFromEventBuffer(detChan, modEvtStartIdx,
                                             nEvtLeftInBuf, &events[0]);

        if (status != XIA_SUCCESS) {
            sprintf(info_string, "Error reading %hu events for detChan %d",
                    nEvtLeftInBuf, detChan);
            pslLogError("pslGetAlphaEvents", info_string, status);
            return status;
        }

        modEvtStartIdx = 0;
        nEvtToRead = (unsigned short)(nEvtToRead - nEvtLeftInBuf);
        evtBase = nEvtLeftInBuf;
    }

    sprintf(info_string, "Reading %hu events from the buffer for detChan %d",
            nEvtToRead, detChan);
    pslLogDebug("pslGetAlphaEvents", info_string);

    status = pslAlphaReadFromEventBuffer(detChan, modEvtStartIdx, nEvtToRead,
                                         &events[evtBase * EVENTLEN]);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error reading %hu events for detChan %d",
                nEvtToRead, detChan);
        pslLogError("pslGetAlphaEvents", info_string, status);
        return status;
    }

    /* Free the events we just read. */
    status = pslAlphaFreeEvents(detChan, ALPHA_NEXT_N_EVENTS[detChan]);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error freeing %hu events for detChan %d",
                ALPHA_NEXT_N_EVENTS[detChan], detChan);
        pslLogError("pslGetAlphaEvents", info_string, status);
        return status;
    }

    ALPHA_EVENT_COUNT[detChan] += (unsigned long)ALPHA_NEXT_N_EVENTS[detChan];
    ALPHA_NEXT_N_EVENTS[detChan] = 0;

    return XIA_SUCCESS;
}
#endif /* XIA_ALPHA */


/*
 * Returns the results of running the Status command
 */
PSL_STATIC int pslGetHardwareStatus(int detChan, char *name, XiaDefaults *defs,
                                    void *value)
{
    int statusX;

    byte_t *statusBytes = (byte_t *)value;

    DEFINE_CMD_ZERO_SEND(CMD_STATUS, 6);


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading hardware status for detChan %d",
                detChan);
        pslLogError("pslGetHardwareStatus", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    memcpy(statusBytes, receive + RECV_DATA_OFFSET_STATUS, 5);

    return XIA_SUCCESS;
}


#ifdef XIA_ALPHA
/*
 * Enables or disables the external trigger.
 *
 * On the board this defaults to enabled and is not saved across power cycles.
 *
 * Requires Alpha-project firmware.
 */
PSL_STATIC int pslSetAlphaExtTrigger(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int statusX;

    boolean_t extTrigger;

    DEFINE_CMD(CMD_SET_ALPHA_EXT_TRIGGER, 1, 2);

    UNUSED(defs);
    UNUSED(name);


    ASSERT(value != NULL);


    extTrigger = *((boolean_t *)value);

    if (extTrigger != FALSE_ && extTrigger != TRUE_) {
        sprintf(info_string, "Specified external trigger setting '%u' is invalid. "
                "Must be true or false.", extTrigger);
        pslLogError("pslSetAlphaExtTrigger", info_string, XIA_BAD_TRIGGER);
        return XIA_BAD_TRIGGER;
    }


    send[0] = (byte_t)extTrigger;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string,
                "Error setting Alpha external trigger to %u for detChan %d",
                extTrigger, detChan);
        pslLogError("pslSetAlphaExtTrigger", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((boolean_t *)value) = (boolean_t)receive[RECV_DATA_BASE];

    return XIA_SUCCESS;
}


/*
 * Reads the high voltage value from the Alpha processor.
 * Or in the case of USB2 udxp read directly from i2c bus
 *
 * Requires Alpha-project firmware.
 *
 * value returns the high voltage value as an unsigned short.
 */
PSL_STATIC int pslGetAlphaHV(int detChan, char *name, XiaDefaults *defs,
                             void *value)
{
    int status;

    /* Index of first voltage reading */
    int v_base = RECV_DATA_BASE;

    int i;
    int sum = 0;
    double average;

    byte_t cmd = CMD_GET_ALPHA_HV;

    unsigned int lenS;

    unsigned int lenR = 9 + RECV_BASE;
    byte_t receive[9 + RECV_BASE];

    byte_t *send = NULL;

    UNUSED(name);
    UNUSED(defs);

    ASSERT(value != NULL);

    if (IS_USB) {
        lenS = 5;
    } else {
        lenS = 0;
    }

    send = (byte_t *)utils->funcs->dxp_md_alloc(lenS * sizeof(byte_t));

    if (IS_USB) {
        /* I2C command to get HV */
        send[0] = ALPHA_I2C_READ;
        send[1] = 0x94;
        send[2] = 0x01;
        send[3] = 0x08;
        send[4] = 0x00;
    }

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);
    utils->funcs->dxp_md_free(send);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting Alpha high voltage for "
                "detChan %d", detChan);
        pslLogError("pslGetAlphaHV", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    /* Average the four scaled values to get the current scaled reading.
     */
    for (i = 0; i < 4; i++) {
        sum += BYTE_TO_WORD(receive[v_base + i * 2 + 1],
                            receive[v_base + i * 2]);
    }

    average = sum / 4.0;

    /* And finally scale to volts.
     */
    *((unsigned short *)value) = (unsigned short)ROUND(average / ALPHA_HV_SCALE);

    return XIA_SUCCESS;
}


/*
 * Sets the high voltage value on the Alpha processor. This setting takes
 * effect immediately, so if you want to ramp up to a target, you must call
 * this multiple times, calculating the incremental values yourself.
 *
 * Requires Alpha-project firmware.
 *
 * value: unsigned short representing the target voltage.
 */
PSL_STATIC int pslSetAlphaHV(int detChan, char *name, XiaDefaults *defs,
                             void *value)
{
    int statusX;

    unsigned short volts;
    unsigned short scaledVolts;
    byte_t voltHigh;
    byte_t voltLow;

    byte_t cmd = CMD_SET_ALPHA_HV;

    unsigned int lenS;

    unsigned int lenR = 1 + RECV_BASE;
    byte_t receive[1 + RECV_BASE];

    byte_t *send = NULL;

    UNUSED(name);
    UNUSED(defs);

    volts = *((unsigned short *)value);

    if ((volts < ALPHA_HV_MIN) || (volts > ALPHA_HV_MAX)) {
        sprintf(info_string, "Specified Alpha high voltage value '%hu' is outside "
                "the valid range of %d-%d for detChan %d.", volts,
                ALPHA_HV_MIN, ALPHA_HV_MAX, detChan);
        pslLogError("pslSetAlphaHV", info_string, XIA_HV_OOR);
        return XIA_HV_OOR;
    }

    scaledVolts = (unsigned short)(volts * ALPHA_HV_SCALE);

    voltLow = (byte_t)(scaledVolts & 0xFF);
    voltHigh = (byte_t)((scaledVolts >> 8) & 0xFF);

    if (IS_USB) {
        lenS = 6;
    } else {
        lenS = 2;
    }

    send = (byte_t *)utils->funcs->dxp_md_alloc(lenS * sizeof(byte_t));

    if (IS_USB) {
        /* I2C command to set HV */
        send[0] = ALPHA_I2C_WRITE;
        send[1] = 0x98;
        send[2] = 0x01;
        send[3] = 0x01;
        send[4] = voltHigh;  /* Note the reversed byte order here */
        send[5] = voltLow;
    } else {
        send[0] = voltLow;
        send[1] = voltHigh;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);
    utils->funcs->dxp_md_free(send);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting Alpha high voltage to %hu "
                "for detChan %d", volts, detChan);
        pslLogError("pslSetAlphaHV", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Reads the Alpha motherboard CPLD firmware version number (2 bytes each)
 *
 * Requires Alpha-project firmware.
 *
 * value returns the CPLD firmware version as an unsigned long.
 */
PSL_STATIC int pslGetCPLDVersion(int detChan, char *name, XiaDefaults *defs,
                                 void *value)
{
    int status;

    char mem[MAXITEM_LEN];

    unsigned long* buf = (unsigned long *) value;
    unsigned long version;

    UNUSED(defs);
    UNUSED(name);

    if (!IS_USB) {
        pslLogError("pslGetCPLDVersion",
                    "Reading of motherboard CPLD firmware version not supported", XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(value != NULL);

    sprintf(mem, "direct:%#x:%u", 0xC005, 1);
    status = dxp_read_memory(&detChan, mem, &version);

    /* Revision version LO and revision version HI */
    *buf = version & 0xFFFF;

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetCPLDVersion",
                    "Error reading Alpha motherboard CPLD firmware version low bytes.", status);
        return status;
    }

    sprintf(mem, "direct:%#x:%u", 0xC004, 1);
    status = dxp_read_memory(&detChan, mem, &version);

    /* Major version and Minor version */
    *buf += (version & 0xFFFF) << 16;

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetCPLDVersion",
                    "Error reading Alpha motherboard CPLD "
                    "firmware version high bytes.", status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Disables the on-board pulser.
 */
PSL_STATIC int pslAlphaPulserDisable(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_ENABLE_DISABLE, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error disabling pulser for detChan %d", detChan);
        pslLogError("pslAlphaPulserDisable", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Enable the on-board pulser.
 */
PSL_STATIC int pslAlphaPulserEnable(int detChan, char *name, XiaDefaults *defs,
                                    void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_ENABLE_DISABLE, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error enabling pulser for detChan %d", detChan);
        pslLogError("pslAlphaPulserEnable", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Configure Pulser 1. Expects a 3-tuple of unsigned shorts in value:
 * period, risetime, amplitude.
 */
PSL_STATIC int pslAlphaPulserConfig1(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int statusX;
    int status;

    unsigned short dac = 0;

    unsigned short *config = NULL;

    DEFINE_CMD(CMD_ALPHA_PULSER_CONFIG_1, 6, 1);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);
    ASSERT(IS_USB);


    config = (unsigned short *)value;

    status = pslAlphaPulserComputeDAC(config[2], config[1], &dac);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error computing pulser current DAC for "
                "detChan %d", detChan);
        pslLogError("pslAlphaPulserConfig1", info_string, status);
        return status;
    }

    send[0] = (byte_t)(config[0] & 0xFF);
    send[1] = (byte_t)((config[0] >> 8) & 0xFF);
    send[2] = (byte_t)(config[1] & 0xFF);
    send[3] = (byte_t)((config[1] >> 8) & 0xFF);
    send[4] = (byte_t)(dac & 0xFF);
    send[5] = (byte_t)((dac >> 8) & 0xFF);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error configuring pulser 1: period = %#x, "
                "risetime = %#x, current = %#x for detChan = %d", config[0],
                config[1], dac, detChan);
        pslLogError("pslAlphaPulserConfig1", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Configure Pulser 2. Expects a 4-tuple of unsigned shorts in value:
 * period, risetime, amplitude and delay.
 */
PSL_STATIC int pslAlphaPulserConfig2(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int statusX;
    int status;

    unsigned short dac = 0;

    unsigned short *config = NULL;

    DEFINE_CMD(CMD_ALPHA_PULSER_CONFIG_2, 8, 1);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);
    ASSERT(IS_USB);


    config = (unsigned short *)value;

    status = pslAlphaPulserComputeDAC(config[2], config[1], &dac);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error computing pulser current DAC for "
                "detChan %d", detChan);
        pslLogError("pslAlphaPulserConfig2", info_string, status);
        return status;
    }

    send[0] = (byte_t)(config[0] & 0xFF);
    send[1] = (byte_t)((config[0] >> 8) & 0xFF);
    send[2] = (byte_t)(config[1] & 0xFF);
    send[3] = (byte_t)((config[1] >> 8) & 0xFF);
    send[4] = (byte_t)(dac & 0xFF);
    send[5] = (byte_t)((dac >> 8) & 0xFF);
    send[6] = (byte_t)(config[3] & 0xFF);
    send[7] = (byte_t)((config[3] >> 8) & 0xFF);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error configuring pulser 2: period = %#x, "
                "risetime = %#x, current = %#x, delay = %#x for detChan = %d",
                config[0], config[1], dac, config[3], detChan);
        pslLogError("pslAlphaPulserConfig2", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Sets the pulser mode. value allows the bit masks specified
 * in psl_udxp_alpha.h.
 */
PSL_STATIC int pslAlphaPulserSetMode(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int statusX;

    unsigned int i;

    unsigned long modes;

    DEFINE_CMD(CMD_ALPHA_PULSER_SET_MODE, 3, 1);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);
    ASSERT(IS_USB);


    modes = *((unsigned long *)value);

    memset(send, 0, lenS);

    for (i = 0; i < lenS; i++) {
        if ((modes & (1 << i)) > 0) {
            send[i] = (byte_t)1;
        }
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting pulser mode to %#lx for "
                "detChan = %d", modes, detChan);
        pslLogError("pslAlphaPulserSetMode", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Configures the veto pulse. Accepts a tuple of bytes in value:
 * offset and step value.
 */
PSL_STATIC int pslAlphaPulserConfigVeto(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    byte_t *config;

    DEFINE_CMD_ZERO_SEND(CMD_ALPHA_PULSER_CONFIG_VETO, 1);

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);
    ASSERT(IS_USB);

    lenS = 2;
    config = (byte_t *)value;

    if (config[1] > 16) {
        sprintf(info_string, "Step value is too large! max = 16, value = %u "
                "for detChan = %d", config[1], detChan);
        pslLogError("pslAlphaPulserConfigVeto", info_string,
                    XIA_VETO_PULSE_STEP);
        return XIA_VETO_PULSE_STEP;
    }

    statusX = dxp_cmd(&detChan, &cmd, &lenS, config, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting veto pulse configuration for "
                "detChan = %d", detChan);
        pslLogError("pslAlphaPulserConfigVeto", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}

/*
 * Enable the veto pulse.
 */
PSL_STATIC int pslAlphaPulserEnableVeto(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_ENABLE_DISABLE_VETO, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error enabling veto pulse for detChan %d",
                detChan);
        pslLogError("pslAlphaPulserEnableVeto", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Disable the veto pulse.
 */
PSL_STATIC int pslAlphaPulserDisableVeto(int detChan, char *name,
                                         XiaDefaults *defs, void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_ENABLE_DISABLE_VETO, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error disabling veto pulse for detChan %d",
                detChan);
        pslLogError("pslAlphaPulserDisableVeto", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Start the pulser running.
 */
PSL_STATIC int pslAlphaPulserStart(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_CONTROL, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)1;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error starting pulser for detChan %d",
                detChan);
        pslLogError("pslAlphaPulserStart", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int pslAlphaPulserStop(int detChan, char *name, XiaDefaults *defs,
                                  void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ALPHA_PULSER_CONTROL, 1, 1);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    send[0] = (byte_t)0;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error stopping pulser for detChan %d",
                detChan);
        pslLogError("pslAlphaPulserStop", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


/*
 * Convert the amplitude and risetime into a DAC setting for the pulser.
 */
PSL_STATIC int pslAlphaPulserComputeDAC(unsigned short amplitude,
                                        unsigned short risetime,
                                        unsigned short *dac)
{
    double ampVolts = 0.0;


    ASSERT(dac != NULL);


    ampVolts = (double)amplitude / 1000.0;
    *dac = (unsigned short)ROUND(ALPHA_PULSER_DAC_RANGE *
                                 (ampVolts / (double)risetime));

    if (*dac > ALPHA_PULSER_DAC_MAX) {
        sprintf(info_string, "Calculated pulser current DAC value '%u' "
                "exceeds the maximum of '%u'; setting to the maximum", *dac,
                ALPHA_PULSER_DAC_MAX);
        pslLogWarning("pslAlphaPulserComputeDAC", info_string);
        *dac = ALPHA_PULSER_DAC_MAX;
    }

    return XIA_SUCCESS;
}


/*
 * Gets the number of events (as an unsigned short) ready to be read
 * from the circular buffer for the specified channel.
 */
PSL_STATIC int pslGetAlphaBufferNumEvents(int detChan, void *value,
                                          XiaDefaults *defs)
{
    int status;

    UNUSED(defs);


    ASSERT(value != NULL);


    status = pslGetParameter(detChan, "EVTSINBUF", (unsigned short *)value);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error getting the # of events in the buffer "
                "that are available to be read for detChan %d", detChan);
        pslLogError("pslGetAlphaBufferNumEvents", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Get a snapshot of the statistics. Returns a double array in @a
 * value containing livetime and realtime.
 */
PSL_STATIC int pslGetAlphaStatistics(int detChan, void *value,
                                     XiaDefaults *defs)
{
    int statusX;

    double *stats = NULL;

    DEFINE_CMD_ZERO_SEND(CMD_ALPHA_READ_STATISTICS, 13);

    UNUSED(defs);
    ASSERT(value);

    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting a statistics snapshot for "
                "detChan %d", detChan);
        pslLogError("pslGetAlphaStatistics", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    stats = (double *)value;

    stats[0] =  pslDoubleFromBytesOffset(receive, 6, 5) * LIVETIME_CLOCK_TICK;
    stats[1] =  pslDoubleFromBytesOffset(receive, 6, 11) * ALPHA_REALTIME_CLOCK_TICK;

    return XIA_SUCCESS;
}

/*
 * Sets the channel referenced by detChan as a realtime clock master.
 *
 * This channel should always have its run started last, per the
 * specification https://support.xia.com/default.asp?W387. Our unit
 * tests seem to pass either way, but it breaks if you set a different
 * master without power cycling in between (we never implemented a
 * clear or slave version of this command).
 *
 * In practice our applications always set channel 1 as clock master,
 * and pslStartRun naturally starts the run on channel 0 then 1.
 */
PSL_STATIC int pslUltraSetAsClockMaster(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    DEFINE_CMD(CMD_ULTRA_SLAVE_MASTER, 1, 2);

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);

    if (detChan != 1) {
        sprintf(info_string, "Setting non-guard channel %d as clock master.",
                detChan);
        pslLogWarning("pslUltraSetAsClockMaster", info_string);
    }

    send[0] = (byte_t)ULTRA_CLOCK_MASTER;

    statusX = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Unable to set detChan %d as a clock master.",
                detChan);
        pslLogError("pslUltraSetAsClockMaster", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(receive[5] == (byte_t)ULTRA_CLOCK_MASTER);

    return XIA_SUCCESS;
}


/*
 * Instructs the UltraLo USB to renumerate itself. IMPORTANT: Using this
 * routine completely ruins any existing USB2 connections that Handel has open!
 * To safely renumerate the device, please use the following procedure (as
 * currently documented in the test 'Renumerate and read USB version' in
 * handel_ultra_renumerate.t:
 *
 * 1) xiaBoardOperation('ultra_renumerate_device')
 * 2) xiaExit
 * 3) Sleep a MINIMUM of 3.0 seconds
 * 4) xiaInit
 * 5) xiaStartSystem
 */
PSL_STATIC int pslUltraRenumerateDevice(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    char mem[MAXITEM_LEN];

    unsigned long bang[1] = { 0x21 };


    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);


    ASSERT(IS_USB);


    sprintf(mem, "direct:%#x:%lu", ULTRA_USB_RENUMERATE, 1);

    statusX = dxp_write_memory(&detChan, mem, &bang[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error renumerating UltraLo hardware at "
                "detChan %d.", detChan);
        pslLogError("pslUltraRenumerateDevice", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int pslUltraSetElectrodeSize(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    enum ElectrodeSize es;

    unsigned long size[1];

    char mem[MAXITEM_LEN];

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);
    ASSERT(IS_USB);


    es = *((enum ElectrodeSize *)value);

    if (es >= ElectrodeEnd || es < Electrode1800) {
        sprintf(info_string, "Illegal electrode size: %d for detChan %d.",
                es, detChan);
        pslLogError("pslUltraSetElectrodeSize", info_string,
                    XIA_BAD_ELECTRODE_SIZE);
        return XIA_BAD_ELECTRODE_SIZE;
    }

    size[0] = (unsigned long)es;

    sprintf(mem, "direct:%#x:%lu", 0x05000000, 1);

    sprintf(info_string, "Setting electrode size to %lu via memory write: %s "
            "for detChan %d.", size[0], mem, detChan);
    pslLogDebug("pslUltraSetElectrodeSize", info_string);

    statusX = dxp_write_memory(&detChan, mem, &size[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error setting electrode to %lu via memory write: "
                "%s for detChan %d.", size[0], mem, detChan);
        pslLogError("pslUltraSetElectrodeSize", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}


PSL_STATIC int pslUltraGetElectrodeSize(int detChan, char *name,
                                        XiaDefaults *defs, void *value)
{
    int statusX;

    unsigned long size[1];

    char mem[MAXITEM_LEN];

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);
    ASSERT(IS_USB);


    sprintf(mem, "direct:%#x:%lu", 0x05000000, 1);

    statusX = dxp_read_memory(&detChan, mem, &size[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error getting electrode via memory read: "
                "%s for detChan %d.", mem, detChan);
        pslLogError("pslUltraGetElectrodeSize", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    *((enum ElectrodeSize *)value) = (enum ElectrodeSize)size[0];

    sprintf(info_string, "Electrode size is %lu via memory read: %s for "
            "detChan %d.", size[0], mem, detChan);
    pslLogDebug("pslUltraGetElectrodeSize", info_string);

    return XIA_SUCCESS;
}


PSL_STATIC int pslUltraMoistureRead(int detChan, char *name,
                                    XiaDefaults *defs, void *value)
{
    int statusX;
    int i;

    char mem[MAXITEM_LEN];

    /* (Leaky abstraction alert) Due to the way in which the lower
     * level code unpacks the unsigned longs, we need to pack the
     * bytes in this order. Technically, this is/should be sent to the
     * device as [0x73, 0x65, 0x6E, 0x64, 0xD, 0xC].
     */
    unsigned long request[ULTRA_MM_REQUEST_LEN] = {
        0x6573, 0x646E, 0x0C0D
    };
    unsigned long result[ULTRA_MM_READ_LEN];

    byte_t resultByte[ULTRA_MM_READ_LEN * 2];

    struct MoistureReading *r;

    char mmStatusStr[5];
    char mmValueStr[7];


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value);


    sprintf(mem, "direct:%#x:%u", ULTRA_MM_REQUEST, ULTRA_MM_REQUEST_LEN);

    statusX = dxp_write_memory(&detChan, mem, &request[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error writing moisture meter query request "
                "'%s' for detChan %d.", mem, detChan);
        pslLogError("pslUltraMoistureRead", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    sprintf(mem, "direct:%#x:%u", ULTRA_MM_READ, ULTRA_MM_READ_LEN);

    statusX = dxp_read_memory(&detChan, mem, &result[0]);

    if (statusX != DXP_SUCCESS) {
        sprintf(info_string, "Error reading moisture meter value '%s' for "
                "detChan %d.", mem, detChan);
        pslLogError("pslUltraMoistureRead", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    for (i = 0; i < ULTRA_MM_READ_LEN; i++) {
        resultByte[i * 2] = result[i] & 0xFF;
        resultByte[(i * 2) + 1] = (result[i] >> 8) & 0xFF;
    }

    r = (struct MoistureReading *)value;
    /* Default value. Updated only if we get a valid reading. */
    r->value = 0;

    memcpy(&mmStatusStr[0], &resultByte[0], 4);
    mmStatusStr[4] = 0;

    memcpy(&mmValueStr[0], &resultByte[4], 6);
    mmValueStr[6] = 0;

    if (resultByte[10] != '\r' || resultByte[11] != '\n') {
        sprintf(info_string, "Moisture meter response is malformed. Last two bytes: "
                "'%#x', '%#x'. detChan %d.", resultByte[10], resultByte[11], detChan);
        pslLogError("pslUltraMoistureRead", info_string, XIA_MALFORMED_MM_RESPONSE);
        return XIA_MALFORMED_MM_RESPONSE;
    }

    /* The "API" for this device is a bit underwhelming. While the
     * rest of the values fit nicely into a bitmask/flag scenario,
     * there is one outlier status that we have to test for.
     */
    if (STREQ(mmStatusStr, "0009")) {
        r->status = MMStartup;

    } else {
        unsigned long mmStatus;

        errno = 0;
        mmStatus = strtoul(&mmStatusStr[0], NULL, 2);

        if (mmStatus == ULONG_MAX) {
            sprintf(info_string, "Moisture meter status overflowed on conversion "
                    "for detChan %d.", detChan);
            pslLogError("pslUltraMoistureRead", info_string, XIA_MALFORMED_MM_STATUS);
            return XIA_MALFORMED_MM_STATUS;
        }

        if (mmStatus == 0) {
            char *p;

            /* Special case: Either an error parsing the string or a
             * normal measurement by the moisture meter.
             */
            if (errno == ERANGE) {
                sprintf(info_string, "Unable to parse moisture meter status string "
                        "'%s' for detChan %d.", mmStatusStr, detChan);
                pslLogError("pslUltraMoistureRead", info_string,
                            XIA_MALFORMED_MM_STATUS);
                return XIA_MALFORMED_MM_STATUS;
            }

            /* The value string can be padded with spaces! */
            for (p = &mmValueStr[0]; *p == ' ' && p < &mmValueStr[6]; *p++) {
                /* Nothing */
            }

            if (*p == '\0') {
                sprintf(info_string, "Moisture sensor value string is all spaces "
                        "for detChan %d.", detChan);
                pslLogError("pslUltraMoistureRead", info_string,
                            XIA_MALFORMED_MM_VALUE);
                return XIA_MALFORMED_MM_VALUE;
            }

            errno = 0;
            r->value = strtoul(p, NULL, 10);

            if (r->value == ULONG_MAX) {
                sprintf(info_string, "Moisture meter value '%s' overflowed on "
                        "conversion for detChan %d.", p, detChan);
                pslLogError("pslUltraMoistureRead", info_string,
                            XIA_MALFORMED_MM_VALUE);
                return XIA_MALFORMED_MM_VALUE;
            }

            if (r->value == 0 && errno == ERANGE) {
                sprintf(info_string, "Unable to parse moisture meter value string "
                        "'%s' for detChan %d.", p, detChan);
                pslLogError("pslUltraMoistureRead", info_string,
                            XIA_MALFORMED_MM_VALUE);
                return XIA_MALFORMED_MM_VALUE;
            }
        }

        r->status = (int)mmStatus;
    }

    return XIA_SUCCESS;
}


/*
 * Returns the UltraLo motherboard's unique ID, defined as the serial number
 * in the I2C EEPROM.
 *
 * value returns the unique ID as a double. The value derives from a
 * 48-bit unsigned integer; we use the double only as a portable large number;
 * it will always be positive with zero fractional portion. uint64_t would be
 * preferred once support for old compilers is dropped.
 */
PSL_STATIC int pslUltraGetMBID(int detChan, char *name,
                               XiaDefaults *defs, void *value)
{
    int status;
    unsigned int i;

    DEFINE_CMD(CMD_GET_MB_ID, 5, 9);

    byte_t calcCRC, retCRC;
    byte_t family;
    double sn;

    UNUSED(name);
    UNUSED(defs);
    ASSERT(value);

    ASSERT(IS_USB);


    send[0] = ALPHA_I2C_READ;
    send[1] = ULTRA_MB_EEPROM_I2C_ADDR;
    send[2] = 0x01;
    send[3] = 0x08;
    send[4] = ULTRA_MB_EEPROM_ID;

    status = dxp_cmd(&detChan, &cmd, &lenS, send, &lenR, receive);

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Error getting MICROMB EEPROM registration number "
                "for detChan %d", detChan);
        pslLogError("pslUltraGetMBID", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    calcCRC = pslDOWCRC(&receive[RECV_BASE], 7);
    retCRC = receive[RECV_BASE + 7];

    /* Dump the buffers so we can see what went wrong. */
    if (calcCRC != retCRC) {
        for (i = 0; i < lenS; i++) {
            sprintf(info_string, "send[%u] = %#x", i, send[i]);
            pslLogDebug("pslUltraGetMBID", info_string);
        }

        for (i = 0; i < lenR; i++) {
            sprintf(info_string, "receive[%u] = %#x", i, receive[i]);
            pslLogDebug("pslUltraGetMBID", info_string);
        }

        sprintf(info_string, "CRC mismatch: retCRC = %u, calcCRC = %u",
                retCRC, calcCRC);
        pslLogError("pslUltraGetMBID", info_string, XIA_CHKSUM);
        return XIA_CHKSUM;
    }

    family = receive[RECV_BASE];

    if (family != ULTRA_MB_EEPROM_FAM) {
        sprintf(info_string, "MICROMB EEPROM registration family number = %#x, "
                "expected %#x, for detChan %d.", family, ULTRA_MB_EEPROM_FAM,
                detChan);
        pslLogWarning("pslUltraGetMBID", info_string);
    }

    sn = pslDoubleFromBytes(&receive[RECV_BASE + 1], 6);

    sprintf(info_string, "MICROMB EEPROM registration family number = %#x, "
            "serial number = %.0f, detChan %d.", family, sn, detChan);
    pslLogDebug("pslUltraGetMBID", info_string);

    *((double*)value) = sn;

    return XIA_SUCCESS;
}


/*
 * Computes a DOW CRC over buffer according to the lookup table algorithm
 *  presented in http://www.maximintegrated.com/app-notes/index.mvp/id/27.
 */
PSL_STATIC byte_t pslDOWCRC(byte_t *buffer, int len)
{
    int i;
    byte_t crc;

    byte_t dowLookup[] = {
        0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
        157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
        35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
        190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
        70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
        219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
        101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
        248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
        140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
        17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
        175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
        50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
        202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
        87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
        233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
        116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
    };

    for (i = 0, crc = 0; i < len; i++) {
        crc = dowLookup[crc ^ buffer[i]];
    }

    return crc;
}
#endif /* XIA_ALPHA */


#ifndef EXCLUDE_USB2
/*
 * Reads the uDXP CPLD firmware version number
 *
 * Requires Alpha or SuperMicro firmware.
 *
 * value returns the CPLD firmware version a unsigned long value
 * [3]Major [2]Minor [1-0]Build.
 */
PSL_STATIC int pslGetUdxpCPLDVersion(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int status;

    char mem[MAXITEM_LEN];

    unsigned long* buf = (unsigned long *) value;
    unsigned long version;

    UNUSED(defs);
    UNUSED(name);

    if (!IS_USB) {
        pslLogError("pslGetUdxpCPLDVersion",
                    "Reading of UDXP CPLD firmware version not supported", XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(value != NULL);

    sprintf(mem, "direct:%#x:%u", 0x8003, 1u);
    status = dxp_read_memory(&detChan, mem, &version);

    /* Build version LO and build version HI */
    *buf = version & 0xFFFF;

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetUdxpCPLDVersion",
                    "Error reading udxp CPLD firmware version low bytes.", status);
        return status;
    }

    sprintf(mem, "direct:%#x:%u", 0x8002, 1u);
    status = dxp_read_memory(&detChan, mem, &version);

    /* Major version and Minor version (Exclude the top 4 bits) */
    *buf += (version & 0x0FFF) << 16;

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetUdxpCPLDVersion",
                    "Error reading udxp CPLD firmware version high bytes.", status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Reads the uDXP CPLD firmware variant number (1 byte)
 *
 * Requires Alpha or SuperMicro firmware.
 *
 * value returns the CPLD firmware variant as an unsigned long.
 */
PSL_STATIC int pslGetUdxpCPLDVariant(int detChan, char *name, XiaDefaults *defs,
                                     void *value)
{
    int status;

    char mem[MAXITEM_LEN];

    unsigned long* buf = (unsigned long *) value;
    unsigned long variant;

    UNUSED(defs);
    UNUSED(name);

    if (!IS_USB) {
        pslLogError("pslGetUdxpCPLDVariant",
                    "Reading of UDXP CPLD firmware variant not supported", XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(value != NULL);

    sprintf(mem, "direct:%#x:%u", 0x8002, 1u);
    status = dxp_read_memory(&detChan, mem, &variant);

    /* Top 4 bits of the second byte is the variant */
    *buf = (variant >> 12) & 0xF;

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetUdxpCPLDVariant",
                    "Error reading udxp CPLD firmware variant.", status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Reads the USB firmware version number. Requires Alpha or Rev H
 * firmware.
 *
 * value returns the USB firmware version as an unsigned long
 * [3]Major [2]Minor [0]Build
 */
PSL_STATIC int pslGetUSBVersion(int detChan, char *name, XiaDefaults *defs,
                                void *value)
{
    int status;

    char mem[MAXITEM_LEN];

    unsigned long* buf = (unsigned long *) value;
    unsigned long version[2];

    UNUSED(defs);
    UNUSED(name);
    ASSERT(value != NULL);

    if (!IS_USB) {
        pslLogError("pslGetUSBVersion",
                    "Reading of USB firmware version not supported", XIA_XERXES);
        return XIA_XERXES;
    }

    ASSERT(value != NULL);

#ifdef XIA_ALPHA
    sprintf(mem, "direct:%#x:%u", ULTRA_USB_VERSION, 2u);
#else
    if (!dxp_is_supermicro(detChan)) {
        pslLogError("pslGetUSBVersion",
                    "Reading of USB firmware version not supported", XIA_XERXES);
        return XIA_XERXES;
    }
    sprintf(mem, "direct:%#x:%u", USB_VERSION_ADDRESS, 2u);
#endif

    status = dxp_read_memory(&detChan, mem, &version[0]);

    if (status != DXP_SUCCESS) {
        pslLogError("pslGetUSBVersion",
                    "Error reading USB firmware version.", status);
        return status;
    }

    sprintf(info_string, "Raw version = %#lx %#lx", version[0], version[1]);
    pslLogDebug("pslGetUSBVersion", info_string);

    /* Version bytes: [3]Revision [2] Minor [1]Major [0]status.
     * Reverse the bytes to [3]Major [2]Minor [1-0]Revision as in the
     * CPLD versions. In this case, Revision only has one significant
     * byte but we reserve the same width for consistency.
     */
    *buf = (((version[0] >> 8) & 0xFF) << 24) |
           ((version[1] & 0xFF) << 16) |
           ((version[1] >> 8) & 0xFF);

    status = version[0] & 0xFF;

    if (status != DXP_SUCCESS) {
        sprintf(info_string, "Reading USB firmware version returns "
                "error status %d.", status);
        pslLogError("pslGetUSBVersion", info_string, XIA_XERXES);
        return XIA_XERXES;
    }

    return XIA_SUCCESS;
}

#endif /* EXCLUDE_USB2 */


#ifndef EXCLUDE_XUP

PSL_STATIC int pslQueryStatus(int detChan)
{
    int status;
    int statusX;

    byte_t cmd       = CMD_STATUS;

    unsigned int lenS = 0;
    unsigned int lenR = 6 + RECV_BASE;

    byte_t receive[6 + RECV_BASE];


    statusX = dxp_cmd(&detChan, &cmd, &lenS, NULL,
                      &lenR, receive);

    if (statusX != DXP_SUCCESS) {
        status = XIA_XERXES;
        sprintf(info_string, "Error getting status for detChan %d", detChan);
        pslLogError("pslQueryStatus", info_string, status);
        return status;
    }

    sprintf(info_string, "Return Status      = %u", receive[4]);
    pslLogDebug("pslQueryStatus", info_string);
    sprintf(info_string, "PIC Status         = %u", receive[5]);
    pslLogDebug("pslQueryStatus", info_string);
    sprintf(info_string, "DSP Boot Status    = %u", receive[6]);
    pslLogDebug("pslQueryStatus", info_string);
    sprintf(info_string, "Run State          = %u", receive[7]);
    pslLogDebug("pslQueryStatus", info_string);
    sprintf(info_string, "DSP BUSY value     = %u", receive[8]);
    pslLogDebug("pslQueryStatus", info_string);
    sprintf(info_string, "DSP RUNERROR value = %u", receive[9]);
    pslLogDebug("pslQueryStatus", info_string);

    return XIA_SUCCESS;
}


/*
 * This routine is responsible for upgrading a user's
 * board using the supplied XUP file and for implementing
 * the security model.
 */
PSL_STATIC int pslDownloadXUP(int detChan, char *name, XiaDefaults *defs,
                              void *value)
{
    int status;
    int i;

    boolean_t isRequired;

    char *xup = (char *)value;


    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);


    sprintf(info_string, "xup = %s", xup);
    pslLogDebug("pslDoXUP", info_string);

    status = pslQueryStatus(detChan);

    if (status != XIA_SUCCESS) {
        pslLogError("pslDoXUP", "Error getting status", status);
        return status;
    }
    if (!xupIsChecksumValid(xup)) {
        status = XIA_CHKSUM;
        pslLogError("pslDoXUP",
                    "Checksum mismatch in the XUP",
                    status);
        return status;
    }

    status = xupIsAccessRequired(xup, &isRequired);

    if (status != XIA_SUCCESS) {
        pslLogError("pslDoXUP", "Error determining access status", status);
        return status;
    }

    if (isRequired) {
        /* For XUPs that don't require
         * access information, we can skip
         * this step and just download
         * the XUP.
         */
        status = xupVerifyAccess(detChan, xup);

        if (status != XIA_SUCCESS) {
            pslLogError("pslDoXUP", "Error verifying access code", status);
            return status;
        }
    }

    /* Now the special backup procedures:
     * Write I2C and Flash memory to a special
     * backup file.
     */
    status = xupWriteBackups(detChan, xup);

    if (status != XIA_SUCCESS) {
        pslLogError("pslDoXUP",
                    "Error backing-up memory",
                    status);
        return status;
    }

    status = xupProcess(detChan, xup);

    if (status != XIA_SUCCESS) {
        /* Maybe we should try and restore
         * the backups at this point?
         */
        sprintf(info_string,
                "Error processing %s",
                xup);
        pslLogError("pslDoXUP", info_string, status);
        return status;
    }

    /* Write update information to the
     * special sector(s) reserved for the XUP
     * record.
     */
    status = xupWriteHistory(detChan, xup);

    if (status != XIA_SUCCESS) {
        pslLogError("pslDoXUP",
                    "Error writing history to board",
                    status);
        return status;
    }

    status = xupReboot(detChan);

    if (status != XIA_SUCCESS) {
        pslLogError("pslDoXUP",
                    "Error rebooting board",
                    status);
        return status;
    }

    /* Invalidate all of the acquisition values since we don't know
     * which ones may have changed with the introduction of new firmware.
     */
    for (i = 0; i < NUM_ACQ_VALS; i++) {
        INVALIDATE("pslDoXUP", acqVals[i].name);
    }

    return XIA_SUCCESS;
}


/*
 * Set the path used when the XUP module writes out backup files
 */
PSL_STATIC int pslSetXUPBackupPath(int detChan, char *name, XiaDefaults *defs,
                                   void *value)
{
    int status;

    char *path = (char *)value;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);

    status = xupSetBackupPath(path);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Unable to set XUP backup path to '%s'", path);
        pslLogError("pslSetXUPBackupPath", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}


/*
 * Creates a master parameter set for use with other boards
 */
PSL_STATIC int pslCreateMasterParamSet(int detChan, char *name,
                                       XiaDefaults *defs, void *value)
{
    int status;

    char *paramSet = (char *)value;

    UNUSED(name);
    UNUSED(defs);


    ASSERT(value != NULL);

    status = xupCreateMasterParams(detChan, paramSet);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error creating master parameter set '%s' for "
                "detChan %d", paramSet, detChan);
        pslLogError("pslCreateMasterParamSet", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Creates a backup of the microDXP memory.
 *
 * The backup file is written to the backup directory using the name
 * generated by the backup writer.
 */
PSL_STATIC int pslCreateBackup(int detChan, char *name, XiaDefaults *defs,
                               void *value)
{
    int status;

    UNUSED(name);
    UNUSED(defs);
    UNUSED(value);

    status = xupWriteBackups(detChan, NULL);

    if (status != XIA_SUCCESS) {
        sprintf(info_string, "Error creating hardware backup for detChan %d",
                detChan);
        pslLogError("pslCreateBackup", info_string, status);
        return status;
    }

    return XIA_SUCCESS;
}

#endif /* EXCLUDE_XUP */
