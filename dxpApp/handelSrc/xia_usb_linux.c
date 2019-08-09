/*
 * Original author: Mark Rivers, University of Chicago
 *
 * Copyright (c) 2009-2012 XIA LLC
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


#include <stdio.h>
#include <stdlib.h>
#include <usb.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <string.h>

#include "Dlldefs.h"
#include "usblib.h"
#include "handel_errors.h"
#include "handel_log.h"
#include "xia_handel.h"

#include "xia_usb2.h"
#include "xia_usb2_errors.h"
#include "xia_usb2_private.h"

#define FALSE    0
#define TRUE    (!0)

#ifndef byte_t
#define byte_t        unsigned char
#endif

#ifndef bool
#define bool int
#endif

#define XIA_USB2_SMALL_READ_PACKET_SIZE 512

static bool is_xia_usb2_device(struct usb_device *q);
static int xia_usb2__send_setup_packet(HANDLE h, unsigned long addr,
                                       unsigned long n_bytes, byte_t rw_flag);
static struct usb_dev_handle        *xia_usb_handle = NULL;
static struct usb_device            *xia_usb_device = NULL;


XIA_EXPORT int XIA_API xia_usb_open(char *device, HANDLE *hDevice)
{
    int                           device_number;
    struct usb_bus                *p;
    struct usb_device             *q;
    int                           found = -1;
    int                           rv = 0;
    static int                    first = TRUE;

    if (xia_usb_handle != NULL) return 0;   /* if not first just return, leaving the old device open */

    device_number = device[strlen(device) - 1] - '0';

    /* Must be original XIA USB 1.0 card */
    if (xia_usb_handle == NULL) {
        if (first) {
            usb_init();
            usb_set_debug(0);
            usb_find_busses();
            usb_find_devices();
            first = FALSE;
        }

        p = usb_get_busses();
        while ((p != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
            q = p->devices;
            while ((q != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
                if ((q->descriptor.idVendor == 0x10e9) && (q->descriptor.idProduct == 0x0700)) {
                    found++;
                    if (found == device_number) {
                        xia_usb_device = q;
                        xia_usb_handle = usb_open(xia_usb_device);
                        if (xia_usb_handle == NULL) {
                            rv = -1;
                        } else {
                            rv = usb_set_configuration(xia_usb_handle,
                                                       xia_usb_device->config[0].bConfigurationValue);
                            if (rv == 0) {
                                rv = usb_claim_interface(xia_usb_handle, 0);
                                rv = usb_reset(xia_usb_handle);
                                sprintf(info_string, "Found USB 1.0 board");
                                xiaLogInfo("xia_usb_open", info_string);
                            }
                        }
                    }
                }
                q = q->next;
            }
            p = p->next;
        }
    }
    if ((xia_usb_handle == NULL) || (rv != 0)) {
        *hDevice = 0;
        if (rv == 0) rv = -99;
    } else {
        *hDevice = 1;
    }

    return rv;
}

static bool is_xia_usb2_device(struct usb_device *q)
{
    bool is_xia_vid = (q->descriptor.idVendor == 0x10E9);

    bool is_ketek_vid = (q->descriptor.idVendor == 0x20BD);

    bool is_saturn = (q->descriptor.idProduct == 0x0701);

    bool is_mercury = (q->descriptor.idProduct == 0x0702) ||
                 (q->descriptor.idProduct == 0x0703) ||
                 (q->descriptor.idProduct == 0x0780) ||
                 (q->descriptor.idProduct == 0x0781);

    bool is_microdxp = (q->descriptor.idProduct == 0x0B01) ||
                 (q->descriptor.idProduct == 0x0A01) ||
                 (q->descriptor.idProduct == 0x0C01);


    bool is_dpp2 = (q->descriptor.idProduct == 0x0020);

    return (is_xia_vid && (is_saturn || is_mercury || is_microdxp)) ||
            (is_ketek_vid && is_dpp2);
}



XIA_EXPORT int XIA_API xia_usb2_open(int device_number, HANDLE *hDevice)
{
    struct usb_bus               *p;
    struct usb_device            *q;
    struct usb_dev_handle        *h;
    int                           found = -1;
    int                           rv = 0;
    static int                    first = TRUE;

    sprintf(info_string, "Entry: device_number = %d, static handle = %p", device_number,
            xia_usb_handle);
    xiaLogInfo("xia_usb2_open", info_string);

    if (xia_usb_handle != NULL) return 0; /* if not first just return, leaving the old device open  */


    sprintf(info_string, "No handle");
    xiaLogInfo("xia_usb2_open", info_string);


    /* Must be new XIA USB 2.0 card */
    if (xia_usb_handle == NULL) {
        if (first) {
            usb_init();
            usb_set_debug(0);
            usb_find_busses();
            usb_find_devices();
            first = FALSE;
        }
        p = usb_get_busses();
        while ((p != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
            q = p->devices;
            while ((q != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
            if (is_xia_usb2_device(q)) {
                    found++;
                    if (found == device_number) {
                        sprintf(info_string, "Opening device %#x:%#x number %d",
                                q->descriptor.idVendor,
                                q->descriptor.idProduct,
                                found);
                        xiaLogInfo("xia_usb2_open", info_string);

                        h = usb_open(q);
                        if (h == NULL) {
                            sprintf(info_string, "usb_open failed");
                            xiaLogInfo("xia_usb2_open", info_string);
                            rv = -1;
                        } else {
                            sprintf(info_string, "setting configuration: %hu",
                                    q->config[0].bConfigurationValue);
                            xiaLogInfo("xia_usb2_open", info_string);

                            rv = usb_set_configuration(h, q->config[0].bConfigurationValue);

                            if (rv == 0) {
                                xiaLogInfo("xia_usb2_open", "claiming the interface");

                                rv = usb_claim_interface(h, 0);
                                if (rv != 0) {
                                    sprintf(info_string, "error claiming the interface: %d",
                                            rv);
                                    xiaLogWarning("xia_usb2_open", info_string);
                                }

                                rv = usb_reset(h);
                                if (rv != 0) {
                                    sprintf(info_string, "error resetting: %d",
                                            rv);
                                    xiaLogWarning("xia_usb2_open", info_string);
                                }

                                sprintf(info_string, "Found USB 2.0 board, product=0x%x",
                                        q->descriptor.idProduct);
                                xiaLogInfo("xia_usb2_open", info_string);

                                xia_usb_device = q;
                                xia_usb_handle = h;
                            } else {
                                sprintf(info_string, "usb_set_configuration failed: %d", rv);
                                xiaLogInfo("xia_usb2_open", info_string);

                                usb_close(h);
                            }
                        }
                    } else {
                        sprintf(info_string, "Skipping device %#x:%#x id = %d",
                                q->descriptor.idVendor,
                                q->descriptor.idProduct,
                                found);
                        xiaLogInfo("xia_usb2_open", info_string);
                    }
                }
                q = q->next;
            }
            p = p->next;
        }
    }

    if ((xia_usb_handle == NULL) || (rv != 0)) {
        *hDevice = 0;
        if (rv == 0) rv = -99;
    } else {
        *hDevice = 1;
    }

    return rv;
}

XIA_EXPORT int XIA_API xia_usb_close(HANDLE hDevice)
{
    int rv = 0;

    /* This does a reset of the USB device before closing it. This is
     * needed for USB2 on Linux or the next time that XIA software is
     * run it cannot open the device correctly.
     */
    if (hDevice && xia_usb_handle) {
        int rv_release, rv_close;

        /* This fails with error -22 (not defined in libusb.h?) in a basic
         * open/close test with a Mercury in Ubuntu 14.04. It passes if there
         * has been any intervening read operation.
         */
        rv_release = usb_release_interface(xia_usb_handle, 0);
        if (rv_release != 0) {
            sprintf(info_string, "Failed to release the interface, handle=%p, error=%d",
                    xia_usb_handle, rv_release);
            xiaLogWarning("xia_usb_close", info_string);
        }

        rv_close = usb_close(xia_usb_handle);
        if (rv_close != 0) {
            sprintf(info_string, "Failed to close, handle=%p, error=%d",
                    xia_usb_handle, rv_close);
            xiaLogWarning("xia_usb_close", info_string);
        }

        rv = rv_release | rv_close;

        xia_usb_handle = NULL;
        xia_usb_device = NULL;
    }

    return rv;
}

XIA_EXPORT int XIA_API xia_usb2_close(HANDLE hDevice)
{
    return(xia_usb_close(hDevice));
}

XIA_EXPORT int XIA_API xia_usb_read(long address, long nWords, char *device, unsigned short *buffer)
{
    int             n_bytes;
    HANDLE          hDevice;
    unsigned char   ctrlBuffer[64];        /* [CTRL_SIZE]; */
    unsigned char   lo_address, hi_address, lo_count, hi_count;
    int             rv = 0;

    rv = xia_usb_open(device, &hDevice);            /* Get handle to USB device */
    if (rv != 0) {
        sprintf(info_string, "Failed to open device %s", device);
        xiaLogError("xia_usb_read", info_string, XIA_MD);
        return 1;
    }


    n_bytes = (nWords * 2);

    hi_address = (unsigned char)(address >> 8);
    lo_address = (unsigned char)(address & 0x00ff);
    hi_count = (unsigned char)(n_bytes >> 8);
    lo_count = (unsigned char)(n_bytes & 0x00ff);

    memset(ctrlBuffer, 0, 64);
    ctrlBuffer[0] = lo_address;
    ctrlBuffer[1] = hi_address;
    ctrlBuffer[2] = lo_count;
    ctrlBuffer[3] = hi_count;
    ctrlBuffer[4] = (unsigned char)0x01;

    rv = usb_bulk_write(xia_usb_handle, OUT1 | USB_ENDPOINT_OUT, (char*)ctrlBuffer, CTRL_SIZE, 10000);
    if (rv != CTRL_SIZE) {
        sprintf(info_string, "usb_bulk_read returned %d should be %d", rv, CTRL_SIZE);
        xiaLogError("xia_usb_read", info_string, XIA_MD);
        return 14;
    }

    rv = usb_bulk_read(xia_usb_handle, IN2 | USB_ENDPOINT_IN, (char*)buffer, n_bytes, 10000);
    if (rv != n_bytes) {
        sprintf(info_string, "usb_bulk_read returned %d should be %d", rv, n_bytes);
        xiaLogError("xia_usb_read", info_string, XIA_MD);
        return 2;
    }

    return 0;

}

XIA_EXPORT int XIA_API xia_usb2_read(HANDLE h, unsigned long addr,
                                     unsigned long n_bytes,
                                     byte_t *buf)
{
    unsigned long rlen = 0;
    int status;

    status = xia_usb2_readn(h, addr, n_bytes, buf, &rlen);
    if (status != XIA_SUCCESS)
        return status;

    if (rlen != n_bytes) {
        sprintf(info_string, "USB bulk read returned %lu bytes, expected %lu",
                rlen, n_bytes);
        xiaLogError("xia_usb2_read", info_string, XIA_MD);
        return XIA_USB2_XFER;
    }

    return XIA_SUCCESS;
}

XIA_EXPORT int XIA_API xia_usb2_readn(HANDLE h, unsigned long addr,
                                      unsigned long n_bytes,
                                      byte_t *buf, unsigned long *n_bytes_read)
{
    int status = 0;
    int rlen = 0;

    if (xia_usb_handle == NULL) {
        return XIA_USB2_NULL_HANDLE;
    }

    if (n_bytes == 0) {
        return XIA_USB2_ZERO_BYTES;
    }

    if (buf == NULL) {
        return XIA_USB2_NULL_BUFFER;
    }

    /* Pad small reads to the max packet size for improved speed, as
     * determined in testing during initial USB2 development. Most
     * products will return the full max packet size, but short
     * microDXP commands may return only what data the command
     * specifies and not pad the response. At this level we check for errors
     * and return the actual number of bytes read by the driver. The caller
     * can validate that against the requested size.
     */
    if (n_bytes < XIA_USB2_SMALL_READ_PACKET_SIZE) {
        byte_t big_packet[XIA_USB2_SMALL_READ_PACKET_SIZE];

        /* Tag xia_usb_linux small packet buffers. */
        memset(big_packet, 0xbccb, XIA_USB2_SMALL_READ_PACKET_SIZE / 2);

        status = xia_usb2__send_setup_packet(0, addr,
                                             XIA_USB2_SMALL_READ_PACKET_SIZE,
                                             XIA_USB2_SETUP_FLAG_READ);

        if (status != XIA_USB2_SUCCESS) {
            return status;
        }

        rlen = usb_bulk_read(xia_usb_handle, XIA_USB2_READ_EP | USB_ENDPOINT_IN, (char*)big_packet,
                             XIA_USB2_SMALL_READ_PACKET_SIZE, 10000);
        if (rlen < 0) {
            sprintf(info_string, "usb_bulk_read error, driver reports: %d", rlen);
            xiaLogError("xia_usb2_read", info_string, XIA_MD);
            return XIA_USB2_XFER;
        }

        if (rlen != n_bytes && rlen != XIA_USB2_SMALL_READ_PACKET_SIZE) {
            sprintf(info_string, "usb_bulk_read %d/%lu", rlen, n_bytes);
            xiaLogError("xia_usb2_read", info_string, XIA_MD);
	}

        memcpy(buf, &big_packet, n_bytes);

        rlen = n_bytes;
    } else {
        status = xia_usb2__send_setup_packet(0, addr, n_bytes, XIA_USB2_SETUP_FLAG_READ);

        if (status != XIA_USB2_SUCCESS) {
            return status;
        }

        rlen = usb_bulk_read(xia_usb_handle, XIA_USB2_READ_EP | USB_ENDPOINT_IN, (char*)buf,
                             n_bytes, 10000);
        if (rlen < 0) {
            sprintf(info_string, "usb_bulk_read error, driver reports: %d", rlen);
            xiaLogError("xia_usb2_read", info_string, XIA_MD);
            return XIA_USB2_XFER;
        }
    }

    *n_bytes_read = rlen;

    return XIA_SUCCESS;
}

XIA_EXPORT int XIA_API xia_usb_write(long address, long nWords, char *device, unsigned short *buffer)
{
    int                     n_bytes;
    HANDLE                  hDevice;
    int                     rv = 0;
    unsigned char           ctrlBuffer[64];            /* [CTRL_SIZE]; */
    unsigned char           lo_address, hi_address, lo_count, hi_count;

    rv = xia_usb_open(device, &hDevice);            /* Get handle to USB device */
    if (rv != 0) {
        sprintf(info_string, "Failed to open %s", device);
        xiaLogError("xia_usb_write", info_string, XIA_MD);
        return 1;
    }


    n_bytes = (nWords * 2);

    hi_address = (unsigned char)(address >> 8);
    lo_address = (unsigned char)(address & 0x00ff);
    hi_count = (unsigned char)(n_bytes >> 8);
    lo_count = (unsigned char)(n_bytes & 0x00ff);

    memset(ctrlBuffer, 0, 64);
    ctrlBuffer[0] = lo_address;
    ctrlBuffer[1] = hi_address;
    ctrlBuffer[2] = lo_count;
    ctrlBuffer[3] = hi_count;
    ctrlBuffer[4] = (unsigned char)0x00;

    rv = usb_bulk_write(xia_usb_handle, OUT1 | USB_ENDPOINT_OUT, (char*)ctrlBuffer, CTRL_SIZE, 10000);
    if (rv != CTRL_SIZE) {
        sprintf(info_string, "usb_bulk_write returned %d should be %d", rv, CTRL_SIZE);
        xiaLogError("xia_usb_write", info_string, XIA_MD);
        return 14;
    }

    rv = usb_bulk_write(xia_usb_handle, OUT2 | USB_ENDPOINT_OUT, (char*)buffer, n_bytes, 10000);
    if (rv != n_bytes) {
        sprintf(info_string, "usb_bulk_write returned %d should be %d", rv, n_bytes);
        xiaLogError("xia_usb_write", info_string, XIA_MD);
        return 15;
    }
    return 0;
}



XIA_EXPORT int XIA_API xia_usb2_write(HANDLE h, unsigned long addr,
                                      unsigned long n_bytes,
                                      byte_t *buf)
{
    int     status;

    if (xia_usb_handle == NULL) {
        return XIA_USB2_NULL_HANDLE;
    }

    if (n_bytes == 0) {
        return XIA_USB2_ZERO_BYTES;
    }

    if (buf == NULL) {
        return XIA_USB2_NULL_BUFFER;
    }

    status = xia_usb2__send_setup_packet(0, addr, n_bytes, XIA_USB2_SETUP_FLAG_WRITE);

    if (status != XIA_USB2_SUCCESS) {
        return status;
    }

    status = usb_bulk_write(xia_usb_handle, XIA_USB2_WRITE_EP | USB_ENDPOINT_OUT, (char*)buf,
                            n_bytes, 10000);

    if (status != n_bytes) {
        sprintf(info_string, "usb_bulk_write returned %d should be %lu", status, n_bytes);
        xiaLogError("xia_usb2_write", info_string, XIA_MD);
        return XIA_USB2_XFER;
    }

    return XIA_USB2_SUCCESS;
}

/*
 * Sends an XIA-specific setup packet to the "setup" endpoint. This
 * is the first stage of our two-part process for transferring data to
 * and from the board.
 */
static int xia_usb2__send_setup_packet(HANDLE h, unsigned long addr,
                                       unsigned long n_bytes, byte_t rw_flag)
{
    int             status;

    byte_t          pkt[XIA_USB2_SETUP_PACKET_SIZE];

    pkt[0] = (byte_t)(addr & 0xFF);
    pkt[1] = (byte_t)((addr >> 8) & 0xFF);
    pkt[2] = (byte_t)(n_bytes & 0xFF);
    pkt[3] = (byte_t)((n_bytes >> 8) & 0xFF);
    pkt[4] = (byte_t)((n_bytes >> 16) & 0xFF);
    pkt[5] = (byte_t)((n_bytes >> 24) & 0xFF);
    pkt[6] = rw_flag;
    pkt[7] = (byte_t)((addr >> 16) & 0xFF);
    pkt[8] = (byte_t)((addr >> 24) & 0xFF);

    status = usb_bulk_write(xia_usb_handle, XIA_USB2_SETUP_EP | USB_ENDPOINT_OUT, (char*)pkt, XIA_USB2_SETUP_PACKET_SIZE, 10000);
    if (status != XIA_USB2_SETUP_PACKET_SIZE) {
        sprintf(info_string, "usb_bulk_write returned %d should be %d", status, XIA_USB2_SETUP_PACKET_SIZE);
        xiaLogError("xia_usb2__send_setup_packet", info_string, XIA_MD);
        return XIA_USB2_XFER;
    }

    return XIA_USB2_SUCCESS;
}
