/*
 *  xia_fdd.h
 *
 *  Created 15-Dec-2001: JEW
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


#ifndef XIA_FDD_H
#define XIA_FDD_H

/* Define some generic constants for use by FDD */
#include <handel_generic.h>
#include <md_generic.h>

/* Include structure typedefs for exporting of global variables */
#include <xia_fdd_structures.h>
#include <xerxes_structures.h>

#include "xia_handel_structures.h"
#include "xia_common.h"

#include <fdddef.h>

#define CODE_VERSION						   0
#define CODE_REVISION		  			   6

/*
 *    CAMAC status Register control codes
 */
#define ALLCHAN              -1

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _FDD_PROTO_
#include <stdio.h>

/*
 * following are internal prototypes for fdd.c routines
 */
FDD_EXPORT int FDD_API xiaFddInitialize(void);
FDD_EXPORT int FDD_API xiaFddInitLibrary(void);
FDD_EXPORT int FDD_API xiaFddGetFirmware(const char *filename, const char *ftype, 
					 double pt,  unsigned int nother, const char **others, 
					 const char *detectorType, char newfilename[MAXFILENAME_LEN], 
					 char rawFilename[MAXFILENAME_LEN]);
FDD_EXPORT int FDD_API xiaFddAddFirmware(const char *filename, const char *ftype, 
					 double ptmin, double ptmax,
					 unsigned short nother, char **others, 
					 const char *ffile, unsigned short numFilter,
					 parameter_t *filterInfo);
FDD_EXPORT int FDD_API xiaFddCleanFirmware(const char *filename);
FDD_EXPORT int FDD_API xiaFddGetNumFilter(const char *filename, double peakingTime, unsigned int nKey, 
					  const char **keywords, unsigned short *numFilter);
FDD_EXPORT int FDD_API xiaFddGetFilterInfo(const char *filename, double peakingTime, unsigned int nKey,
					   const char **keywords, parameter_t *filterInfo);

/* Routines contained in xia_common.c.  Routines that are used across libraries but not exported */
static FILE* dxp_find_file(const char *, const char *, char [MAXFILENAME_LEN]);
FDD_STATIC boolean_t FDD_API xiaFddFindFirmware(const char *filename, const char *ftype, 
					      double ptmin, double ptmax, 
					      unsigned int nother, char **others,
					      const char *mode, FILE **fp, boolean_t *exact, char rawFilename[MAXFILENAME_LEN]);

FDD_IMPORT int dxp_md_init_util(Xia_Util_Functions *funcs, char *type);

#else									/* Begin old style C prototypes */
/*
 * following are internal prototypes for fdd.c routines
 */
FDD_EXPORT int FDD_API xiaFddInitialize();
FDD_EXPORT int FDD_API xiaFddInitLibrary();
FDD_EXPORT int FDD_API xiaFddGetFirmware();
FDD_EXPORT int FDD_API xiaFddAddFirmware();
FDD_EXPORT int FDD_API xiaFddCleanFirmware();
FDD_EXPORT int FDD_API xiaFddGetNumFilter();
FDD_EXPORT int FDD_API xiaFddGetFilterInfo();

/* Routines contained in xia_common.c.  Routines that are used across libraries but not exported */
static FILE* dxp_find_file();
FDD_STATIC boolean_t FDD_API xiaFddFindFirmware();

FDD_IMPORT int dxp_md_init_util();

#endif                                  /*   end if _FDD_PROTO_ */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

/* 
 * Define the utility routines used throughout this library
 */

/* Added new logging routines 8/22/01 -- PJF */

DXP_MD_ERROR_CONTROL fdd_md_error_control;
DXP_MD_ERROR fdd_md_error;
DXP_MD_WARNING fdd_md_warning;
DXP_MD_INFO fdd_md_info;
DXP_MD_DEBUG fdd_md_debug;
DXP_MD_OUTPUT fdd_md_output;
DXP_MD_SUPPRESS_LOG fdd_md_suppress_log;
DXP_MD_ENABLE_LOG fdd_md_enable_log;
DXP_MD_SET_LOG_LEVEL fdd_md_set_log_level;
DXP_MD_LOG fdd_md_log;
DXP_MD_ALLOC fdd_md_alloc;
DXP_MD_FREE fdd_md_free;
DXP_MD_PUTS fdd_md_puts;
DXP_MD_WAIT fdd_md_wait;

/* Logging macro wrappers */
#define xiaFddLogError(x, y, z)	fdd_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define xiaFddLogWarning(x, y)	fdd_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define xiaFddLogInfo(x, y)		fdd_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define xiaFddLogDebug(x, y)		fdd_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)


#endif						/* Endif for XIA_FDD_H */
