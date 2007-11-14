/*************
 *  usblib_linux.c  
 *
 *  JEW: Shamelessly adapted from work by Don Wharton
 * 
 * Copyright XIA 2002
 **************/

/*************
 *
 *  P. L. Charles Fischer (4pi Analysis)
 *
 *  Make these call work in Linux (2.6 kernel)
 *  13-Apr-2005
 *
 **************/

/* These #pragmas remove warnings generated
 * by bugs in the Microsoft headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <usb.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <string.h>
#include "Dlldefs.h"
#include "usblib.h"
#include "xia_usb2.h"
#include "xia_usb2_errors.h"
#include "xia_usb2_private.h"

#define FALSE	0
#define TRUE	(!0)
#define false	0
#define true	(!0)

#ifndef byte_t
#define byte_t		unsigned char
#endif

#ifndef bool
#define bool int
#endif

static int xia_usb2__send_setup_packet(HANDLE h, unsigned short addr,
                                       unsigned long n_bytes, byte_t rw_flag);
static struct usb_dev_handle        *xia_usb_handle = NULL;
static struct usb_device            *xia_usb_device = NULL;
static bool                         xia_usb_1_0 = FALSE;
static bool                         xia_usb_2_0 = FALSE;


/*---------------------------------------------------------------------------------------*/
XIA_EXPORT int XIA_API xia_usb_open(char *device, HANDLE *hDevice)
{
	int							device_number;
	struct usb_bus				*p;
	struct usb_device			*q;
	int							found = -1;
	int							rv = 0;
	static int					first = TRUE;

printf("xia_usb_open, xia_usb_handle=%p\n", xia_usb_handle);
	if (xia_usb_handle != NULL) return 0;				/* if not first just return, leaving the old device open	*/

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
printf("xia_usb_open, usb_get_busses returns %p\n", p);
		while ((p != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
			q = p->devices;
printf("xia_usb_open, p->devices=%p\n", q);
			while ((q != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
printf("xia_usb_open, q->descriptor.idVendor=%x, q->descriptor.idProduct=%x\n", q->descriptor.idVendor, q->descriptor.idProduct);
				if ((q->descriptor.idVendor == 0x10e9) && (q->descriptor.idProduct == 0x0700)) {
					found++;
					if (found == device_number) {
						xia_usb_device = q;
						xia_usb_handle = usb_open(xia_usb_device);
printf("xia_usb_open, usb_open returns=%p\n", xia_usb_handle);
						if (xia_usb_handle == NULL) {
							rv = -1;
						} else {
printf("xia_usb_open, calling usb_set_configuration returns with value=%x\n", xia_usb_device->config[0].bConfigurationValue);
							rv = usb_set_configuration(xia_usb_handle, xia_usb_device->config[0].bConfigurationValue);
printf("xia_usb_open, usb_set_configuration returns=%x\n", rv);
							if (rv == 0) {
								rv = usb_claim_interface(xia_usb_handle, 0);
								xia_usb_1_0 = true;
								xia_usb_2_0 = false;
								fprintf(stderr, "xia_usb:found USB 1.0 board\n"); fflush(stderr);
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

printf("xia_usb_open, device=%s, rv=%d, *hDevice=%d\n", device, rv, *hDevice);
	return rv;
}
/*---------------------------------------------------------------------------------------*/
XIA_EXPORT int XIA_API xia_usb2_open(int device_number, HANDLE *hDevice)
{
	struct usb_bus				*p;
	struct usb_device			*q;
	int							found = -1;
	int							rv = 0;
	static int					first = TRUE;

printf("xia_usb2_open, xia_usb_handle=%p\n", xia_usb_handle);
	if (xia_usb_handle != NULL) return 0;				/* if not first just return, leaving the old device open	*/

printf("xia_usb2_open, device_number=%d\n", device_number);

	/*
	// Must be new XIA USB 2.0 card
	*/
	if (xia_usb_handle == NULL) {
		if (first) {
			usb_init();
			usb_set_debug(0);
			usb_find_busses();
			usb_find_devices();
			first = FALSE;
		}
		p = usb_get_busses();
printf("xia_usb2_open, usb_get_busses returns %p\n", p);
		while ((p != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
			q = p->devices;
printf("xia_usb2_open, p->devices=%p\n", q);
			while ((q != NULL) && (xia_usb_handle == NULL) && (rv == 0)) {
printf("xia_usb2_open, q->descriptor.idVendor=%x, q->descriptor.idProduct=%x\n", q->descriptor.idVendor, q->descriptor.idProduct);
				if ((q->descriptor.idVendor == 0x10e9) && (q->descriptor.idProduct == 0x0701)) {
					found++;
					if (found == device_number) {
						xia_usb_device = q;
						xia_usb_handle = usb_open(xia_usb_device);
printf("xia_usb2_open, usb_open returns=%p\n", xia_usb_handle);
						if (xia_usb_handle == NULL) {
							rv = -1;
						} else {
printf("xia_usb2_open, calling usb_set_configuration returns with value=%x\n", xia_usb_device->config[0].bConfigurationValue);
							rv = usb_set_configuration(xia_usb_handle, xia_usb_device->config[0].bConfigurationValue);
printf("xia_usb2_open, usb_set_configuration returns=%x\n", rv);
							if (rv == 0) {
								rv = usb_claim_interface(xia_usb_handle, 0);
								xia_usb_1_0 = false;
								xia_usb_2_0 = true;
								fprintf(stderr, "xia_usb:found USB 2.0 board\n"); fflush(stderr);
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

printf("xia_usb2_open, device_number=%d, rv=%d, *hDevice=%d\n", device_number, rv, *hDevice);
	return rv;
}

/*---------------------------------------------------------------------------------------*/
XIA_EXPORT int XIA_API xia_usb_close(HANDLE hDevice)
{
	return 0;											/* At this time never close		*/

	if (hDevice) {
		usb_close(xia_usb_handle);
		xia_usb_handle = NULL;
		xia_usb_device = NULL;
	}

	return 0;
}

/*---------------------------------------------------------------------------------------*/
XIA_EXPORT int XIA_API xia_usb_read(long address, long nWords, char *device, unsigned short *buffer)
{
	int             status = 0;
	int             byte_count;
	HANDLE					hDevice;
	int						rv = 0;
	
	rv = xia_usb_open(device, &hDevice);			/* Get handle to USB device */
	if (rv != 0) {
		fprintf(stderr, "xia_usb_read -1: device: %s\n", device); fflush(stderr);
		return 1;
	}

	if (xia_usb_1_0) {
		unsigned char			ctrlBuffer[64];		/* [CTRL_SIZE]; */
		unsigned char			lo_address, hi_address, lo_count, hi_count;

		byte_count = (nWords * 2);
	  
		hi_address = (unsigned char)(address >> 8);
		lo_address = (unsigned char)(address & 0x00ff);
		hi_count = (unsigned char)(byte_count >> 8);
		lo_count = (unsigned char)(byte_count & 0x00ff);
	  
		memset(ctrlBuffer, 0, 64);
		ctrlBuffer[0] = lo_address;
		ctrlBuffer[1] = hi_address;
		ctrlBuffer[2] = lo_count;
		ctrlBuffer[3] = hi_count;
		ctrlBuffer[4] = (unsigned char)0x01;

printf("xia_usb_read, calling usb_bulk_write\n");		
		rv = usb_bulk_write(xia_usb_handle, OUT1 | USB_ENDPOINT_OUT, (char*)ctrlBuffer, CTRL_SIZE, 10000);
printf("xia_usb_read, usb_bulk_write returns %d, should be %d\n", rv, CTRL_SIZE);		
		if (rv != CTRL_SIZE) {
			fprintf(stderr, "xia_usb_read -14: device: %s, Return value: %d\n", device, rv); fflush(stderr);
			xia_usb_close(hDevice);
			return 14;
		}

		rv = usb_bulk_read(xia_usb_handle, IN2 | USB_ENDPOINT_IN, (char*)buffer, byte_count, 10000);
printf("xia_usb_read, usb_bulk_read returns %d, should be %d\n", rv, byte_count);		
		if (rv != byte_count) {
			fprintf(stderr, "xia_usb_read -2: device: %s\n", device); fflush(stderr);
			xia_usb_close(hDevice);
			return 2;
		}

	} else {
		int					i;
		unsigned short 		*buf = NULL;
		byte_t 				*byte_buf = NULL;

		if (xia_usb_handle == NULL) {
			return XIA_USB2_NULL_HANDLE;
		}

		if (nWords == 0) {
			return XIA_USB2_ZERO_BYTES;
		}

		if (buffer == NULL) {
			return XIA_USB2_NULL_BUFFER;
		}

		byte_count = (nWords * 2);
		buf = (unsigned short *)buffer;
		byte_buf = malloc(byte_count);
		
		if (byte_count < XIA_USB2_SMALL_READ_PACKET_SIZE) {
printf("xia_usb_read, calling xia_usb2__send_setup_packet\n");		
			status = xia_usb2__send_setup_packet(0, address,
		                                     XIA_USB2_SMALL_READ_PACKET_SIZE,
		                                     XIA_USB2_SETUP_FLAG_READ);

printf("xia_usb_read, xia_usb2__send_setup_packet returns %d\n", status);		
			if (status != XIA_USB2_SUCCESS) {
				return status;
			}

			byte_t 		big_packet[XIA_USB2_SMALL_READ_PACKET_SIZE];
			  
			status = usb_bulk_read(xia_usb_handle, XIA_USB2_READ_EP | USB_ENDPOINT_IN, (char*)big_packet, XIA_USB2_SMALL_READ_PACKET_SIZE, 10000);
printf("xia_usb_read, usb_bulk_read returns %d, should be %d\n", status, XIA_USB2_SMALL_READ_PACKET_SIZE);		
  			memcpy(byte_buf, big_packet, byte_count);
			status = (status != XIA_USB2_SMALL_READ_PACKET_SIZE);
			/*
			if (byte_count == 2) {
				unsigned short temp_short;

				temp_short = (unsigned short)(byte_buf[0 * 2] | (byte_buf[(0 * 2) + 1] << 8));
				printf("Ok usb_rd-c %d, %8X, %d, %8X\n", (int)status, (int)address, (int)byte_count, (int)temp_short);
			}
			*/

			/*//status = xia_usb2__small_read_xfer(xia_usb_handle, (DWORD)byte_count, byte_buf); */
		} else {
			status = xia_usb2__send_setup_packet(0, address, byte_count, XIA_USB2_SETUP_FLAG_READ);

			if (status != XIA_USB2_SUCCESS) {
		  		return status;
			}

			/*printf("Ok usb_rd-d %d, %8X, %d\n", (int)status, (int)address, (int)byte_count);*/

			status = usb_bulk_read(xia_usb_handle, XIA_USB2_READ_EP | USB_ENDPOINT_IN, (char*)byte_buf, byte_count, 10000);
			/* //status = xia_usb2__xfer(xia_usb_handle, XIA_USB2_READ_EP, (DWORD)byte_count, byte_buf);*/
			status = (status != byte_count);
		}

		for (i = 0; i < nWords; i++) {
			buf[i] = (unsigned short)(byte_buf[i * 2] | (byte_buf[(i * 2) + 1] << 8));
      	}
      	
		free(byte_buf);
		


		/*fflush(stdout);*/	
	}

	xia_usb_close(hDevice);

	return(status);
}

/*---------------------------------------------------------------------------------------*/
XIA_EXPORT int XIA_API xia_usb_write(long address, long nWords, char *device, unsigned short *buffer)
{
	int 					status = 0;
	int						byte_count;
	HANDLE					hDevice;
	int						rv = 0;
	
	rv = xia_usb_open(device, &hDevice);			/* Get handle to USB device */
	if (rv != 0) {
		fprintf(stderr, "xia_usb_write -1: device: %s\n", device); fflush(stderr);
		return 1;
	}

	if (xia_usb_1_0) {
		unsigned char			ctrlBuffer[64];			/* [CTRL_SIZE]; */
		unsigned char			lo_address, hi_address, lo_count, hi_count;
		int						byte_count;

		byte_count = (nWords * 2);
	  
		hi_address = (unsigned char)(address >> 8);
		lo_address = (unsigned char)(address & 0x00ff);
		hi_count = (unsigned char)(byte_count >> 8);
		lo_count = (unsigned char)(byte_count & 0x00ff);
	  
		memset(ctrlBuffer, 0, 64);
		ctrlBuffer[0] = lo_address;
		ctrlBuffer[1] = hi_address;
		ctrlBuffer[2] = lo_count;
		ctrlBuffer[3] = hi_count;
		ctrlBuffer[4] = (unsigned char)0x00;

		rv = usb_bulk_write(xia_usb_handle, OUT1 | USB_ENDPOINT_OUT, (char*)ctrlBuffer, CTRL_SIZE, 10000);
		if (rv != CTRL_SIZE) {
			fprintf(stderr, "xia_usb_write -14: device: %s\n", device); fflush(stderr);
			xia_usb_close(hDevice);
			return 14;
		}

		rv = usb_bulk_write(xia_usb_handle, OUT2 | USB_ENDPOINT_OUT, (char*)buffer, byte_count, 10000);
		if (rv != byte_count) {
			fprintf(stderr, "xia_usb_write -15: device: %s\n", device); fflush(stderr);
			xia_usb_close(hDevice);
			return 15;
		}

	} else {
		int					i;
		unsigned short 		*buf = NULL;
		byte_t 				*byte_buf = NULL;

		status = xia_usb_open(device, &hDevice);			/* Get handle to USB device */
		if (status != 0) {
			fprintf(stderr, "xia_usb_write -1: device: %s\n", device); fflush(stderr);
			return 1;
		}

		if (xia_usb_handle == NULL) {
			return XIA_USB2_NULL_HANDLE;
		}

		if (nWords == 0) {
			return XIA_USB2_ZERO_BYTES;
		}

		if (buffer == NULL) {
			return XIA_USB2_NULL_BUFFER;
		}

		byte_count = (nWords * 2);
		buf = (unsigned short *)buffer;
		byte_buf = malloc(byte_count);
		

		status = xia_usb2__send_setup_packet(0, address, byte_count, XIA_USB2_SETUP_FLAG_WRITE);

		if (status != XIA_USB2_SUCCESS) {
			return status;
		}

		for (i = 0; i < nWords; i++) {
        	byte_buf[i * 2]       = (byte_t)(buf[i] & 0xFF);
        	byte_buf[(i * 2) + 1] = (byte_t)((buf[i] >> 8) & 0xFF);
		}

  		status = usb_bulk_write(xia_usb_handle, XIA_USB2_WRITE_EP | USB_ENDPOINT_OUT, (char*)byte_buf, byte_count, 10000);
		/*status = xia_usb2__xfer(h, XIA_USB2_WRITE_EP, (DWORD)n_bytes, byte_buf);*/
 		/*printf("Ok usb_wr-a %d, %8X, %d\n", (int)status, (int)address, (int)byte_count);*/
     	
		free(byte_buf);

		status = (status != byte_count);

	}

	xia_usb_close(hDevice);

	return(status);
}

/*---------------------------------------------------------------------------------------*/
/**
 * @brief Sends an XIA-specific setup packet to the "setup" endpoint. This
 * is the first stage of our two-part process for transferring data to
 * and from the board.
 */
static int xia_usb2__send_setup_packet(HANDLE h, unsigned short addr,
                                       unsigned long n_bytes, byte_t rw_flag)
{
  int 			status;

  byte_t 		pkt[XIA_USB2_SETUP_PACKET_SIZE];

  /*
  ASSERT(n_bytes != 0);
  ASSERT(rw_flag < 2);
  */

  pkt[0] = (byte_t)(addr & 0xFF);
  pkt[1] = (byte_t)((addr >> 8) & 0xFF);
  pkt[2] = (byte_t)(n_bytes & 0xFF);
  pkt[3] = (byte_t)((n_bytes >> 8) & 0xFF);
  pkt[4] = (byte_t)((n_bytes >> 16) & 0xFF);
  pkt[5] = (byte_t)((n_bytes >> 24) & 0xFF);
  pkt[6] = rw_flag;

  /*status = usb_bulk_write(xia_usb_handle, XIA_USB2_SETUP_EP | USB_ENDPOINT_OUT, (char*)pkt, XIA_USB2_SETUP_PACKET_SIZE, 10000);*/
  status = usb_bulk_write(xia_usb_handle, XIA_USB2_SETUP_EP | USB_ENDPOINT_OUT, (char*)pkt, XIA_USB2_SETUP_PACKET_SIZE, 10000);

  /*printf("Ok usb_st-a %d, %8X, %d\n", (int)status, (int)(XIA_USB2_SETUP_EP | USB_ENDPOINT_OUT), (int)XIA_USB2_SETUP_PACKET_SIZE);*/
		
  /*status = xia_usb2__xfer(h, XIA_USB2_SETUP_EP, XIA_USB2_SETUP_PACKET_SIZE, pkt);*/
  status = (status != XIA_USB2_SETUP_PACKET_SIZE);

  return status;
}

