/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2016 XIA LLC
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

#ifndef _PSL_UDXP_H_
#define _PSL_UDXP_H_

/*
 * Function pointers
 */

typedef int (*Udxp_GetAcqValue_FP)(int detChan, char *name, XiaDefaults *defs, void *value);
typedef int (*Udxp_SetAcqValue_FP)(int detChan, char *name, XiaDefaults *defs, void *value);
typedef int (*Udxp_DoRunData_FP)(int detChan, void *value, XiaDefaults *defs);

/*
 * Structures
 */

/* A udxp specific acquisition value */
typedef struct _Udxp_AcquisitionValue {

  char           		*name;
  flag_t         		member;
  double         		def;
  Udxp_SetAcqValue_FP  	setFN;
  Udxp_GetAcqValue_FP 	getFN;

} Udxp_AcquisitionValue;


typedef struct _Udxp_RunData {

  char         		*name;
  Udxp_DoRunData_FP fn;

} Udxp_RunData;

/* This struct is used to
 * store the information
 * about a specific peaking
 * time, including the DSP
 * clock speed associated
 * with this peaking time, the
 * FiPPI # and the PARSET #.
 */
typedef struct _PeakingTimeRecord {

  double         time;
  double         clock;
  unsigned short fippi;
  unsigned short fipIdx;
  unsigned short parset;

} PeakingTimeRecord;


/* Ketek High/Log Gain to Variable Gain lookup table */
static const double HIGHLOW_GAIN_LUT[2] =
{
	2.0,
	4.02
};

static const double HIGHLOW_LOWEST_BASEGAIN = 2.423;
static const double HIGH_LOW_GAIN_SPACING = 6.08;

/* Switch Gain SWGAIN to Variable Gain (V/V) lookup table */
static const double VARIABLE_GAIN_LUT[16] =
{
	1.217,
	1.476,
	1.806,
	2.186,
	2.659,
	3.226,
	3.947,
	4.777,
	5.772,
	7.003,
	8.567,
	10.37,
	12.82,
	15.56,
	19.03,
	23.04
};

static const double VARIABLE_LOWEST_BASEGAIN = 3.848;
static const double VARIABLE_GAIN_SPACING = 1.7;

/*
 * Constants
 */

#define BASE_CLOCK_STD    32.0

/* Non-supermicro decay_time clock scaling */
#define DECAYTIME_CLOCK_SPEED 8.0

/* Custom clock speed conversion */
#define LIVETIME_CLOCK_TICK 500.0e-9
#define REALTIME_CLOCK_TICK 500.0e-9
#define PRESET_CLOCK_TICK   500.0e-9

#define AV_MEM_ALL        0x0
#define AV_MEM_NONE       0x01
#define AV_MEM_REQ        0x02
#define AV_MEM_PARSET     0x04
#define AV_MEM_GENSET     0x08
#define AV_MEM_FIPPI      0x10
#define AV_MEM_ADC        0x20
#define AV_MEM_GLOB       0x40
#define AV_MEM_CUST       0x80

#define AV_MEM_R_PAR    (AV_MEM_REQ | AV_MEM_PARSET)
#define AV_MEM_R_GEN    (AV_MEM_REQ | AV_MEM_GENSET)
#define AV_MEM_R_FIP    (AV_MEM_REQ | AV_MEM_FIPPI)
#define AV_MEM_R_ADC    (AV_MEM_REQ | AV_MEM_ADC)
#define AV_MEM_R_GLB    (AV_MEM_REQ | AV_MEM_GLOB)

#define MIN_BIN_WIDTH     1.0
#define MAX_BIN_WIDTH   255.0

#define PRESET_STANDARD      0
#define PRESET_REALTIME      1
#define PRESET_LIVETIME      2
#define PRESET_OUTPUT_COUNTS 3
#define PRESET_INPUT_COUNTS  4


#define DB_PER_LSB          (40.0 / 65535.0)

/* Base gain scaling factor is 1 / (sqrt(10)) */
#define GAIN_SCALE_FACTOR	0.316227766

