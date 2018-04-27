/*
 * The PLX API (and all code from the SDK) is
 * Copyright (c) 2003 PLX Technology Inc
 *
 * Copyright (c) 2004 X-ray Instrumentation Associates
 * Copyright (c) 2005-2017 XIA LLC
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
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef _WIN32
/* The PLX headers include wtypes.h manually which gets around
 * WIN32_LEAN_AND_MEAN's exclusion of rpc.h, leading to the C4115
 * warning on VS6.
 */
#pragma warning(disable : 4115)
#pragma warning(disable : 4214)
#pragma warning(disable : 4201)
#endif /* _WIN32 */

/* Headers from PLX SDK */
#include "PlxApi.h"

/* The PlxApi.h file causes _WIN32 to be defined on Cygwin. */
#ifdef CYGWIN32
#undef _WIN32
#endif /* CYGWIN32 */

#ifdef _WIN32
#pragma warning(default : 4201)
#pragma warning(default : 4214)
#pragma warning(default : 4115)
#endif /* _WIN32 */

#include "Dlldefs.h"
#include "xia_common.h"
#include "xia_assert.h"

#if !defined(NDEBUG) && defined(__GNUC__)
#define PLXLIB_DEBUG
#elif defined(_DEBUG)
#define PLXLIB_DEBUG
#endif /* (!NDEBUG && __GNUC__) || _DEBUG */

#include "plxlib.h"
#include "plxlib_errors.h"


/* In milliseconds. Here are the assumptions for this calculation:
 * 1) Maximum memory read could be 1M x 32 bits, which is 4MB.
 * 2) A good transfer rate for us is 80 MB/s.
 * 3) Let's play worst case and say 50 MB/s.
 * 4 MB / 50 MB/s -> 40 ms.
 */
#define MAX_BURST_TIMEOUT 40

static void _plx_log_DEBUG(char *msg, ...);
static void _plx_print_more(PLX_STATUS err);

static int _plx_find_handle_index(HANDLE h, unsigned long *idx);
static int _plx_add_slot_to_map(PLX_DEVICE_OBJECT *device);
static int _plx_remove_slot_from_map(unsigned long idx);
static int _plx_resize_map(void);

static FILE *LOG_FILE = NULL;

static virtual_map_t V_MAP =
{
    NULL, NULL, 0
};


/*
 * This table maps PLX error codes to error strings.
 */
API_ERRORS ApiErrors[] =
{
    { ApiSuccess,                   "ApiSuccess"                   },
    { ApiFailed,                    "ApiFailed"                    },
    { ApiNullParam,                 "ApiNullParam"                 },
    { ApiUnsupportedFunction,       "ApiUnsupportedFunction"       },
    { ApiNoActiveDriver,            "ApiNoActiveDriver"            },
    { ApiConfigAccessFailed,        "ApiConfigAccessFailed"        },
    { ApiInvalidDeviceInfo,         "ApiInvalidDeviceInfo"         },
    { ApiInvalidDriverVersion,      "ApiInvalidDriverVersion"      },
    { ApiInvalidOffset,             "ApiInvalidOffset"             },
    { ApiInvalidData,               "ApiInvalidData"               },
    { ApiInvalidSize,               "ApiInvalidSize"               },
    { ApiInvalidAddress,            "ApiInvalidAddress"            },
    { ApiInvalidAccessType,         "ApiInvalidAccessType"         },
    { ApiInvalidIndex,              "ApiInvalidIndex"              },
    { ApiInvalidPowerState,         "ApiInvalidPowerState"         },
    { ApiInvalidIopSpace,           "ApiInvalidIopSpace"           },
    { ApiInvalidHandle,             "ApiInvalidHandle"             },
    { ApiInvalidPciSpace,           "ApiInvalidPciSpace"           },
    { ApiInvalidBusIndex,           "ApiInvalidBusIndex"           },
    { ApiInsufficientResources,     "ApiInsufficientResources"     },
    { ApiWaitTimeout,               "ApiWaitTimeout"               },
    { ApiWaitCanceled,              "ApiWaitCanceled"              },
    { ApiDmaChannelUnavailable,     "ApiDmaChannelUnavailable"     },
    { ApiDmaChannelInvalid,         "ApiDmaChannelInvalid"         },
    { ApiDmaDone,                   "ApiDmaDone"                   },
    { ApiDmaPaused,                 "ApiDmaPaused"                 },
    { ApiDmaInProgress,             "ApiDmaInProgress"             },
    { ApiDmaCommandInvalid,         "ApiDmaCommandInvalid"         },
    { ApiDmaInvalidChannelPriority, "ApiDmaInvalidChannelPriority" },
    { ApiDmaSglPagesGetError,       "ApiDmaSglPagesGetError"       },
    { ApiDmaSglPagesLockError,      "ApiDmaSglPagesLockError"      },
    { ApiMuFifoEmpty,               "ApiMuFifoEmpty"               },
    { ApiMuFifoFull,                "ApiMuFifoFull"                },
    { ApiPowerDown,                 "ApiPowerDown"                 },
    { ApiHSNotSupported,            "ApiHSNotSupported"            },
    { ApiVPDNotSupported,           "ApiVPDNotSupported"           },
};

