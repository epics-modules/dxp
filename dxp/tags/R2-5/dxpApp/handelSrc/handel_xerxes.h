
/*
 * handel_xerxes.h
 *
 * Created 10/26/01 -- PJF
 *
 * Copyright (c) 2002, X-ray Instrumentation Associates
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
 */


#ifndef HANDEL_XERXES_H
#define HANDEL_XERXES_H

#include "handeldef.h"
#include "xia_handel_structures.h"

/** Constants **/

/* This is very, very hacky and will be made
 * more robust soon.
 */
/*
#ifdef EXCLUDE_UDXP
#define KNOWN_BOARDS  6
#else
#define KNOWN_BOARDS  8
#endif
*/



static char *null[1]                 = { "NULL" };

static char *boardList[] = {
#ifndef EXCLUDE_DXP4C2X
  "dxp2x",
  "dxp4c2x",
#endif /* EXCLUDE_DXP4C2X */
#ifndef EXCLUDE_DXP4C
  "dxp4c",
#endif /* EXCLUDE_DXP4C */
#ifndef EXCLUDE_DXPX10P
  "dxpx10p",
#endif /* EXCLUDE_DXPX10P */
#ifndef EXCLUDE_DGF200
  "dgfg200",
#endif /* EXCLUDE_DGFG200 */
#ifndef EXCLUDE_POLARIS
  "polaris",
#endif /* EXCLUDE_POLARIS */
#ifndef EXCLUDE_UDXPS
  "udxps",
#endif /* EXCLUDE_UDXPS */
#ifndef EXCLUDE_UDXP
  "udxp",
#endif /* EXCLUDE_UDXP */
  };


/* These EXCLUDES must be kept in
 * sync with the EXCLUDES in
 * xia_module.h
 */
static char *interfList[] = { "bad",
#ifndef EXCLUDE_CAMAC
							   "CAMAC",
							   "CAMAC",
#endif /* EXCLUDE_CAMAC */
#ifndef EXCLUDE_EPP
							   "EPP",
							   "EPP",
#endif /* EXCLUDE_EPP */
#ifndef EXCLUDE_SERIAL
							   "SERIAL",
#endif /* EXCLUDE_SERIAL */
#ifndef EXCLUDE_USB
							   "USB",
#endif /* EXCLUDE_USB */
#ifndef EXCLUDE_ARCNET
							   "ARCNET",
#endif /* EXCLUDE_ARCNET */
};

#define KNOWN_BOARDS  (sizeof(boardList) / sizeof(boardList[0]))

/** Prototypes **/
HANDEL_STATIC void HANDEL_API xiaBuildInterfString(Module *module, char *interfStr[2]);
HANDEL_STATIC void HANDEL_API xiaBuildMDString(Module *module, char *mdString);
HANDEL_STATIC int HANDEL_API xiaGetDSPName(Module *module, int channel, 
					   double peakingTime, char *dspName, 
					   char *detectorType, char *rawFilename);
HANDEL_STATIC int HANDEL_API xiaGetFippiName(Module *module, int channel, double peakingTime, 
					     char *fippiName, char *detectorType, 
					     char *rawFilename);
 
#endif /* HANDEL_XERXES_H */
