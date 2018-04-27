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


#ifndef _PSL_STJ_H_
#define _PSL_STJ_H_


/*
 * Structures
 */

/* STJ specific function pointer */
typedef int (*Stj_DoSpecialRun_FP)(int, void *, XiaDefaults *, Detector *det);

/* STJ specific PSL structs */
typedef struct _Stj_SpecialRun {

  char         		*name;
  Stj_DoSpecialRun_FP fn;

} Stj_SpecialRun;


/*
 * Constants
 */

#define SYSTEM_GAIN         1.27
#define INPUT_RANGE_MV      2200.0
#define DSP_SCALING         4.0
#define MAX_BINFACT_ITERS   2
#define GAINDAC_BITS        16
#define GAINDAC_DB_RANGE    40.0
#define ADC_RANGE           16384.0

#define MIN_MCA_CHANNELS    256
#define MAX_MCA_CHANNELS    8192
#define MIN_SLOWLEN         5
#define MAX_SLOWLEN         1024
#define MIN_SLOWGAP         0
#define MAX_SLOWGAP         1024
#define MAX_SLOWFILTER      1024
#define MIN_FASTLEN         2
#define MAX_FASTLEN         256
#define MIN_FASTGAP         0
#define MAX_FASTFILTER      256
#define MIN_MAXWIDTH        1
#define MAX_MAXWIDTH        1024
#define MAX_BLFILTERLEN     2048

#define MAX_NUM_INTERNAL_SCA 64

#define DEFAULT_CLOCK_SPEED 50.0e6


/* These values are really low-level but required for the runtime readout
 * since Handel doesn't support it directly in dxp_get_statisitics().
 */
#define STJ_MEMORY_BLOCK_SIZE  256
#define STJ_SCA_PIXEL_BLOCK_HEADER_SIZE 64
#define STJ_32_EXT_MEMORY 0x3000000

/* size of statistics block in SRAM. */
#define STJ_STATS_BLOCK_SIZE    0x400

/* relative offset for each channel in the external memory statistics block. */
#define STJ_STATS_CHAN_OFFSET    0x20

#define STJ_STATS_REALTIME_OFFSET   0x0
#define STJ_STATS_TLIVETIME_OFFSET  0x2
#define STJ_STATS_TRIGGERS_OFFSET   0x4
#define STJ_STATS_EVENTS_OFFSET     0x6
#define STJ_STATS_UNDERFLOWS_OFFSET 0x8
#define STJ_STATS_OVERFLOWS_OFFSET  0xA

/* bias scan data memory locations */
#define STJ_BIAS_SCAN_DATA_LEN		0x2000
#define STJ_BIAS_SCAN_DATA_OFFSET	0x80000
#define STJ_BIAS_SCAN_NOISE_OFFSET	0xC0000

/* DAC values are expressed as signed short int in the range 0xE000-0x1FFF */
#define STJ_DAC_RANGE_MIN -8192
#define STJ_DAC_RANGE_MAX 8191

/* DAC to mV conversion: rounding off from 4.096 */
#define STJ_DAC_PER_MV 4

/* DSP warning for analog module */
#define STJ_ANALOG_DISCONNECTED		0x1

/* Mapping flag register bit offsets */
#define STJ_MFR_BUFFER_A_FULL  1
#define STJ_MFR_BUFFER_A_DONE  2
#define STJ_MFR_BUFFER_A_EMPTY 3
#define STJ_MFR_BUFFER_B_FULL  5
#define STJ_MFR_BUFFER_B_DONE  6
#define STJ_MFR_BUFFER_B_EMPTY 7
#define STJ_MFR_BUFFER_SWITCH  14
#define STJ_MFR_BUFFER_OVERRUN 15

/* Acquisition value update flags */
#define STJ_UPDATE_NEVER   0x1
#define STJ_UPDATE_MAPPING 0x2
#define STJ_UPDATE_MCA     0x4

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

enum master {
    STJ_GATE_MASTER,
    STJ_SYNC_MASTER,
    STJ_LBUS_MASTER,
    STJ_NO_MASTER,
};

/*
 * Macros
 */

/* Memory management */
DXP_MD_ALLOC  stj_psl_md_alloc;
DXP_MD_FREE   stj_psl_md_free;

#endif /* _PSL_STJ_H_ */
