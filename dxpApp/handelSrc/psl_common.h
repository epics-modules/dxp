/*
 * Copyright (c) 2009-2016 XIA LLC
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


#ifndef __PSL_COMMON_H__
#define __PSL_COMMON_H__

#include "psldef.h"

#include "xia_handel_structures.h"

#include "xerxes.h"


/* Logging macro wrappers */
#define pslLogError(x, y, z)	utils->funcs->dxp_md_log(MD_ERROR, (x), (y), (z), __FILE__, __LINE__)
#define pslLogWarning(x, y)	utils->funcs->dxp_md_log(MD_WARNING, (x), (y), 0, __FILE__, __LINE__)
#define pslLogInfo(x, y)	utils->funcs->dxp_md_log(MD_INFO, (x), (y), 0, __FILE__, __LINE__)
#define pslLogDebug(x, y)	utils->funcs->dxp_md_log(MD_DEBUG, (x), (y), 0, __FILE__, __LINE__)

static char info_string[400];

/* PSL function pointers and structs */

/* Individual device PSL can define their customized function pointer
*  in psl_devicename.h or use this generic definition.
*/
typedef int (*DoGainOperation_FP)(int detChan, Detector *det,
                                int modChan, Module *m, XiaDefaults *defs,
                                void *value);
typedef int (*DoBoardOperation_FP)(int, char *, XiaDefaults *, void *);
typedef int (*DoSpecialRun_FP)(int, void *, XiaDefaults *);
typedef int (*DoSpecialRunData_FP)(int, void *, XiaDefaults *);
typedef int (*DoRunData_FP)(int detChan, void *value, XiaDefaults *defs,
				Module *m);
typedef int (*GetAcqValue_FP)(int, void *, XiaDefaults *);
typedef int (*SetAcqValue_FP)(int detChan, int modChan, char *name, void *value,
							  char *detectorType, XiaDefaults *defs, Module *m,
							  Detector *det, FirmwareSet *fs);
typedef int (*SynchAcqValue_FP)(int detChan, int det_chan, Module *m,
								Detector *det, XiaDefaults *defs);
typedef int (*FirmwareDownloader_FP)(int detChan, char *file, char *rawFile,
									 Module *m);
typedef int (*ParamData_FP)(int detChan, void *value);


/* A required acquisition value. Will be
 * merged into the new acquisition values
 * structure eventually.
 */
typedef struct _RequiredDefs {

  char           *name;
  boolean_t      present;
  GetAcqValue_FP fn;

} RequiredDefs;


/* A generic gain operation */
typedef struct _GainOperation {

  char                *name;
  DoGainOperation_FP fn;

} GainOperation;

/* A generic run data type */
typedef struct _RunData {

  char         *name;
  DoRunData_FP fn;

} RunData;


/* A generic special run data type */
typedef struct _SpecialRunData {

  char                *name;
  DoSpecialRunData_FP fn;

} SpecialRunData;


/* A generic special run type */
typedef struct _SpecialRun {

  char            *name;
  DoSpecialRun_FP fn;

} SpecialRun;


/* A generic board operation */
typedef struct _BoardOperation {

  char                *name;
  DoBoardOperation_FP fn;

} BoardOperation;

/* A parameter data type. */
typedef struct _ParamData {

  char *name;
  ParamData_FP fn;

} ParamData_t;

/* A firmware downloader. */
typedef struct _FirmwareDownloader {

  char *name;
  FirmwareDownloader_FP fn;

} FirmwareDownloader_t;

/* A generic acquisition value */
typedef struct _AcquisitionValue {

  char *           name;
  boolean_t        isDefault;
  boolean_t        isSynch;
  unsigned short   update;
  double           def;
  SetAcqValue_FP   setFN;
  GetAcqValue_FP   getFN;
  SynchAcqValue_FP synchFN;

} AcquisitionValue_t;


/* Memory allocation wrappers */
#define MALLOC(n) utils->funcs->dxp_md_alloc(n)
#define FREE(ptr) utils->funcs->dxp_md_free(ptr)


/* Wrappers around other MD utility routines. */
#define FGETS(s, size, stream) utils->funcs->dxp_md_fgets((s), (size), (stream))

/* Macro for block wrapper in Macros to disable the "conditional expression is constant" warning */
#ifdef _WIN32
#  define ONCE __pragma( warning(push) ) \
               __pragma( warning(disable:4127) ) \
               while( 0 ) \
               __pragma( warning(pop) )
#else
#  define ONCE while( 0 )
#endif

#define GET_PARAM_IDX(detChan, name, idx, fname) do { int statusX; statusX = dxp_get_symbol_index(&detChan, name, &idx); if (statusX != DXP_SUCCESS) { sprintf(info_string, "Unable to get the index of '%s' for detChan %d. Xerxes reports status = %d.", name, detChan, statusX); pslLogError("psl__Extract" # fname, info_string, XIA_XERXES); return XIA_XERXES; } } ONCE


/* Shared routines */
PSL_SHARED int PSL_API pslGetDefault(char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int PSL_API pslSetDefault(char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int pslGetModChan(int detChan, Module *m,
                             unsigned int *modChan);
PSL_SHARED int PSL_API pslDestroySCAs(Module *m, unsigned int modChan);
PSL_SHARED XiaDaqEntry * pslFindEntry(char *name, XiaDefaults *defs);
PSL_SHARED int pslInvalidate(char *name, XiaDefaults *defs);
PSL_SHARED void pslDumpDefaults(XiaDefaults *defs);
PSL_SHARED double pslU64ToDouble(unsigned long *u64);
PSL_SHARED int pslRemoveDefault(char *name, XiaDefaults *defs,
                                XiaDaqEntry **removed);
PSL_SHARED boolean_t pslIsUpperCase(char *s);

#endif /* __PSL_COMMON_H__ */