XIA_PLX_ERRORS XiaPlxErrors[] =
{
    { PLX_SUCCESS,                  "xia_plx success"              },
    { PLX_MEM,                      "xia_plx memory allocation"    },
    { PLX_UNKNOWN_HANDLE,           "xia_plx unknown handle"       },
};


/*
 * Close a previously opened PCI slot.
 */
XIA_EXPORT int XIA_API plx_close_slot(HANDLE h)
{
    PLX_STATUS status;
    PLX_DEVICE_OBJECT device_object;

    unsigned long idx;


    ASSERT(h);

    status = _plx_find_handle_index(h, &idx);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Unable to find HANDLE %p\n", h);
        return status;
    }

    device_object = V_MAP.device[idx];

    status = _plx_remove_slot_from_map(idx);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Error unmapping device (h = %p)\n", h);
        return status;
    }

    status = PlxPci_DeviceClose(&device_object);

    if (status != ApiSuccess) {
        _plx_log_DEBUG("Error closing device (h = %p)\n", h);
        _plx_print_more(status);
        return status;
    }

    return PLX_SUCCESS;
}


/*
 * Opens the PCI device located using the specified parameters
 *
 * As the specification for the DEVICE_LOCATION struct states, id, bus and
 * slot may be set to -1 to indicate that the value should not be used in the
 * search.
 */
XIA_EXPORT int XIA_API plx_open_slot(unsigned short id, byte_t bus, byte_t slot,
                                     HANDLE *h)
{
    PLX_STATUS status;

    PLX_DEVICE_KEY dev;
    PLX_DEVICE_OBJECT device_object;

    DWORD err;

    memset(&dev, PCI_FIELD_IGNORE, sizeof(PLX_DEVICE_KEY));

    dev.bus       = bus;
    dev.slot      = slot;

    /* Per the PLX SDK docs, we are not supposed to futz with the
     * members of the PLX_DEVICE_OBJECT structure, which makes sense
     * except for the assumptions they made about new instances of
     * PLX_DEVICE_OBJECT on the stack. In some build configurations, the
     * memory will be reused between subsequent calls as-is, leading to
     * the IsValidTag value being incorrectly set to "valid". We need to
     * override that here since we are promising to pass in
     * uninitialized device objects.
     */
    device_object.IsValidTag = 0;

    status = PlxPci_DeviceOpen(&dev, &device_object);

    if (status != ApiSuccess) {
        err = GetLastError();
        _plx_log_DEBUG("Error opening device (id = %hu, bus = %hu): status = %d\n"
                       "Last windows API error = %#lx\n",
                       id, (unsigned short)bus, status, err);

        _plx_print_more(status);
        return status;
    }

    status = _plx_add_slot_to_map(&device_object);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Error adding device %hu/%hu/%hu to virtual map\n",
                       (unsigned short)dev.bus, (unsigned short)dev.slot,
                       dev.DeviceId);
        return status;
    }
        
    *h = (HANDLE) device_object.hDevice;

    return PLX_SUCCESS;
}

/*
 * Translate the error code of a PlxApi error
 */
XIA_EXPORT void XIA_API plx_print_error(int errorcode, char *errorstring)
{
    int i;

    for (i = 0; i < N_ELEMS(ApiErrors); i++) {
        if ((int)ApiErrors[i].code == errorcode) {
            sprintf(errorstring, "Error caught in plxlib, %s", ApiErrors[i].text);
            return; 
        }
    }
    
    for (i = 0; i < N_ELEMS(XiaPlxErrors); i++) {
        if (XiaPlxErrors[i].code == errorcode) {
            sprintf(errorstring, "Error caught in plxlib, %s", XiaPlxErrors[i].text);
            return; 
        }
    }
        
    sprintf(errorstring, "UNKNOWN ERROR (%d) caught in plxlib", errorcode);  
}

