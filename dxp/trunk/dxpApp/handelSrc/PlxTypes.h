#ifndef __PLX_TYPES_H
#define __PLX_TYPES_H

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
 *      PlxTypes.h
 *
 * Description:
 *
 *      This file defines the basic types available to the PCI SDK.
 *
 * Revision:
 *
 *      09-01-09 : PLX SDK v6.30
 *
 ******************************************************************************/


#include "Plx.h"
#include "PlxDefCk.h"
#include "PlxStat.h"
#include "PciTypes.h"



#ifdef __cplusplus
extern "C" {
#endif




/******************************************
 *   Definitions for Code Portability
 ******************************************/
/* Convert pointer to an integer */
#define PLX_PTR_TO_INT( ptr )                     ((PLX_UINT_PTR)(ptr))

/* Convert integer to a pointer */
#define PLX_INT_TO_PTR( int )                     ((VOID*)(PLX_UINT_PTR)(int))

/* Macros that guarantee correct endian format regardless of CPU platform */
#if defined(PLX_BIG_ENDIAN)
    #define PLX_LE_DATA_32(value)                 EndianSwap32( (value) )
    #define PLX_BE_DATA_32(value)                 (value)
#else
    #define PLX_LE_DATA_32(value)                 (value)
    #define PLX_BE_DATA_32(value)                 EndianSwap32( (value) )
#endif

/* Macros to support portable type casting on BE/LE platforms */
#if (PLX_SIZE_64 == 4)
    #define PLX_64_HIGH_32(value)                 0
    #define PLX_64_LOW_32(value)                  ((U32)(value))
#else
    #if defined(PLX_BIG_ENDIAN)
        #define PLX_64_HIGH_32(value)             ((U32)(value))
        #define PLX_64_LOW_32(value)              ((U32)((value) >> 32))

        #define PLX_CAST_64_TO_8_PTR( ptr64 )     (U8*) ((U8*)(ptr64) + (7*sizeof(U8)))
        #define PLX_CAST_64_TO_16_PTR( ptr64 )    (U16*)((U8*)(ptr64) + (6*sizeof(U8)))
        #define PLX_CAST_64_TO_32_PTR( ptr64 )    (U32*)((U8*)(ptr64) + sizeof(U32))

        #define PLX_LE_U32_BIT( pos )             ((U32)(1 << (31 - (pos))))
    #else
        #define PLX_64_HIGH_32(value)             ((U32)((value) >> 32))
        #define PLX_64_LOW_32(value)              ((U32)(value))

        #define PLX_CAST_64_TO_8_PTR( ptr64 )     (U8*) (ptr64)
        #define PLX_CAST_64_TO_16_PTR( ptr64 )    (U16*)(ptr64)
        #define PLX_CAST_64_TO_32_PTR( ptr64 )    (U32*)(ptr64)

        #define PLX_LE_U32_BIT( pos )             ((U32)(1 << (pos)))
    #endif
#endif



/******************************************
 *      Miscellaneous definitions
 ******************************************/
#if !defined(VOID)
    typedef void              VOID;
#endif

#if (!defined(PLX_MSWINDOWS)) || defined(PLX_VXD_DRIVER)
    #if !defined(BOOLEAN)
        typedef S8            BOOLEAN;
    #endif
#endif

#if !defined(PLX_MSWINDOWS)
    #if !defined(BOOL)
        typedef S8            BOOL;
    #endif
#endif

#if !defined(NULL)
    #define NULL              ((VOID *) 0x0)
#endif

#if !defined(TRUE)
    #define TRUE              1
#endif

#if !defined(FALSE)
    #define FALSE             0
#endif

#if defined(PLX_MSWINDOWS) 
    #define PLX_TIMEOUT_INFINITE        INFINITE
#elif defined(PLX_LINUX) || defined(PLX_LINUX_DRIVER)
    #define PLX_TIMEOUT_INFINITE        MAX_SCHEDULE_TIMEOUT

    /*********************************************************
     * Convert milliseconds to jiffies.  The following
     * formula is used:
     *
     *                      ms * HZ
     *           jiffies = ---------
     *                       1,000
     *
     *  where:  HZ      = System-defined clock ticks per second
     *          ms      = Timeout in milliseconds
     *          jiffies = Number of HZ's per second
     ********************************************************/
    #define Plx_ms_to_jiffies( ms )     ( ((ms) * HZ) / 1000 )
#endif




/******************************************
 *   PLX-specific types & structures
 ******************************************/

/* Mode PLX API uses to access device */
typedef enum _PLX_API_MODE
{
    PLX_API_MODE_PCI,                   /* Device accessed via PLX driver over PCI/PCIe */
    PLX_API_MODE_I2C_AARDVARK,          /* Device accessed via Aardvark I2C USB */
    PLX_API_MODE_TCP                    /* Device accessed via TCP/IP */
} PLX_API_MODE;


/* Access Size Type */
typedef enum _PLX_ACCESS_TYPE
{
    BitSize8,
    BitSize16,
    BitSize32,
    BitSize64
} PLX_ACCESS_TYPE;


/* Generic states used internally by PLX software */
typedef enum _PLX_STATE
{
    PLX_STATE_ENABLED,
    PLX_STATE_DISABLED,
    PLX_STATE_IDLE,
    PLX_STATE_BUSY,
    PLX_STATE_STARTED,
    PLX_STATE_STARTING,
    PLX_STATE_STOPPED,
    PLX_STATE_STOPPING,
    PLX_STATE_DELETED,
    PLX_STATE_MARKED_FOR_DELETE,
    PLX_STATE_OK_TO_DELETE,
    PLX_STATE_TRIGGERED,
    PLX_STATE_WAITING
} PLX_STATE;


/* EEPROM status */
typedef enum _PLX_EEPROM_STATUS
{
    PLX_EEPROM_STATUS_NONE         = 0,     /* Not present */
    PLX_EEPROM_STATUS_VALID        = 1,     /* Present with valid data */
    PLX_EEPROM_STATUS_INVALID_DATA = 2,     /* Present w/invalid data or CRC error */
    PLX_EEPROM_STATUS_BLANK        = PLX_EEPROM_STATUS_INVALID_DATA,
    PLX_EEPROM_STATUS_CRC_ERROR    = PLX_EEPROM_STATUS_INVALID_DATA
} PLX_EEPROM_STATUS;


/* PCI Express Link Speeds */
typedef enum _PLX_LINK_SPEED
{
    PLX_LINK_SPEED_2_5_GBPS     = 1,
    PLX_LINK_SPEED_5_0_GBPS     = 2,
    PLX_LINK_SPEED_8_0_GBPS     = 3,
    PLX_PCIE_GEN_1_0            = PLX_LINK_SPEED_2_5_GBPS,
    PLX_PCIE_GEN_2_0            = PLX_LINK_SPEED_5_0_GBPS,
    PLX_PCIE_GEN_3_0            = PLX_LINK_SPEED_8_0_GBPS
} PLX_LINK_SPEED;


/* Port types */
typedef enum _PLX_PORT_TYPE
{
    PLX_PORT_UNKNOWN            = 0xFF,
    PLX_PORT_ENDPOINT           = 0,
    PLX_PORT_LEGACY_ENDPOINT    = 1,
    PLX_PORT_NON_TRANS          = PLX_PORT_ENDPOINT,  /* NT port is an endpoint */
    PLX_PORT_ROOT_PORT          = 4,
    PLX_PORT_UPSTREAM           = 5,
    PLX_PORT_DOWNSTREAM         = 6,
    PLX_PORT_PCIE_TO_PCI_BRIDGE = 7,
    PLX_PORT_PCI_TO_PCIE_BRIDGE = 8,
    PLX_PORT_ROOT_ENDPOINT      = 9,
    PLX_PORT_ROOT_EVENT_COLL    = 10
} PLX_PORT_TYPE;


/* Non-transparent Port types */
typedef enum _PLX_NT_PORT_TYPE
{
    PLX_NT_PORT_NONE            = 0,                       /* Not an NT port */
    PLX_NT_PORT_PRIMARY         = 1,                       /* NT Primary Host side port */
    PLX_NT_PORT_SECONDARY       = 2,                       /* NT Seconday Host side port */
    PLX_NT_PORT_VIRTUAL         = PLX_NT_PORT_PRIMARY,     /* NT Virtual-side port */
    PLX_NT_PORT_LINK            = PLX_NT_PORT_SECONDARY,   /* NT Link-side port */
    PLX_NT_PORT_UNKOWN          = 0xFF                     /* NT side is undetermined */
} PLX_NT_PORT_TYPE;


/* DMA control commands */
typedef enum _PLX_DMA_COMMAND
{
    DmaPause,
    DmaPauseImmediate,
    DmaResume,
    DmaAbort
} PLX_DMA_COMMAND;


/* DMA transfer direction */
typedef enum _PLX_DMA_DIR
{
    PLX_DMA_PCI_TO_LOC         = 0,                       /* DMA PCI --> Local bus   (9000 DMA) */
    PLX_DMA_LOC_TO_PCI         = 1,                       /* DMA Local bus --> PCI   (9000 DMA) */
    PLX_DMA_USER_TO_PCI        = PLX_DMA_PCI_TO_LOC,      /* DMA User buffer --> PCI (8600 DMA) */
    PLX_DMA_PCI_TO_USER        = PLX_DMA_LOC_TO_PCI       /* DMA PCI --> User buffer (8600 DMA) */
} PLX_DMA_DIR;


/* DMA Descriptor Mode */
typedef enum _PLX_DMA_DESCR_MODE
{
    PLX_DMA_MODE_BLOCK         = 0,                      /* DMA Block transfer mode */
    PLX_DMA_MODE_SGL           = 1                       /* DMA SGL transfer mode */
} PLX_DMA_DESCR_MODE;


/* DMA Ring Delay Period */
typedef enum _PLX_DMA_RING_DELAY_TIME
{
    PLX_DMA_RING_DELAY_0       = 0,
    PLX_DMA_RING_DELAY_1us     = 1,
    PLX_DMA_RING_DELAY_2us     = 2,
    PLX_DMA_RING_DELAY_8us     = 3,
    PLX_DMA_RING_DELAY_32us    = 4,
    PLX_DMA_RING_DELAY_128us   = 5,
    PLX_DMA_RING_DELAY_512us   = 6,
    PLX_DMA_RING_DELAY_1ms     = 7
} PLX_DMA_RING_DELAY_TIME;


/* DMA Maximum Source Transfer Size */
typedef enum _PLX_DMA_MAX_SRC_TSIZE
{
    PLX_DMA_MAX_SRC_TSIZE_64B  = 0,
    PLX_DMA_MAX_SRC_TSIZE_128B = 1,
    PLX_DMA_MAX_SRC_TSIZE_256B = 2,
    PLX_DMA_MAX_SRC_TSIZE_512B = 3,
    PLX_DMA_MAX_SRC_TSIZE_1K   = 4,
    PLX_DMA_MAX_SRC_TSIZE_2K   = 5,
    PLX_DMA_MAX_SRC_TSIZE_4K   = 7
} PLX_DMA_SRC_MAX_TSIZE;


/* Performance monitor control */
typedef enum _PLX_PERF_CMD
{
    PLX_PERF_CMD_START,
    PLX_PERF_CMD_STOP,
} PLX_PERF_CMD;


/* DMA Maximum Source Transfer Size */
typedef enum _PLX_SWITCH_MODE
{
    PLX_SWITCH_MODE_STANDARD   = 0,
    PLX_SWITCH_MODE_MULTI_HOST = 2
} PLX_SWITCH_MODE;


/* Used for device power state. Added for code compatability with Linux */
#if !defined(PLX_MSWINDOWS) 
    typedef enum _DEVICE_POWER_STATE
    {
        PowerDeviceUnspecified = 0,
        PowerDeviceD0,
        PowerDeviceD1,
        PowerDeviceD2,
        PowerDeviceD3,
        PowerDeviceMaximum
    } DEVICE_POWER_STATE;
#endif


/* Properties of API access mode */
typedef struct _PLX_MODE_PROP
{
    union
    {
        struct
        {
            U16 I2cPort;
            U16 SlaveAddr;
            U32 ClockRate;
        } I2c;

        struct
        {
            U64 IpAddress;
        } Tcp;
    };
} PLX_MODE_PROP;


/* PLX version information */
typedef struct _PLX_VERSION
{
    PLX_API_MODE ApiMode;

    union
    {
        struct
        {
            U16 ApiLibrary;     /* API library version */
            U16 Software;       /* Software version */
            U16 Firmware;       /* Firmware version */
            U16 Hardware;       /* Hardware version */
            U16 SwReqByFw;      /* Firmware requires software must be >= this version */
            U16 FwReqBySw;      /* Software requires firmware must be >= this version */
            U16 ApiReqBySw;     /* Software requires API interface must be >= this version */
            U32 Features;       /* Bitmask of supported features */
        } I2c;
    };
} PLX_VERSION;


/* PCI Memory Structure */
typedef struct _PLX_PHYSICAL_MEM
{
    U64 UserAddr;                    /* User-mode virtual address */
    U64 PhysicalAddr;                /* Bus physical address */
    U64 CpuPhysical;                 /* CPU physical address */
    U32 Size;                        /* Size of the buffer */
} PLX_PHYSICAL_MEM;


/* PLX Driver Properties */
typedef struct _PLX_DRIVER_PROP
{
    char    DriverName[16];          /* Name of driver */
    BOOLEAN bIsServiceDriver;        /* Is service driver or PnP driver? */
    U64     AcpiPcieEcam;            /* Base address of PCIe ECAM */
    U8      Reserved[40];            /* Reserved for future use */
} PLX_DRIVER_PROP;


/* PCI BAR Properties */
typedef struct _PLX_PCI_BAR_PROP
{
    U32      BarValue;               /* Actual value in BAR */
    U64      Physical;               /* BAR Physical Address */
    U64      Size;                   /* Size of BAR space */
    BOOLEAN  bIoSpace;               /* Memory or I/O space? */
    BOOLEAN  bPrefetchable;          /* Is space pre-fetchable? */
    BOOLEAN  b64bit;                 /* Is PCI BAR 64-bit? */
} PLX_PCI_BAR_PROP;


/* Used for getting the port properties and status */
typedef struct _PLX_PORT_PROP
{
    U8      PortType;                /* Port configuration */
    U8      PortNumber;              /* Internal port number */
    U8      LinkWidth;               /* Negotiated port link width */
    U8      MaxLinkWidth;            /* Max link width device is capable of */
    U8      LinkSpeed;               /* Negotiated link speed */
    U8      MaxLinkSpeed;            /* Max link speed device is capable of */
    U16     MaxReadReqSize;          /* Max read request size allowed */
    U16     MaxPayloadSize;          /* Max payload size setting */
    BOOLEAN bNonPcieDevice;          /* Flag whether device is a PCIe device */
} PLX_PORT_PROP;


/* Used for getting the multi-host switch properties */
typedef struct _PLX_MULTI_HOST_PROP
{
    U8      SwitchMode;              /* Current switch mode */
    U16     VS_EnabledMask;          /* Bit for each enabled Virtual Switch */
    U8      VS_UpstreamPortNum[8];   /* Upstream port number of each Virtual Switch */
    U32     VS_DownstreamPorts[8];   /* Downstream ports associated with a Virtual Switch */
    BOOLEAN bIsMgmtPort;             /* Is selected device management port */
    BOOLEAN bMgmtPortActiveEn;       /* Is active management port enabled */
    U8      MgmtPortNumActive;       /* Active management port */
    BOOLEAN bMgmtPortRedundantEn;    /* Is redundant management port enabled */
    U8      MgmtPortNumRedundant;    /* Redundant management port */
} PLX_MULTI_HOST_PROP;


/* PCI Device Key Identifier */
typedef struct _PLX_DEVICE_KEY
{
    U32 IsValidTag;                  /* Magic number to determine validity */
    U8  bus;                         /* Physical device location */
    U8  slot;
    U8  function;
    U16 VendorId;                    /* Device Identifier */
    U16 DeviceId;
    U16 SubVendorId;
    U16 SubDeviceId;
    U8  Revision;
    U16 PlxChip;                     /* PLX chip type */
    U8  PlxRevision;                 /* PLX chip revision */
    U8  ApiIndex;                    /* Used internally by the API */
    U8  DeviceNumber;                /* Used internally by device drivers */
    U8  ApiMode;                     /* Mode API uses to access device */
    U8  PlxPort;                     /* PLX port number of device */
    U8  NTPortType;                  /* If NT port, stores NT port type */
    U8  NTPortNum;                   /* If NT port exists, store NT port number */
    U32 ApiInternal[2];              /* Reserved for internal PLX API use */
} PLX_DEVICE_KEY;


/* PLX Device Object Structure */
typedef struct _PLX_DEVICE_OBJECT
{
    U32               IsValidTag;    /* Magic number to determine validity */
    PLX_DEVICE_KEY    Key;           /* Device location key identifier */
    PLX_DRIVER_HANDLE hDevice;       /* Handle to driver */
    PLX_PCI_BAR_PROP  PciBar[6];     /* PCI BAR properties */
    U64               PciBarVa[6];   /* For PCI BAR user-mode BAR mappings */
    U8                BarMapRef[6];  /* BAR map count used by API */
    PLX_PHYSICAL_MEM  CommonBuffer;  /* Used to store common buffer information */
    U32               Reserved[8];   /* Reserved for future use */
} PLX_DEVICE_OBJECT;


/* PLX Notification Object */
typedef struct _PLX_NOTIFY_OBJECT
{
    U32 IsValidTag;                  /* Magic number to determine validity */
    U64 pWaitObject;                 /* -- INTERNAL -- Wait object used by the driver */
    U64 hEvent;                      /* User event handle (HANDLE can be 32 or 64 bit) */
} PLX_NOTIFY_OBJECT;


/* PLX Interrupt Structure  */
typedef struct _PLX_INTERRUPT
{
    U32 Doorbell;                    /* Up to 32 doorbells */
    U8  PciMain                :1;
    U8  PciAbort               :1;
    U8  LocalToPci             :2;   /* Local->PCI interrupts 1 & 2 */
    U8  DmaDone                :4;   /* DMA channel 0-3 interrrupts */
    U8  DmaPauseDone           :4;
    U8  DmaAbortDone           :4;
    U8  DmaImmedStopDone       :4;
    U8  DmaInvalidDescr        :4;
    U8  DmaError               :4;
    U8  MuInboundPost          :1;
    U8  MuOutboundPost         :1;
    U8  MuOutboundOverflow     :1;
    U8  TargetRetryAbort       :1;
    U8  Message                :4;   /* 6000 NT 0-3 message interrupts */
    U8  SwInterrupt            :1;
    U8  ResetDeassert          :1;
    U8  PmeDeassert            :1;
    U8  GPIO_4_5               :1;
    U8  GPIO_14_15             :1;
    U8  NTV_LE_Correctable     :1;   /* NT Virtual - Link-side Error interrupts */
    U8  NTV_LE_Uncorrectable   :1;
    U8  NTV_LE_LinkStateChange :1;
    U8  NTV_LE_UncorrErrorMsg  :1;
    U8  HotPlugAttention       :1;
    U8  HotPlugPowerFault      :1;
    U8  HotPlugMrlSensor       :1;
    U8  HotPlugChangeDetect    :1;
    U8  HotPlugCmdCompleted    :1;
} PLX_INTERRUPT;


/* DMA Channel Properties Structure */
typedef struct _PLX_DMA_PROP
{
    /* 8600 DMA properties */
    U8  CplStatusWriteBack  :1;
    U8  DescriptorMode      :1;
    U8  DescriptorPollMode  :1;
    U8  RingHaltAtEnd       :1;
    U8  RingWrapDelayTime   :3;
    U8  RelOrderDescrRead   :1;
    U8  RelOrderDescrWrite  :1;
    U8  RelOrderDataReadReq :1;
    U8  RelOrderDataWrite   :1;
    U8  NoSnoopDescrRead    :1;
    U8  NoSnoopDescrWrite   :1;
    U8  NoSnoopDataReadReq  :1;
    U8  NoSnoopDataWrite    :1;
    U8  MaxSrcXferSize      :3;
    U8  TrafficClass        :3;
    U8  MaxPendingReadReq   :6;
    U8  DescriptorPollTime;
    U8  MaxDescriptorFetch;
    U16 ReadReqDelayClocks;

    /* 9000 DMA properties */
    U8  ReadyInput          :1;
    U8  Burst               :1;
    U8  BurstInfinite       :1;
    U8  SglMode             :1;
    U8  DoneInterrupt       :1;
    U8  RouteIntToPci       :1;
    U8  ConstAddrLocal      :1;
    U8  WriteInvalidMode    :1;
    U8  DemandMode          :1;
    U8  EnableEOT           :1;
    U8  FastTerminateMode   :1;
    U8  ClearCountMode      :1;
    U8  DualAddressMode     :1;
    U8  EOTEndLink          :1;
    U8  ValidMode           :1;
    U8  ValidStopControl    :1;
    U8  LocalBusWidth       :2;
    U8  WaitStates          :4;
} PLX_DMA_PROP;


/* DMA Transfer Parameters */
typedef struct _PLX_DMA_PARAMS
{
    U64         UserVa;             /* User buffer virtual address */
    U64         AddrSource;         /* Source address      (8600 DMA) */
    U64         AddrDest;           /* Destination address (8600 DMA) */
    U64         PciAddr;            /* PCI address         (9000 DMA) */
    U32         LocalAddr;          /* Local bus address   (9000 DMA) */
    U32         ByteCount;          /* Number of bytes to transfer */
    PLX_DMA_DIR Direction;          /* Direction of transfer (Local<->PCI, User<->PCI) (9000 DMA) */
    U8          bConstAddrSrc   :1; /* Constant source PCI address?      (8600 DMA) */
    U8          bConstAddrDest  :1; /* Constant destination PCI address? (8600 DMA) */
    U8          bForceFlush     :1; /* Force DMA to flush write on final descriptor (8600 DMA) */
    U8          bIgnoreBlockInt :1; /* For block mode only, do not enable DMA done interrupt */
} PLX_DMA_PARAMS;


/* Performance properties */
typedef struct _PLX_PERF_PROP
{
    U32 IsValidTag;   /* Magic number to determine validity */

    /* Port properties */
    U8  PortNumber;
    U8  LinkWidth;
    U8  LinkSpeed;
    U8  Station;
    U8  StationPort;

    /* Ingress counters */
    U32 IngressPostedHeader;
    U32 IngressPostedDW;
    U32 IngressNonpostedDW;
    U32 IngressCplHeader;
    U32 IngressCplDW;
    U32 IngressDllp;
    U32 IngressPhy;

    /* Egress counters */
    U32 EgressPostedHeader;
    U32 EgressPostedDW;
    U32 EgressNonpostedDW;
    U32 EgressCplHeader;
    U32 EgressCplDW;
    U32 EgressDllp;
    U32 EgressPhy;

    /* Storage for previous counter values */

    /* Previous Ingress counters */
    U32 Prev_IngressPostedHeader;
    U32 Prev_IngressPostedDW;
    U32 Prev_IngressNonpostedDW;
    U32 Prev_IngressCplHeader;
    U32 Prev_IngressCplDW;
    U32 Prev_IngressDllp;
    U32 Prev_IngressPhy;

    /* Previous Egress counters */
    U32 Prev_EgressPostedHeader;
    U32 Prev_EgressPostedDW;
    U32 Prev_EgressNonpostedDW;
    U32 Prev_EgressCplHeader;
    U32 Prev_EgressCplDW;
    U32 Prev_EgressDllp;
    U32 Prev_EgressPhy;
} PLX_PERF_PROP;


/* Performance statistics */
typedef struct _PLX_PERF_STATS
{
    S64         IngressTotalBytes;              /* Total bytes including overhead */
    long double IngressTotalByteRate;           /* Total byte rate */
    S64         IngressCplAvgPerReadReq;        /* Average number of completion TLPs for read requests */
    S64         IngressCplAvgBytesPerTlp;       /* Average number of bytes per completion TLPs */
    S64         IngressPayloadReadBytes;        /* Payload bytes read (Completion TLPs) */
    S64         IngressPayloadReadBytesAvg;     /* Average payload bytes for reads (Completion TLPs) */
    S64         IngressPayloadWriteBytes;       /* Payload bytes written (Posted TLPs) */
    S64         IngressPayloadWriteBytesAvg;    /* Average payload bytes for writes (Posted TLPs) */
    S64         IngressPayloadTotalBytes;       /* Payload total bytes */
    double      IngressPayloadAvgPerTlp;        /* Payload average size per TLP */
    long double IngressPayloadByteRate;         /* Payload byte rate */
    long double IngressLinkUtilization;         /* Total link utilization */

    S64         EgressTotalBytes;               /* Total byte including overhead */
    long double EgressTotalByteRate;            /* Total byte rate */
    S64         EgressCplAvgPerReadReq;         /* Average number of completion TLPs for read requests */
    S64         EgressCplAvgBytesPerTlp;        /* Average number of bytes per completion TLPs */
    S64         EgressPayloadReadBytes;         /* Payload bytes read (Completion TLPs) */
    S64         EgressPayloadReadBytesAvg;      /* Average payload bytes for reads (Completion TLPs) */
    S64         EgressPayloadWriteBytes;        /* Payload bytes written (Posted TLPs) */
    S64         EgressPayloadWriteBytesAvg;     /* Average payload bytes for writes (Posted TLPs) */
    S64         EgressPayloadTotalBytes;        /* Payload total bytes */
    double      EgressPayloadAvgPerTlp;         /* Payload average size per TLP */
    long double EgressPayloadByteRate;          /* Payload byte rate */
    long double EgressLinkUtilization;          /* Total link utilization */
} PLX_PERF_STATS;




#ifdef __cplusplus
}
#endif

#endif
