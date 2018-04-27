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


#ifndef _PSL_XMAP_H_
#define _PSL_XMAP_H_


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

/* relative offset for each channel in the external memory SCA block */
#define XMAP_SCA_CHAN_OFFSET      0x40

#define DEFAULT_CLOCK_SPEED 50.0e6


/* These values are really low-level but required for the runtime readout
 * since Handel doesn't support it directly in dxp_get_statisitics().
 */
#define XMAP_MEMORY_BLOCK_SIZE  256
#define XMAP_SCA_PIXEL_BLOCK_HEADER_SIZE 64
#define XMAP_32_EXT_MEMORY 0x3000000

static unsigned long XMAP_STATS_CHAN_OFFSET[] = {
  0x000000,
  0x000040,
  0x000080,
  0x0000C0
};

#define XMAP_STATS_REALTIME_OFFSET   0x0
#define XMAP_STATS_TLIVETIME_OFFSET  0x2
#define XMAP_STATS_ELIVETIME_OFFSET  0x4
#define XMAP_STATS_TRIGGERS_OFFSET   0x6
#define XMAP_STATS_EVENTS_OFFSET     0x8
#define XMAP_STATS_UNDERFLOWS_OFFSET 0xA
#define XMAP_STATS_OVERFLOWS_OFFSET  0xC

/* Mapping flag register bit offsets */
#define XMAP_MFR_BUFFER_A_FULL  1
#define XMAP_MFR_BUFFER_A_DONE  2
#define XMAP_MFR_BUFFER_A_EMPTY 3
#define XMAP_MFR_BUFFER_B_FULL  5
#define XMAP_MFR_BUFFER_B_DONE  6
#define XMAP_MFR_BUFFER_B_EMPTY 7
#define XMAP_MFR_BUFFER_SWITCH  14
#define XMAP_MFR_BUFFER_OVERRUN 15

/* Acquisition value update flags */
#define XMAP_UPDATE_NEVER   0x1
#define XMAP_UPDATE_MAPPING 0x2
#define XMAP_UPDATE_MCA     0x4

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
    XMAP_GATE_MASTER,
    XMAP_SYNC_MASTER,
    XMAP_LBUS_MASTER,
    XMAP_NO_MASTER,
};

/*
 * Macros
 */

/* Memory management */
DXP_MD_ALLOC  xmap_psl_md_alloc;
DXP_MD_FREE   xmap_psl_md_free;

#endif /* _PSL_XMAP_H_ */
