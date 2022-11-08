/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2020 XIA LLC
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


#ifndef XERXES_ERROR_H
#define XERXES_ERROR_H

#define DXP_SUCCESS            0

/* I/O level error codes 4001-4100 */
#define DXP_MDOPEN             4001 /* Error opening port for communication */
#define DXP_MDIO               4002 /* Device IO error */
#define DXP_MDINITIALIZE       4003 /* Error configurting port during initialization */
#define DXP_MDSIZE             4007 /* Incoming data packet length larger than requested */
#define DXP_MDUNKNOWN          4009 /* Unknown IO function */
#define DXP_MDCLOSE            4010 /* Error closing connection */
#define DXP_MDINVALIDPRIORITY  4012 /* Invalid priority type */
#define DXP_MDPRIORITY         4013 /* Error setting priority */
#define DXP_MDINVALIDNAME      4014 /* Error parsing IO name */
#define DXP_MD_TARGET_ADDR     4018 /* Invalid target address */
#define DXP_OPEN_EPP           4019 /* Unable to open EPP port */
#define DXP_BAD_IONAME         4020 /* Invalid io name format */
#define DXP_UNKONWN_BAUD       4020 /* Unknown baud rate */

/* Saturn specific error codes */
#define DXP_WRITE_TSAR       4101 /* Error writing TSAR register */
#define DXP_WRITE_CSR        4102 /* Error writing CSR register */
#define DXP_WRITE_WORD       4103 /* Error writing single word */
#define DXP_READ_WORD        4104 /* Error reading single word */
#define DXP_WRITE_BLOCK      4105 /* Error writing block data */
#define DXP_READ_BLOCK       4106 /* Error reading block data */
#define DXP_READ_CSR         4110 /* Error reading CSR register */
#define DXP_WRITE_FIPPI      4111 /* Error writing to FIPPI */
#define DXP_WRITE_DSP        4112 /* Error writing to DSP */
#define DXP_DSPSLEEP		 4115 /* Error with DSP Sleep */
#define DXP_NOFIPPI			 4116 /* No valid FiPPI defined */
#define DXP_FPGADOWNLOAD     4117 /* Error downloading FPGA */
#define DXP_DSPLOAD          4118 /* Error downloading DSP */
#define DXP_DSPACCESS        4119 /* Specified DSP parameter is read-only */
#define DXP_DSPPARAMBOUNDS   4120 /* DSP parameter value is out of range */
#define DXP_NOCONTROLTYPE    4122 /* Unknown control task */
#define DXP_CONTROL_TASK     4123 /* Control task parameter error */

/* microDXP errors */
#define DXP_STATUS_ERROR     4140 /* microDXP device status error */
#define DXP_DSP_ERROR        4141 /* microDXP DSP status error */
#define DXP_PIC_ERROR        4142 /* microDXP PIC status error */
#define DXP_UDXP_RESPONSE    4143 /* microDXP response error */
#define DXP_MISSING_ESC      4144 /* microDXP response is missing ESCAPE char */

/* DSP/FIPPI level error codes 4201-4300 */
#define DXP_DSPRUNERROR      4201 /* Error decoding run error */
#define DXP_NOSYMBOL         4202 /* Unknown DSP parameter */
#define DXP_TIMEOUT          4203 /* Time out waiting for DSP to be ready */
#define DXP_DSP_RETRY        4204 /* DSP failed to download after multiple attempts */
#define DXP_CHECKSUM         4205 /* Error verifing checksum */
#define DXP_BAD_BIT          4206 /* Requested bit is out-of-range */
#define DXP_RUNACTIVE		 4207 /* Run already active in module */
#define DXP_INVALID_STRING   4208 /* Invalid memory string format */
#define DXP_UNIMPLEMENTED    4209 /* Requested function is unimplemented */
#define DXP_MEMORY_LENGTH    4210 /* Invalid memory length */
#define DXP_MEMORY_BLK_SIZE  4211 /* Invalid memory block size */
#define DXP_UNKNOWN_MEM      4212 /* Unknown memory type */
#define DXP_UNKNOWN_FPGA     4213 /* Unknown FPGA type */
#define DXP_FPGA_TIMEOUT     4214 /* Time out downloading FPGA */
#define DXP_APPLY_STATUS     4215 /* Error applying change */
#define DXP_INVALID_LENGTH   4216 /* Specified length is invalid */
#define DXP_NO_SCA           4217 /* No SCAs defined for the specified channel */
#define DXP_FPGA_CRC         4218 /* CRC error after FPGA downloaded */
#define DXP_UNKNOWN_REG      4219 /* Unknown register */
#define DXP_OPEN_FILE        4220 /* UNable to open firmware file */
#define DXP_REWRITE_FAILURE  4221 /* Couldn't set parameter even after n iterations. */

/* Xerxes onfiguration errors 4301-4400 */
#define DXP_BAD_SYSTEMITEM   4301 /* Invalid system item format */
#define DXP_MAX_MODULES      4302 /* Too many modules specified */
#define DXP_NODETCHAN        4303 /* Detector channel is unknown */
#define DXP_NOIOCHAN         4304 /* IO Channel is unknown */
#define DXP_NOMODCHAN        4305 /* Modchan is unknown */
#define DXP_NEGBLOCKSIZE     4306 /* Negative block size unsupported */
#define DXP_INITIALIZE	     4307 /* Initialization error */
#define DXP_UNKNOWN_BTYPE    4308 /* Unknown board type */
#define DXP_BADCHANNEL       4309 /* Invalid channel number */
#define DXP_NULL             4310 /* Parameter cannot be NULL */
#define DXP_MALFORMED_FILE   4311 /* Malformed firmware file */
#define DXP_UNKNOWN_CT       4312 /* Unknown control task */

/* Host machine error codes 4401-4500 */
#define DXP_NOMEM            4401 /* Error allocating memory */
#define DXP_WIN32_API        4402 /* Windows API error */

/* Misc error codes 501-600 */
#define DXP_LOG_LEVEL		 4501 /* Log level invalid */

#endif /* XERXES_ERRORS_H */