/*
 * Sets the file that debugging messages will be written to.
 */
#ifdef PLXLIB_DEBUG
XIA_EXPORT void XIA_API plx_set_file_DEBUG(char *f)
{
    if (LOG_FILE) {
        fclose(LOG_FILE);
    }

    LOG_FILE = fopen(f, "w");

    if (!LOG_FILE) {
        LOG_FILE = stderr;
        return;
    }
}
#endif /* PLXLIB_DEBUG */


/*
 * Writes the specified message to the debug output stream
 */
static void _plx_log_DEBUG(char *msg, ...)
{
#ifdef PLXLIB_DEBUG

    va_list args;

    if (!LOG_FILE) {
        LOG_FILE = stderr;
    }


    va_start(args, msg);
    vfprintf(LOG_FILE, msg, args);
    va_end(args);

    fflush(LOG_FILE);

#else /* PLXLIB_DEBUG */
    UNUSED(msg);
#endif /* PLXLIB_DEBUG */
}


/*
 * Prints out more error information based on the strings in the
 * API from PLX Technology.
 */
static void _plx_print_more(PLX_STATUS errorcode)
{
    int i;

    for (i = 0; i < N_ELEMS(ApiErrors); i++) {
        if ((int)ApiErrors[i].code == errorcode) {
            _plx_log_DEBUG("Error caught in plxlib, %s\n", ApiErrors[i].text);
            return;
        }    
    }
    
    _plx_log_DEBUG("UNKNOWN ERROR (%d) caught in plxlib\n", errorcode);
}


/*
 * Adds a slot (specified by h) to the virtual map and sets up the
 * BAR.
 */
static int _plx_add_slot_to_map(PLX_DEVICE_OBJECT *device)
{
    PLX_STATUS status;

    V_MAP.n++;

    if (!V_MAP.addr) {

        ASSERT(V_MAP.n == 1);
        ASSERT(!V_MAP.device);

        V_MAP.addr = (PLX_UINT_PTR *)malloc(sizeof(PLX_UINT_PTR));

        if (!V_MAP.addr) {
            _plx_log_DEBUG("Unable to allocate %zu bytes for V_MAP.addr array\n",
                           sizeof(PLX_UINT_PTR));
            return PLX_MEM;
        }

        V_MAP.device = (PLX_DEVICE_OBJECT *)malloc(sizeof(PLX_DEVICE_OBJECT));

        if (!V_MAP.device) {
            _plx_log_DEBUG("Unable to allocate %zu bytes for V_MAP.device array\n",
                           sizeof(PLX_DEVICE_OBJECT));
            return PLX_MEM;
        }

        V_MAP.events = (PLX_NOTIFY_OBJECT *)malloc(sizeof(PLX_NOTIFY_OBJECT));

        if (!V_MAP.events) {
            _plx_log_DEBUG("Unable to allocate %zu bytes for V_MAP.events array\n",
                           sizeof(PLX_NOTIFY_OBJECT));
            return PLX_MEM;
        }

        V_MAP.intrs = (PLX_INTERRUPT *)malloc(sizeof(PLX_INTERRUPT));

        if (!V_MAP.intrs) {
            _plx_log_DEBUG("Unable to allocate %zu bytes for V_MAP.intrs array\n",
                           sizeof(PLX_INTERRUPT));
            return PLX_MEM;
        }

        V_MAP.registered = (boolean_t *)malloc(sizeof(boolean_t));

        if (!V_MAP.registered) {
            _plx_log_DEBUG("Unable to allocate %zu bytes for V_MAP.registered array\n",
                           sizeof(boolean_t));
            return PLX_MEM;
        }

    } else {
        status = _plx_resize_map();

        if (status != PLX_SUCCESS) {
            _plx_log_DEBUG("Error resizing V_MAP data.\n");
            return status;
        }
    }

    memcpy(&(V_MAP.device[V_MAP.n - 1]), device, sizeof(*device));


    status = PlxPci_PciBarMap(&(V_MAP.device[V_MAP.n - 1]), PLX_PCI_SPACE_0,
                              (VOID **)&(V_MAP.addr[V_MAP.n - 1]));

    if (status != ApiSuccess) {
        /* Undo memory allocation, etc. here */
        _plx_log_DEBUG("Error getting BAR for handle %p\n", &device);
        _plx_print_more(status);
        return status;
    }

    V_MAP.registered[V_MAP.n - 1] = FALSE_;

    return PLX_SUCCESS;
}


