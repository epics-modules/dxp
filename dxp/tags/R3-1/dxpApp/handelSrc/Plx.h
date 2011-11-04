#ifndef __PLX_H
#define __PLX_H

/*******************************************************************************
 * Copyright (c) PLX Technology, Inc.
 *
 * PLX Technology Inc. licenses this source file under the GNU Lesser General Public
 * License (LGPL) version 2.  This source file may be modified or redistributed
 * under the terms of the LGPL and without express permission from PLX Technology.
 *
 * PLX Technology, Inc. provides this software AS IS, WITHOUT ANY WARRANTY,
 * EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, ANY WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  PLX makes no guarantee
 * or representations regarding the use of, or the results of the use of,
 * the software and documentation in terms of correctness, accuracy,
 * reliability, currentness, or otherwise; and you rely on the software,
 * documentation and results solely at your own risk.
 *
 * IN NO EVENT SHALL PLX BE LIABLE FOR ANY LOSS OF USE, LOSS OF BUSINESS,
 * LOSS OF PROFITS, INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES
 * OF ANY KIND.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * File Name:
 *
 *      Plx.h
 *
 * Description:
 *
 *      This file contains definitions that are common to all PCI SDK code
 *
 * Revision:
 *
 *      09-01-09 : PLX SDK v6.30
 *
 ******************************************************************************/


#ifdef __cplusplus
extern "C" {
#endif




/**********************************************
*               Definitions
**********************************************/
/* SDK Version information */
#define PLX_SDK_VERSION_MAJOR            6
#define PLX_SDK_VERSION_MINOR            3
#define PLX_SDK_VERSION_REVISION         1
#define PLX_SDK_VERSION_STRING           "6.31"
#define PLX_SDK_COPYRIGHT_STRING         "\251 PLX Technology, Inc. 2009"

#define MAX_PCI_BUS                      255            /* Max PCI Buses */
#define MAX_PCI_DEV                      32             /* Max PCI Slots */
#define MAX_PCI_FUNC                     8              /* Max PCI Functions */
#define PCI_NUM_BARS_TYPE_00             6              /* Total PCI BARs for Type 0 Header */
#define PCI_NUM_BARS_TYPE_01             2              /* Total PCI BARs for Type 1 Header */

#define PLX_VENDOR_ID                    0x10B5         /* PLX Vendor ID */

/* Device object validity codes */
#define PLX_TAG_VALID                    0x5F504C58     /* "_PLX" in Hex */
#define PLX_TAG_INVALID                  0x564F4944     /* "VOID" in Hex */
#define ObjectValidate(pObj)             ((pObj)->IsValidTag = PLX_TAG_VALID)
#define ObjectInvalidate(pObj)           ((pObj)->IsValidTag = PLX_TAG_INVALID)
#define IsObjectValid(pObj)              ((pObj)->IsValidTag == PLX_TAG_VALID)

/* Used for locating PCI devices */
#define PCI_FIELD_IGNORE                 (-1)

/* Constants for CRC status */
#define PLX_CRC_VALID                    TRUE
#define PLX_CRC_INVALID                  FALSE

/* Used for VPD accesses */
#define VPD_COMMAND_MAX_RETRIES          5         /* Max number VPD command re-issues */
#define VPD_STATUS_MAX_POLL              10        /* Max number of times to read VPD status */
#define VPD_STATUS_POLL_DELAY            5         /* Delay between polling VPD status (Milliseconds) */

/* Define a large value for a signal to the driver */
#define FIND_AMOUNT_MATCHED              80001

/* Used for performance counter calculations */
#define PERF_DW_SIZE                     sizeof(U32)                    /* Size of a DWord */
#define PERF_TLP_OH_DW                   2                              /* Overhead DW per TLP */
#define PERF_TLP_DW                      (3 + PERF_TLP_OH_DW)           /* DW per TLP */
#define PERF_TLP_SIZE                    (PERF_TLP_DW * PERF_DW_SIZE)   /* Bytes per TLP w/o payload */
#define PERF_DLLP_SIZE                   (2 * PERF_DW_SIZE)             /* Bytes per DLLP */
#define PERF_MAX_RATE_2_5_GBPS           ((U64)2000000000)              /* 2.5Gbps * 80% */
#define PERF_MAX_STATIONS                3                              /* Max number of stations */
#define PERF_PORTS_PER_STATION           4                              /* Max ports per station */

/* Used for EEPROM file read/write */
#define EndianSwap32(value)              ( ((((value) >>  0) & 0xff) << 24) | \
                                           ((((value) >>  8) & 0xff) << 16) | \
                                           ((((value) >> 16) & 0xff) <<  8) | \
                                           ((((value) >> 24) & 0xff) <<  0) )

#define EndianSwap16(value)              ( ((((value) >>  0) & 0xffff) << 16) | \
                                           ((((value) >> 16) & 0xffff) <<  0) )

/* PLX 9000-series doorbell value to initiate a local CPU reset */
#define PLX_RESET_EMBED_INT              ((unsigned long)1 << 31)

/* Device IDs of PLX reference boards */
#define PLX_9080RDK_960_DEVICE_ID        0x0960
#define PLX_9080RDK_401B_DEVICE_ID       0x0401
#define PLX_9080RDK_860_DEVICE_ID        0x0860
#define PLX_9054RDK_860_DEVICE_ID        0x1860
#define PLX_9054RDK_LITE_DEVICE_ID       0x5406
#define PLX_CPCI9054RDK_860_DEVICE_ID    0xC860
#define PLX_9056RDK_LITE_DEVICE_ID       0x5601
#define PLX_9056RDK_860_DEVICE_ID        0x56c2
#define PLX_9656RDK_LITE_DEVICE_ID       0x9601
#define PLX_9656RDK_860_DEVICE_ID        0x96c2
#define PLX_9030RDK_LITE_DEVICE_ID       0x3001
#define PLX_CPCI9030RDK_LITE_DEVICE_ID   0x30c1
#define PLX_9050RDK_LITE_DEVICE_ID       0x9050
#define PLX_9052RDK_LITE_DEVICE_ID       0x5201



#ifdef __cplusplus
}
#endif

#endif
