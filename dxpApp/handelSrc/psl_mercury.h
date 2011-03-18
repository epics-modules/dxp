/*
 * Copyright (c) 2007-2010 XIA LLC
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
 * 
 * $Id$
 *
 */


#ifndef __PSL_MERCURY_H__
#define __PSL_MERCURY_H__


/** FUNCTION POINTERS **/
typedef int (*DoBoardOperation_FP)(int, char *, XiaDefaults *, void *); 
typedef int (*DoSpecialRunData_FP)(int, void *, XiaDefaults *);
typedef int (*DoSpecialRun_FP)(int, void *, XiaDefaults *);
typedef int (*DoRunData_FP)(int detChan, void *value, XiaDefaults *defs,
							Module *m);
typedef int (*GetAcqValue_FP)(int, void *, XiaDefaults *defs);
typedef int (*SetAcqValue_FP)(int detChan, int modChan, char *name, void *value,
							  char *detectorType, XiaDefaults *defs, Module *m,
							  Detector *det, FirmwareSet *fs);
typedef int (*SynchAcqValue_FP)(int detChan, int det_chan, Module *m,
								Detector *det, XiaDefaults *defs);
typedef int (*FirmwareDownloader_FP)(int detChan, char *file, char *rawFile,
									 Module *m);
typedef int (*ParamData_FP)(int detChan, void *value);



/** STRUCTURES **/

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

/* A required acquisition value. Will be
 * merged into the new acquisition values
 * structure eventually.
 */
typedef struct _RequiredDefs {

  char           *name;
  boolean_t      present;
  GetAcqValue_FP fn;
    
} RequiredDefs;


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


/** Constants **/
#define MIN_MCA_CHANNELS    256.0
#define MAX_MCA_CHANNELS    16384.0
#define MIN_SLOWLEN         5
#define MAX_SLOWLEN         128
#define MIN_SLOWGAP         0
#define MAX_SLOWGAP         128
#define MAX_SLOWFILTER      128
#define MIN_FASTLEN         2
#define MAX_FASTLEN         64
#define MIN_FASTGAP         0
#define MAX_FASTFILTER      64
#define MIN_MAXWIDTH        1
#define MAX_MAXWIDTH        255
#define MAX_NUM_INTERNAL_SCA 64

/* These are relative offsets for each channel in the external memory
 * statistics block.
 */
static unsigned long MERCURY_STATS_CHAN_OFFSET[] = {
  0x000000,
  0x000040,
  0x000080,
  0x0000C0
};

/* These values are really low-level but required for the runtime readout
 * since Handel doesn't support it directly in dxp_get_statisitics().
 */
#define MERCURY_MEMORY_BLOCK_SIZE  256
 
#define MERCURY_STATS_REALTIME_OFFSET   0x0
#define MERCURY_STATS_TLIVETIME_OFFSET  0x2
#define MERCURY_STATS_ELIVETIME_OFFSET  0x4
#define MERCURY_STATS_TRIGGERS_OFFSET   0x6
#define MERCURY_STATS_MCAEVENTS_OFFSET  0x8
#define MERCURY_STATS_UNDERFLOWS_OFFSET 0xA
#define MERCURY_STATS_OVERFLOWS_OFFSET  0xC

/* Mapping flag register bit offsets */
#define MERCURY_MFR_BUFFER_A_FULL  1
#define MERCURY_MFR_BUFFER_A_DONE  2
#define MERCURY_MFR_BUFFER_A_EMPTY 3
#define MERCURY_MFR_BUFFER_B_FULL  5
#define MERCURY_MFR_BUFFER_B_DONE  6
#define MERCURY_MFR_BUFFER_B_EMPTY 7
#define MERCURY_MFR_BUFFER_OVERRUN 15

/* Mapping control register bit offsets */
#define MERCURY_MCR_PIXEL_ADVANCE 4

/* Mapping buffer block offsets */
#define MERCURY_BUFFER_BLOCK_SIZE  256

/* System FPGA variant register bit offset */
#define MERCURY_VAR_DAQ_MODE 0

/* Acquisition value update flags */
#define MERCURY_UPDATE_NEVER   0x1
#define MERCURY_UPDATE_MAPPING 0x2
#define MERCURY_UPDATE_MCA     0x4

/* Masks for psl__IsMapping(). */
#define MAPPING_MCA  0x1
#define MAPPING_SCA  0x2
#define MAPPING_LIST 0x4
#define MAPPING_ANY  (MAPPING_MCA | MAPPING_SCA | MAPPING_LIST)

/* Actual MAPPINGMODE constants. */
#define MAPPINGMODE_NIL  0
#define MAPPINGMODE_MCA  1
#define MAPPINGMODE_SCA  2
#define MAPPINGMODE_LIST 3


/* Clock speed */
#define MERCURY_CLOCK_SPEED 50.0e6

/* Gain */
#define MERCURY_INPUT_RANGE_MV   2200.0
#define MERCURY_ADC_RANGE        16384.0
#define MERCURY_SYSTEM_GAIN      1.27
#define MERCURY_GAINDAC_BITS     16
#define MERCURY_GAINDAC_DB_RANGE 40.0

/* Control */
#define MERCURY_MAX_BINFACT_ITERS 2

/* For reading and setting the serial number */
#define SERIAL_NUM_LEN  16
#define BOARD_SER_NUM   0x10009300

/* Temperature calibration setting */
#define MERCURY_TEMP_NO_CORRECTION	0

/** Memory Management **/
DXP_MD_ALLOC  mercury_psl_md_alloc;
DXP_MD_FREE   mercury_psl_md_free;

#ifdef USE_XIA_MEM_MANAGER
#include "xia_mem.h"
#define mercury_psl_md_alloc(n)  xia_mem_malloc((n), __FILE__, __LINE__)
#define mercury_psl_md_free(ptr) xia_mem_free(ptr)
#endif /* USE_XIA_MEM_MANAGER */

#endif /* __PSL_MERCURY_H__ */