/*
 * Remove the slot (specified by h) from the system
 *
 * Includes unmapping the BAR
 */
static int _plx_remove_slot_from_map(unsigned long idx)
{
    PLX_STATUS status;

    unsigned long i;

    /* If the handle is registered as a notifier
     * then we need to unregister it to free up the event handle.
     */
    if (V_MAP.registered[idx]) {
        status = PlxPci_NotificationCancel(&(V_MAP.device[idx]),
                                           &(V_MAP.events[idx]));

        if (status != ApiSuccess) {
            _plx_log_DEBUG("Error unregistering notification of PCI DMA channel");
            _plx_print_more(status);
        }
    }

    status = PlxPci_PciBarUnmap(&(V_MAP.device[idx]), (VOID**)&(V_MAP.addr[idx]));

    if (status != ApiSuccess) {
        _plx_log_DEBUG("Error unmapping HANDLE %p\n", &(V_MAP.device[idx]));
        _plx_print_more(status);
        return status;
    }

    V_MAP.n--;

    /* Shrink the virtual map */
    if (V_MAP.n > 0) {
        for (i = idx; i < V_MAP.n; i++) {
            V_MAP.addr[i]  = V_MAP.addr[i + 1];
            V_MAP.device[i]  = V_MAP.device[i + 1];
            V_MAP.events[i]  = V_MAP.events[i + 1];
            V_MAP.intrs[i]  = V_MAP.intrs[i + 1];
        }

        status = _plx_resize_map();

        if (status != PLX_SUCCESS) {
            _plx_log_DEBUG("Error removing slot %d from V_MAP.\n", idx);
            return status;
        }

    } else {
        free(V_MAP.addr);
        free(V_MAP.device);
        free(V_MAP.events);
        free(V_MAP.intrs);
        free(V_MAP.registered);

        V_MAP.addr   = NULL;
        V_MAP.device = NULL;
        V_MAP.events = NULL;
        V_MAP.intrs  = NULL;
        V_MAP.registered  = NULL;
    }

    return PLX_SUCCESS;
}

/*
 * resize the memory allocation of the global V_MAP structure
 * to the size specified by V_MAP.n
 */
static int _plx_resize_map(void)
{

    PLX_UINT_PTR *new_addr = NULL;

    PLX_NOTIFY_OBJECT *new_events = NULL;

    PLX_INTERRUPT *new_intrs = NULL;

    PLX_DEVICE_OBJECT *new_device = NULL;

    boolean_t *new_registered = NULL;

    /* Need to grow the handle array to accomadate another module. We want to
    * do this as an atomic update. Either both memory allocations succeed or we
    * don't update the virtual map.
    */
    new_addr = (PLX_UINT_PTR *)realloc(V_MAP.addr,
                                       V_MAP.n * sizeof(PLX_UINT_PTR));

    if (!new_addr) {
        _plx_log_DEBUG("Unable to allocate %zu bytes for 'new_addr'\n",
                       V_MAP.n * sizeof(PLX_UINT_PTR));
        return PLX_MEM;
    }

    new_device = (PLX_DEVICE_OBJECT *)realloc(V_MAP.device, V_MAP.n *
                                              sizeof(PLX_DEVICE_OBJECT));

    if (!new_device) {
        _plx_log_DEBUG("Unable to allocate %zu bytes for 'new_device'\n",
                       V_MAP.n * sizeof(PLX_DEVICE_OBJECT));
        return PLX_MEM;
    }

    new_events = (PLX_NOTIFY_OBJECT *)realloc(V_MAP.events, V_MAP.n *
                                              sizeof(PLX_NOTIFY_OBJECT));

    if (!new_events) {
        _plx_log_DEBUG("Unable to allocate %zu bytes for 'new_events'\n",
                       V_MAP.n * sizeof(PLX_NOTIFY_OBJECT));
        return PLX_MEM;
    }

    new_intrs = (PLX_INTERRUPT *)realloc(V_MAP.intrs, V_MAP.n * sizeof(PLX_INTERRUPT));

    if (!new_intrs) {
        _plx_log_DEBUG("Unable to allocate %zu bytes for 'new_intrs'\n",
                       V_MAP.n * sizeof(PLX_INTERRUPT));
        return PLX_MEM;
    }

    new_registered = (boolean_t *)realloc(V_MAP.registered, V_MAP.n *
                                          sizeof(boolean_t));

    if (!new_registered) {
        _plx_log_DEBUG("Unable to allocate %zu bytes for 'new_registered'\n",
                       V_MAP.n * sizeof(boolean_t));
        return PLX_MEM;
    }

    V_MAP.addr       = new_addr;
    V_MAP.device     = new_device;
    V_MAP.events     = new_events;
    V_MAP.intrs      = new_intrs;
    V_MAP.registered = new_registered;

    return PLX_SUCCESS;
}

