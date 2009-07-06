/*
 * psl_dxp4c2x.h
 *
 * Copyright (c) 2004, X-ray Instrumentation Associates
 *               2005, XIA LCC
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
 * $Id: psl_dxp4c2x.h,v 1.3 2009-07-06 18:24:30 rivers Exp $
 *
 */

#ifndef _PSL_DXP4C2X_H_
#define _PSL_DXP4C2X_H_


/** FUNCTION POINTERS **/
typedef int (*DoBoardOperation_FP)(int, char *, XiaDefaults *, void *); 
typedef int (*SetAcqValue_FP)(int detChan, void *value, FirmwareSet *fs,
							  char *detType, XiaDefaults *defs, double preampGain,
							  double gainScale, Module *m, Detector *det,
							  int detector_chan);
typedef int (*SynchAcqValue_FP)(int detChan, int det_chan, Module *m,
								Detector *det, XiaDefaults *defs);


/** STRUCTURES **/


/* A generic board operation */
typedef struct BoardOperation {
  
  char                *name;
  DoBoardOperation_FP fn;

} BoardOperation_t;


/* Acquisition Value defaults */
typedef struct AcquisitionValue {
  
  char             *name;
  boolean_t         isDefault;
  boolean_t         isSynch;
  double            def;
  SetAcqValue_FP    setFN;
  SynchAcqValue_FP  synchFN;

} AcquisitionValue_t;


/** MACROS **/

/* This saves us a lot of typing. */
#define ACQUISITION_VALUE(x) \
  PSL_STATIC int pslDo ## x (int detChan, void *value, FirmwareSet *fs, \
							 char *detectorType, XiaDefaults *defs, \
							 double preampGain, double gainScale, \
							 Module *m, Detector *det, int detector_chan)


/** CONSTANTS **/

#define RT_BASELINE_CUT  0x400

#define MIN_NUM_BASELINE_SAMPLES 2.0
#define MAX_NUM_BASELINE_SAMPLES 32768.0

#endif /* _PSL_DXP4C2X_H_ */


