/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2010 XIA LLC
 * All rights reserved.
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
 *   * Neither the name of X-ray Instrumentation Associates 
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
 *
 *
 * $Id: xia_psl.h 15170 2010-03-27 01:26:34Z patrick $
 *
 */


#ifndef __XIA_PSL_H__
#define __XIA_PSL_H__

#include "xia_system.h"
#include "xia_handel_structures.h"
#include "psldef.h"
#include "xia_common.h"


/* All PSL drivers (xmap_psl.c, mercury_psl.c, etc.) need to implement
 * these routines.
 */
PSL_STATIC int PSL_API pslValidateModule(Module *module);
PSL_STATIC int PSL_API pslValidateDefaults(XiaDefaults *defaults);
PSL_STATIC int PSL_API pslDownloadFirmware(int detChan, char *type, char *file, 
                                           Module *m, char *rawFilename,
                                           XiaDefaults *defs);
PSL_STATIC int PSL_API pslSetAcquisitionValues(int detChan, char *name,
                                               void *value,
                                               XiaDefaults *defaults, 
                                               FirmwareSet *firmwareSet, 
                                               CurrentFirmware *currentFirmware, 
                                               char *detectorType, 
                                               Detector *detector,
                                               int detector_chan, Module *m,
											   int modChan);
PSL_STATIC int PSL_API pslGetAcquisitionValues(int detChan, char *name,
											   void *value, 
											   XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGainOperation(int detChan, char *name, void *value,
                                        Detector *detector, int detector_chan,
                                        XiaDefaults *defaults,
                                        CurrentFirmware *currentFirmware,
                                        char *detectorType, Module *m);
PSL_STATIC int PSL_API pslGainChange(int detChan, double deltaGain, 
									 XiaDefaults *defaults, 
									 CurrentFirmware *currentFirmware, 
									 char *detectorType, Detector *detector,
                                     int detector_chan, Module *m, int modChan);
PSL_STATIC int PSL_API pslGainCalibrate(int detChan, Detector *det, int modChan,
										Module *m, XiaDefaults *defs,
										double deltaGain);
PSL_STATIC int pslStartRun(int detChan, unsigned short resume,
                           XiaDefaults *defs, Module *m);
PSL_STATIC int pslStopRun(int detChan, Module *m);
PSL_STATIC int PSL_API pslGetRunData(int detChan, char *name, void *value,
									 XiaDefaults *defaults, Module *m);
PSL_STATIC int pslSetPolarity(int detChan, Detector *det, int detectorChannel,
							  XiaDefaults *defs, Module *m);
PSL_STATIC int PSL_API pslSetDetectorTypeValue(int detChan, Detector *detector, int detectorChannel, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetDefaultAlias(char *alias, char **names,
                                          double *values);
PSL_STATIC unsigned int PSL_API pslGetNumDefaults(void);
PSL_STATIC int PSL_API pslGetParameter(int detChan, const char *name,
                                       unsigned short *value);
PSL_STATIC int PSL_API pslSetParameter(int detChan, const char *name,
                                       unsigned short value);
PSL_STATIC int PSL_API pslUserSetup(int detChan, XiaDefaults *defaults,
                                    FirmwareSet *firmwareSet, 
									CurrentFirmware *currentFirmware,
                                    char *detectorType, Detector *detector,
                                    int detector_chan, Module *m, int modChan);
PSL_STATIC int PSL_API pslDoSpecialRun(int detChan, char *name, void *info, 
                                       XiaDefaults *defaults,
                                       Detector *detector, int detector_chan);
PSL_STATIC int PSL_API pslGetSpecialRunData(int detChan, char *name,
                                            void *value, XiaDefaults *defaults);
PSL_STATIC int PSL_API pslGetNumParams(int detChan, unsigned short *numParams);
PSL_STATIC int PSL_API pslGetParamData(int detChan, char *name, void *value);
PSL_STATIC int PSL_API pslGetParamName(int detChan, unsigned short index,
                                       char *name);
PSL_STATIC int pslBoardOperation(int detChan, char *name, void *value,
								 XiaDefaults *defs);

PSL_STATIC boolean_t PSL_API pslCanRemoveName(char *name);

PSL_STATIC int PSL_API pslUnHook(int detChan);

#endif /* __XIA_PSL_H__ */