/*
 * Find the index of the specified HANDLE in the virtual map
 */
static int _plx_find_handle_index(HANDLE h, unsigned long *idx)
{
    unsigned long i;


    ASSERT(h);
    ASSERT(idx);

    for (i = 0; i < V_MAP.n; i++) {
        if (V_MAP.device[i].hDevice == (PLX_DRIVER_HANDLE) h) {
            *idx = i;
            return PLX_SUCCESS;
        }
    }

    _plx_log_DEBUG("Unable to locate HANDLE %p in the virtual map\n", h);

    return PLX_UNKNOWN_HANDLE;
}


/*
 * Read a long from the specified address
 */
XIA_EXPORT int XIA_API plx_read_long(HANDLE h, unsigned long addr,
                                     unsigned long *data)
{
    PLX_STATUS status;

    unsigned long idx;
    ptrdiff_t address = (ptrdiff_t)addr;

    ASSERT(h);
    ASSERT(data);


    status = _plx_find_handle_index(h, &idx);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Unable to find HANDLE %p\n", h);
        return status;
    }

    *data = *((unsigned long *)(V_MAP.addr[idx] + address));

#ifdef PLX_DEBUG_IO_TRACE
    _plx_log_DEBUG("[plx_read_long] addr = %#lx, data = %p\n", addr, data);
#endif /* PLX_DEBUG_IO_TRACE */

    return PLX_SUCCESS;
}


/*
 * Write a long to the specified address
 */
XIA_EXPORT int XIA_API plx_write_long(HANDLE h, unsigned long addr,
                                      unsigned long data)
{
    PLX_STATUS status;

    unsigned long idx;
    ptrdiff_t address = (ptrdiff_t)addr;

    ASSERT(h);


    status = _plx_find_handle_index(h, &idx);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Unable to find HANDLE %p\n", h);
        return status;
    }

#ifdef PLX_DEBUG_IO_TRACE
    _plx_log_DEBUG("[plx_write_long] addr = %#lx, data = %#lx\n", addr, data);
#endif /* PLX_DEBUG_IO_TRACE */

    ASSERT(V_MAP.addr[idx] != 0);
    *((unsigned long *)(V_MAP.addr[idx] + address)) = data;

    return PLX_SUCCESS;
}


/*
 * 'Burst' read a block of data
 */