/* Additional gain scaling factor for High/Low switched gain */
#define GAIN_HIGHLOW_FACTOR	2.61

/* Min tracewait depends on the clock speed. Max is fixed in the spec. */
#define MAXIMUM_TRACEWAIT    512.0 /* microseconds */

#define PREAMP_TYPE_RESET    0
#define PREAMP_TYPE_RC       1

#define GAIN_TRIM_LINEAR_MIN  .5
#define GAIN_TRIM_LINEAR_MAX 2.0
#define GAIN_LINEAR_MIN      1.0
#define GAIN_LINEAR_MAX    100.0
#define GAINTWEAK_MAX		65535


#define GAIN_MODE_FIXED     0
#define GAIN_MODE_VGA       1
#define GAIN_MODE_DIGITAL   2
#define GAIN_MODE_SWITCHED  3
#define GAIN_MODE_HIGHLOW   4

#define MAX_NUM_BINS     8192

#define MAX_THRESHOLD_STD   255.0
#define MAX_THRESHOLD_SUPER 4095.0
#define MIN_THRESHOLD       0.0

#define MAX_FILTER_PARAM(super) ((super) ?  0x3FF : 0xFF)

/* peak interval and peak sampling time have wider ranges than raw parameters. */
#define MAX_FILTER_TIMER(super) ((super) ?  0xFFF : 0xFF)

#define FILTER_SLOWLEN  0
#define FILTER_SLOWGAP  1
#define FILTER_PEAKINT  2
#define FILTER_PEAKSAM  3
#define FILTER_FASTLEN  4
#define FILTER_FASTGAP  5
#define FILTER_MINWIDTH 6
#define FILTER_MAXWIDTH 7
#define FILTER_BFACTOR  8
#define FILTER_PEAKMODE 9

#define MAX_TRACEWAIT_US 400.0

#define MAX_GAINBASE    65535
#define MIN_GAINBASE        0

#define MIN_BYTES_PER_BIN 1.0
#define MAX_BYTES_PER_BIN 3.0
#define RAW_BYTES_PER_BIN 4

/* This is in 16-bit words */
#define BASELINE_LEN 1024

#define USB_VERSION_ADDRESS     0x04000000

#define DEBUG_TRACE_TYPE (N_ELEMS(traceTypes) - 1)

#define MAX_NUM_INTERNAL_SCA 4
#define MAX_NUM_INTERNAL_SCA_HI 16

#define MIN_SCA_SUPPORT_CODEREV 		0x0406
#define MIN_UPDATED_SCA_CODEREV 		0x0520
#define MIN_UPDATED_PRESET_CODEREV 		0x0431
#define MIN_SNAPSHOT_SUPPORT_CODEREV 	0x0431
#define MIN_PASSTHROUGH_SUPPORT_CODEREV 0x0576
#define MIN_SNAPSHOTSCA_SUPPORT_CODEREV 0x0584

#define MAX_PASSTHROUGH_SIZE 32

#define MIN_PULSER_PERIOD 1
#define MAX_PULSER_PERIOD 255

#define MAX_RESET_INTERVAL 	255
#define MAX_DECAY_TIME		65535

/* Scale ratio between pulser period to microseconds */
#define PULSER_PERIOD_SCALE 40.0

/*
 * Macros
 */

#if defined(_MSC_VER) && (_MSC_VER >= 1300)
#define INVALIDATE_END          \
__pragma(warning(push))         \
__pragma(warning(disable:4127)) \
} while (FALSE_)                \
__pragma(warning(pop))
#else
#define INVALIDATE_END } while (FALSE_)
#endif /* _WIN32 */

#define INVALIDATE(fn, x)                                                       \
do {                                                                            \
  status = pslInvalidate((x), defs);                                            \
  if (status != XIA_SUCCESS) {                                                  \
    sprintf(info_string, "Error invalidating %s for detChan %d", (x), detChan); \
    pslLogError((fn), info_string, status);                                     \
    return status;                                                              \
  }                                                                             \
INVALIDATE_END

#endif /* _PSL_UDXP_H_ */
