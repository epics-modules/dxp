/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2015 XIA LLC
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

#ifndef _UDXP_COMMAND_H_
#define _UDXP_COMMAND_H_

#define RECV_BASE               5
#define RECV_DATA_OFFSET        4
#define RECV_DATA_OFFSET_STATUS 5
#define RECV_DATA_BASE          5

#define SERIAL_NUM_LEN 16

#define NUM_SECTORS_FOR_FIPPI      146
#define WORDS_PER_SECTOR           128
#define BYTES_PER_SECTOR           256
#define QUADRANT_SIZE               64
#define NUM_QUADRANTS                4
#define MAX_FLASH_READ              32
#define MAX_I2C_READ                32
#define SECTOR_SIZE_BYTES          256
#define MAX_I2C_WRITE_BYTES         64
#define FLASH_MEMORY_SIZE_BYTES ((unsigned long)131072)
#define I2C_MEMORY_SIZE_BYTES   ((unsigned int)16384)

#define XUP_HISTORY_ADDR     0x0380

#define GEN_ADDR_OFFSET      0x0000
#define PARAMS_ADDR_OFFSET   0x0180
#define DATA_ADDR_OFFSET     0x0980

#define I2C_PREAM_OFFSET     0x0000
#define I2C_DSP_OFFSET       0x0080

#define DSP_DATA_MEMORY_OFFSET 0x4000
#define RELEASE_IDMA_BUS_ADDR  0x8001

/* Run Control and Readout */
#define CMD_START_RUN           0x00
#define CMD_STOP_RUN            0x01
#define CMD_READ_MCA            0x02
#define CMD_READ_SCA            0x04
#define CMD_READ_STATISTICS     0x06
#define CMD_SET_PRESET          0x07
#define CMD_GET_PRESET          0x07
#define CMD_SNAPSHOT            0x08
#define CMD_READ_SNAPSHOT_MCA   0x09
#define CMD_READ_SNAPSHOT_STATS 0x0A
#define CMD_READ_SNAPSHOT_SCA   0x0C

/* Diagnostic Tools */
#define CMD_READ_BASELINE       0x10
#define CMD_READ_ADC_TRACE      0x11
#define CMD_READ_BASELINE_HIST  0x12

/* General Communications and Control */
#define CMD_GET_TEMPERATURE     0x41
#define CMD_GET_DSP_PARAM_NAMES 0x42
#define CMD_GET_NUM_PARAM_NAMES 0x42
#define CMD_READ_DSP_PARAMETER  0x43
#define CMD_WRITE_DSP_PARAMETER 0x43
#define CMD_WRITE_DSP_DATA_MEM  0x45
#define CMD_READ_DSP_DATA_MEM   0x45
#define CMD_GET_SERIAL_NUMBER   0x48
#define CMD_GET_BOARD_INFO      0x49
#define CMD_ECHO                0x4A
#define CMD_STATUS              0x4B
#define CMD_REBOOT              0x4F

/* Spectrometer Control */
#define CMD_SET_DIG_CLOCK       0x80
#define CMD_GET_DIG_CLOCK       0x80
#define CMD_SET_FIPPI_CONFIG    0x81
#define CMD_GET_FIPPI_CONFIG    0x81
#define CMD_SET_PARSET          0x82
#define CMD_GET_PARSET          0x82
#define CMD_SET_GENSET          0x83
#define CMD_GET_GENSET          0x83
#define CMD_SET_BIN_WIDTH       0x84
#define CMD_GET_BIN_WIDTH       0x84
#define CMD_SET_NUM_BINS        0x85
#define CMD_GET_NUM_BINS        0x85
#define CMD_SET_THRESHOLD       0x86
#define CMD_GET_THRESHOLD       0x86
#define CMD_SET_POLARITY        0x87
#define CMD_GET_POLARITY        0x87
#define CMD_SET_GAINBASE        0x88
#define CMD_GET_GAINBASE        0x88
#define CMD_GET_RCFEED          0x89
#define CMD_SET_RCFEED          0x89
#define CMD_GET_RESET           0x8A
#define CMD_SET_RESET           0x8A
#define CMD_SET_FILTER_PARAMS   0x8B
#define CMD_GET_FILTER_PARAMS   0x8B
#define CMD_GET_PARSET_VALS     0x8C
#define CMD_SAVE_PARSET         0x8D
#define CMD_GET_GENSET_VALS     0x8E
#define CMD_SAVE_GENSET         0x8F
#define CMD_READ_SLOWLEN_VALS   0x90
#define CMD_SET_GAINTWEAK       0x91
#define CMD_GET_GAINTWEAK       0x91
#define CMD_SET_BLFILTER        0x92
#define CMD_GET_BLFILTER        0x92
#define CMD_SET_RUNTASKS        0x93
#define CMD_GET_RUNTASKS        0x93
#define CMD_SET_FIPCONTROL      0x94
#define CMD_GET_FIPCONTROL      0x94
#define CMD_GET_SCALIMIT        0x97
#define CMD_SET_SCALIMIT        0x97
#define CMD_GET_SWGAIN          0x9B
#define CMD_SET_SWGAIN          0x9B
#define CMD_GET_DIGITALGAIN     0x9C
#define CMD_SET_DIGITALGAIN     0x9C
#define CMD_SET_OFFADC          0x9D
#define CMD_APPLY               0x9F

/* UART passthrough command */
#define CMD_PASSTHROUGH         0xC0

/* XIA Setup Commands */
#define CMD_WRITE_I2C           0xF6
#define CMD_READ_I2C            0xF7
#define CMD_WRITE_FLASH         0xF8
#define CMD_READ_FLASH          0xF9

/* This is a special command number used
 * for situations where I need to call
 * dxp_cmd() with an IO flag set for non-IO
 * operation, like turning on the sniff
 * mode.
 */
#define CMD_NOP                 0xFF

/* Another macro used to reduce the amount of
 * code it takes to define the variables for a
 * RS-232 command. I found myself using a rather
 * consistent format and I was writing a lot of
 * redundant code...
 */
#define DEFINE_CMD(c, ls, lr)                           \
                  unsigned int lenS = (ls);             \
                  unsigned int lenR = (lr) + RECV_BASE; \
                  byte_t cmd = (c);                     \
                  byte_t send[(ls)];                    \
                  byte_t receive[(lr) + RECV_BASE]

#define DEFINE_CMD_ZERO_SEND(c, lr)                     \
                  unsigned int lenS = 0;                \
                  unsigned int lenR = (lr) + RECV_BASE; \
                  byte_t cmd = (c);                     \
                  byte_t receive[(lr) + RECV_BASE]

#define OLD_MICRO_CMD(ls, lr)                           \
                  lenS = (ls);                          \
                  lenR = (lr) + RECV_BASE;

#endif /* _UDXP_COMMAND_H_ */