XIA_EXPORT int XIA_API plx_read_block(HANDLE h, unsigned long addr,
                                      unsigned long len, unsigned long n_dead,
                                      unsigned long *data)
{
    unsigned long idx;

    unsigned long *local = NULL;

    PLX_STATUS status;
    PLX_STATUS ignored_status;

    PLX_DMA_PROP dma_prop;

    PLX_DMA_PARAMS dma_params;


    ASSERT(len > 0);
    ASSERT(data != NULL);

    status = _plx_find_handle_index(h, &idx);

    if (status != PLX_SUCCESS) {
        _plx_log_DEBUG("Unable to find HANDLE %p\n", h);
        return status;
    }

    memset(&dma_prop, 0, sizeof(PLX_DMA_PROP));

    dma_prop.ReadyInput       = 1;
    dma_prop.Burst            = 1;
    dma_prop.BurstInfinite    = 1;
    dma_prop.ConstAddrLocal   = 1;
    dma_prop.LocalBusWidth    = 2;  // 32-bit bus


    status = PlxPci_DmaChannelOpen(&(V_MAP.device[idx]), 0, &dma_prop);

    if (status != ApiSuccess) {
        _plx_log_DEBUG("Error opening PCI channel 0 for 'burst' read: HANDLE %p\n",
                       h);
        _plx_print_more(status);
        return status;
    }

    /* If the handle is not registered as a notifier, then we need to do it.
    * this only needs to be done once per handle.
    */
    if (!V_MAP.registered[idx]) {
        memset(&(V_MAP.intrs[idx]), 0, sizeof(PLX_INTERRUPT));

        // Setup to wait for DMA channel 0
        V_MAP.intrs[idx].DmaDone = 1;

        status = PlxPci_NotificationRegisterFor(&(V_MAP.device[idx]),
                                                &(V_MAP.intrs[idx]), &(V_MAP.events[idx]));

        if (status != ApiSuccess) {
            ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
            _plx_log_DEBUG("Error registering for notification of PCI DMA channel 0: "
                           "HANDLE %p\n", h);
            _plx_print_more(status);
            return status;
        }

        V_MAP.registered[idx] = TRUE_;
    }

    /* Write transfer address to XMAP_REG_TAR */
    status = plx_write_long(h, 0x50, addr);

    if (status != PLX_SUCCESS) {
        ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
        _plx_log_DEBUG("Error setting block address %#lx: HANDLE %p\n", addr, h);
        _plx_print_more(status);
        return status;
    }

    memset(&dma_params, 0, sizeof(PLX_DMA_PARAMS));

    /* We include the dead words in the transfer */
    local = (unsigned long *)malloc((len + n_dead) * sizeof(unsigned long));

    if (!local) {
        _plx_log_DEBUG("Error allocating %d bytes for 'local'.\n",
                       (len + 2) * sizeof(unsigned long));
        return PLX_MEM;
    }

    dma_params.UserVa            = (U64)local;
    dma_params.LocalAddr         = EXTERNAL_MEMORY_LOCAL_ADDR;
    dma_params.ByteCount         = (len + n_dead) * 4;
    dma_params.Direction 				 = PLX_DMA_LOC_TO_PCI;

    status = PlxPci_DmaTransferUserBuffer(&(V_MAP.device[idx]), 0, &dma_params, 0);

    if (status != ApiSuccess) {
        free(local);
        ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
        _plx_log_DEBUG("Error during 'burst' read: HANDLE %p\n", h);
        _plx_print_more(status);
        return status;
    }

    /* ASSERT((V_MAP.events[idx]).IsValidTag == PLX_TAG_VALID); */
    status = PlxPci_NotificationWait(&(V_MAP.device[idx]), &(V_MAP.events[idx]), 10000);

    if (status != ApiSuccess) {
        free(local);
        ignored_status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);
        _plx_log_DEBUG("Error waiting for 'burst' read to complete: HANDLE %p\n", h);
        _plx_print_more(status);
        return status;
    }

    memcpy(data, local + n_dead, len * sizeof(unsigned long));
    free(local);

    status = PlxPci_DmaChannelClose(&(V_MAP.device[idx]), 0);

    if (status != ApiSuccess) {
        _plx_log_DEBUG("Error closing PCI channel 0: HANDLE %p\n", h);
        _plx_print_more(status);
        return status;
    }

    return PLX_SUCCESS;
}


/*
 * Dump out the virtual map.
 */
#ifdef PLXLIB_DEBUG
XIA_EXPORT void XIA_API plx_dump_vmap_DEBUG(void)
{
    unsigned long i = 0;


    _plx_log_DEBUG("Starting virtual map dump.\n");

    for (i = 0; i < V_MAP.n; i++) {
        _plx_log_DEBUG("\t%lu: addr = %#lx, HANDLE = %p, REGISTERED = %u\n",
                       i, V_MAP.addr[i], V_MAP.device[i].hDevice,
                       (unsigned short)V_MAP.registered[i]);

        if (V_MAP.registered[i]) {
            _plx_log_DEBUG("\t   hEvent = %p, IsValidTag = %#lx\n",
                           (U64) (V_MAP.events[i]).hEvent,
                           (U32) (V_MAP.events[i]).IsValidTag
                          );
        }

        if (V_MAP.addr[i]) {
            _plx_log_DEBUG(
                "PCI BAR\n"
                "0: 0x%p\n"
                "1: 0x%p\n"
                "2: 0x%p\n"
                "3: 0x%p\n"
                "5: 0x%p\n",
                "4: 0x%p\n",
                (intptr_t)(V_MAP.addr[i]),
                (intptr_t)(V_MAP.addr[i] + (0x10)),
                (intptr_t)(V_MAP.addr[i] + (0x20)),
                (intptr_t)(V_MAP.addr[i] + (0x30)),
                (intptr_t)(V_MAP.addr[i] + (0x50)),
                (intptr_t)(V_MAP.addr[i] + (0x40))
            );
        }


    }

    _plx_log_DEBUG("Virtual map dump complete.\n");
}
#endif /* PLXLIB_DEBUG */
