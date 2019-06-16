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

#ifndef __HANDEL_CONSTANTS_H__
#define __HANDEL_CONSTANTS_H__

/* Preset Run Types */
#define XIA_PRESET_NONE           0.0
#define XIA_PRESET_FIXED_REAL     1.0
#define XIA_PRESET_FIXED_LIVE     2.0
#define XIA_PRESET_FIXED_EVENTS   3.0
#define XIA_PRESET_FIXED_TRIGGERS 4.0

/* Preamplifier type */
#define XIA_PREAMP_RESET 0.0
#define XIA_PREAMP_RC    1.0

/* Peak mode */
#define XIA_PEAK_SENSING_MODE     0
#define XIA_PEAK_SAMPLING_MODE    1

/* Mapping Mode Point Control Types */
#define XIA_MAPPING_CTL_GATE 1.0
#define XIA_MAPPING_CTL_SYNC 2.0

/* Trigger and livetime signal output constants */
#define XIA_OUTPUT_DISABLED        0
#define XIA_OUTPUT_FASTFILTER      1
#define XIA_OUTPUT_BASELINEFILTER  2
#define XIA_OUTPUT_ENERGYFILTER    3
#define XIA_OUTPUT_ENERGYACTIVE    4

/* Old Test Constants */
#define XIA_HANDEL_TEST_MASK             0x1
#define XIA_HANDEL_DYN_MODULE_TEST_MASK  0x2
#define XIA_HANDEL_FILE_TEST_MASK        0x4
#define XIA_HANDEL_RUN_PARAMS_TEST_MASK  0x8
#define XIA_HANDEL_RUN_CONTROL_TEST_MASK 0x10
#define XIA_XERXES_TEST_MASK             0x20
#define XIA_HANDEL_SYSTEM_TEST_MASK      0x40

/* List mode variant */
#define XIA_LIST_MODE_SLOW_PIXEL 0.0
#define XIA_LIST_MODE_FAST_PIXEL 1.0
#define XIA_LIST_MODE_CLOCK      2.0
#define XIA_LIST_MODE_PMT        16.0

/* microDXP acquisition value "apply" constants */
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

/* Supported board features returned by get_board_features board operations */
#define BOARD_SUPPORTS_NO_EXTRA_FEATURES 0x0;
#define BOARD_SUPPORTS_SCA	0x01			/* microDxp SCA regions set */
#define BOARD_SUPPORTS_UPDATED_SCA 0x02 	/* microDxp SCA data readout  */
#define BOARD_SUPPORTS_TRACETRIGGERS 0x03 	/* Support for trace triggers */
#define BOARD_SUPPORTS_MULTITRACETYPES 0x04 /* Support for multiple trace types */
#define BOARD_USE_UPDATED_BOARDINFO 0x05 	/* Updated get_board_info response structure */
#define BOARD_SUPPORTS_UPDATED_PRESET 0x06 	/* Extra word for preset values */
#define BOARD_SUPPORTS_SNAPSHOT 0x07 		/* Support for snapshot readout */
#define BOARD_SUPPORTS_PASSTHROUGH 0x08 	/* Support for uart passthrough */
#define BOARD_SUPPORTS_SNAPSHOTSCA 0x09     /* Support for sca snapshot readout */

/* FalconX features 0x11 - 0x18 */

/* UltraLo */
enum ElectrodeSize
{
    Electrode1800 = 0,
    Electrode707 = 1,
    ElectrodeEnd
};

enum MoistureMeterStatus
{
    MMNormal = 0x0,
    MMAutoCalibFail = 0x1,
    MMSettle = 0x2,
    MMAutoCalib = 0x4,
    MMPurge = 0x8,
    /* This is a special value and should not be treated as a bitmask. */
    MMStartup = 0xFF
};

/* STJ trace trigger types */
enum TriggerType
{
	Disabled = 0,
	Threshold = 0x1,
	McaEvent = 0x2,
	OverflowEvent = 0x4,
	UnderflowEvent = 0x8,
	AdcRange = 0x10,
	GateEdge = 0x20,
	FastPileup = 0x40,
	SlowPileup = 0x80
};

/* Bitmask for RUNTASKS DSP parameter in microDxp */
enum RunTasks
{
  FillBaselineHistogram = 0x0,
  AutoAdjustOffsets     = 0x1
};

/* module_statistics_2 index */
enum RunStatistics
{
	Realtime = 0,
	TriggerLivetime,
	EnergyLivetime,
	Triggers,
	Events,
	Icr,
	Ocr,
	Underflows,
	Overflows,
	NUMBER_STATS
};

#endif /* __HANDEL_CONSTANTS_H__ */
